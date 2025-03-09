#include <regex>
#include <filesystem>
#include <fstream>
//#define CPPHTTPLIB_OPENSSL_SUPPORT
//#include "../../dependencies/Libssl/http.h"
#include "../../dependencies/server/httplib.h"
using namespace httplib;
#include "../../dependencies/json/nlohmann/json.hpp"
using json = nlohmann::json;
#include <Windows.h>
#include <thread>
#include <format>
#include <filesystem>
#include <iostream>
#include "../instance/instances.hpp"
#include "../../dependencies/json/httplib.h"
#include "../../dependencies/base64/base64.hpp"
#include "../Execution.hpp"
#include "../../EntryPoint.hpp"
#include "../../src/Instance/Instances.hpp"
#include "../utils/memory/Driver.hpp"
#include "../../src/Offsets/Offsets.hpp"
#include "../../src/utils/ByteCompiler.hpp"
#include "../../dependencies/lz4/lz4.h"
#include "../../dependencies/lz4/lz4hc.h"
#include "../../dependencies/ixwebsocket/IXWebSocket.h"
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#include <comdef.h>


bool loadstring(const std::string& source) {
	ExecuteScript(source);
	return true;
}


rbx::instances GetObjectValuePtr(const std::string_view objectval_name)
{
	auto DataModel = static_cast<rbx::instances>(GetDatamodel());

	auto CoreGui = DataModel.FindFirstChild("CoreGui");

	auto RobloxGui = CoreGui.FindFirstChild("RobloxGui");

	auto Folder = RobloxGui.FindFirstChild("Container");

	if (Folder.Address < 1000)
	{
		CreateException("89", "Couldn't fetch the Main Container");
	}

	auto objectValContainer = Folder.FindFirstChild("Pointers");

	if (objectValContainer.Address < 1000)
	{
		CreateException("91", "Couldn't fetch the Pointers");
	}

	std::uintptr_t objectValue = objectValContainer.FindFirstChildAddress(std::string(objectval_name));

	return rbx::instances(Driver->read<std::uintptr_t>(objectValue + Offsets::ValueBase));
}

std::string GetBytecode(const std::string_view objectval_name) {
	rbx::instances scriptPtr = GetObjectValuePtr(objectval_name);

	return scriptPtr.GetScriptBytecode();
}

void UnlockModule(const std::string_view objectval_name) {
	rbx::instances scriptPtr = GetObjectValuePtr(objectval_name);

	return scriptPtr.BypassModule();
}


static bool withinDirectory(const std::filesystem::path& base, const std::filesystem::path& path) {
	std::filesystem::path baseAbs = std::filesystem::absolute(base).lexically_normal();
	std::filesystem::path resolvedPath = baseAbs;

	for (const std::filesystem::path& part : path) {
		if (part == "..") {
			if (resolvedPath.has_parent_path()) {
				resolvedPath = resolvedPath.parent_path();
			}
		}
		else if (part != ".") {
			resolvedPath /= part;
		}
	}

	resolvedPath = resolvedPath.lexically_normal();

	return std::mismatch(baseAbs.begin(), baseAbs.end(), resolvedPath.begin()).first == baseAbs.end();
}



static std::filesystem::path GetHandlePath(HANDLE processHandle) {
	char buffer[MAX_PATH];
	DWORD bufferSize = sizeof(buffer);

	if (QueryFullProcessImageNameA(processHandle, 0, buffer, &bufferSize)) {
		return std::filesystem::path(buffer);
	}

	return {};
}

bool console_active = false;
std::mutex rconsoleMtx;
static void activate_console() {
	std::lock_guard<std::mutex> lock(rconsoleMtx);
	if (console_active)
		return;

	console_active = true;

	AllocConsole();
	SetConsoleTitleA("Synapse Injector Console");
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	freopen_s(&pCout, "CONIN$", "r", stdin);
}
std::string WaitForInput() {
	std::string input;
	std::getline(std::cin, input);
	return input;
}


std::mutex websocketMutex;
std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> websocketConnections;

static void StartServer(Response& res, const json& body) {
	const std::string_view Type = body["c"];

	if (Type == "GetCustomAsset") {
		if (!body.contains("p")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}

		const std::string path = body["p"];
		if (!withinDirectory(std::filesystem::current_path(), path)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}

		const std::filesystem::path sourcePath = std::filesystem::current_path() / path;

		if (!std::filesystem::is_regular_file(sourcePath)) {
			res.status = 400;
			res.set_content(R"({"error":"Given path is not a normal file"})", "application/json");
			return;
		}

		auto ClientDir = GetHandlePath(handle).parent_path();

		std::filesystem::path contentDir = ClientDir / "content";
		if (!std::filesystem::is_directory(contentDir)) {
			res.status = 400;
			res.set_content(R"({"error":"directory 'content' does not exist or is not a directory"})", "application/json");
			return;
		}
		std::filesystem::path destinationPath = contentDir / sourcePath.filename();
		std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::overwrite_existing);
		res.status = 200;
		res.set_content("rbxasset://" + destinationPath.filename().string(), "text/plain");
		return;


		res.status = 400;
		res.set_content(R"({"error":"Client with the given PID was not found"})", "application/json");
		return;
	}

	if (Type == "btc") { // get script bytecode
		if (!body.contains("cn") /*container name*/) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}
		const std::string containerName = body["cn"];


		const std::string bytecode = GetBytecode(containerName);
		res.status = 200;
		res.set_content(bytecode, "text/plain");
	}

	if (Type == "Request") { 
		if (!body.contains("l") || !body.contains("m")  || !body.contains("b") || !body.contains("h")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}

		const std::string url = body["l"];
		const std::string method = body["m"];
		const std::string rBody = base64::from_base64(body["b"]);
		const json headersJ = body["h"];

		const std::regex urlR(R"(^(http[s]?:\/\/)?([^\/]+)(\/.*)?$)");
		std::smatch urlM;
		std::string host;
		std::string path;
		if (std::regex_match(url, urlM, urlR)) {
			host = urlM[2];
			path = urlM[3].str();
		}
		else {
			res.status = 400;
			res.set_content(R"({"error":"Invalid URL"})", "application/json");
			return;
		}

		Client client(host.c_str());
		client.set_follow_location(true);

		Headers headers;
		for (auto it = headersJ.begin(); it != headersJ.end(); ++it) {
			headers.insert({ it.key(), it.value() });
		}

		Result proxiedRes;
		if (method == "GET") {
			proxiedRes = client.Get(path, headers);
		}
		else if (method == "POST") {
			proxiedRes = client.Post(path, headers, rBody, "application/json");
		}
		else if (method == "PUT") {
			proxiedRes = client.Put(path, headers, rBody, "application/json");
		}
		else if (method == "DELETE") {
			proxiedRes = client.Delete(path, headers, rBody, "application/json");
		}
		else if (method == "PATCH") {
			proxiedRes = client.Patch(path, headers, rBody, "application/json");
		}
		else {
			res.status = 400;
			res.set_content(R"({"error":"Unsupported HTTP method"})", "application/json");
			return;
		}

		if (proxiedRes) {
			json responseJ;
			responseJ["c"] = proxiedRes->status;
			responseJ["r"] = proxiedRes->reason;
			responseJ["v"] = proxiedRes->version;

			json rHeadersJ;
			for (const auto& header : proxiedRes->headers) {
				rHeadersJ[header.first] = header.second;
			}
			responseJ["h"] = rHeadersJ;

			const auto contentType = proxiedRes->get_header_value("Content-Type");
			if (contentType.find("application/json") == std::string::npos &&
				contentType.find("text/") == std::string::npos) { // convert binary files to base 64
				responseJ["b"] = base64::to_base64(proxiedRes->body);
				responseJ["b64"] = true;
			}
			else {
				responseJ["b"] = proxiedRes->body;
			}

			res.status = 200;
			res.set_content(responseJ.dump(), "application/json");
		}
		else {
			res.status = 200;
			res.set_content("x", "text/plain");
		}
		return;
	}
	if (Type == "hw") {
		HW_PROFILE_INFO hwProfileInfo;

		if (GetCurrentHwProfile(&hwProfileInfo)) {
			std::string hwid = std::string(hwProfileInfo.szHwProfileGuid,
				hwProfileInfo.szHwProfileGuid + wcslen(hwProfileInfo.szHwProfileGuid));
			res.status = 200;
			res.set_content(hwid, "text/plain");
		}
		else {
			res.status = 500;
			res.set_content("Failed to retrieve HWID", "text/plain");
		}
		return;
	}
	if (Type == "ReadFile") {
		if (!body.contains("p")) { 
			res.status = 400;
			res.set_content(R"({"error":"Missing argument path"})", "application/json");
			return;
		}

		const std::string bodypath = body["p"];

		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to access restricted or invalid path"})", "application/json");
			return;
		}

		std::filesystem::path fsPath(bodypath);
		if (!std::filesystem::exists(bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Directory does not exist"})", "application/json");
			return;
		}
		/*
		std::filesystem::path dirPath = fsPath.parent_path();
		if (!std::filesystem::exists(dirPath)) {
			res.status = 400;
			res.set_content(R"({"error":"Parent directory does not exist"})", "application/json");
			return;
		}
		if (!std::filesystem::is_directory(dirPath)) {
			res.status = 400;
			res.set_content(R"({"error":"Parent path is not a directory"})", "application/json");
			return;
		}*/

		if (!std::filesystem::exists(fsPath)) {
			res.status = 404;
			res.set_content(R"({"error":"Given file path not found"})", "application/json");
			return;
		}

		if (!std::filesystem::is_regular_file(fsPath)) {
			res.status = 400;
			res.set_content(R"({"error":"Given file path is not a valid file"})", "application/json");
			return;
		}

		std::ifstream file(fsPath);
		if (!file) {
			res.status = 500;
			res.set_content(R"({"error":"Couldn't open file!"})", "application/json");
			return;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		res.status = 200;
		res.set_content(buffer.str(), "text/plain");
		return;
	}


	if (Type == "ListFiles") {
		if (!body.contains("Path")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing path argument"})", "application/json");
			return;
		}

		const std::string bodypath = body["Path"];
		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}
		if (!std::filesystem::is_directory(bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"The path provided doesn't exists"})", "application/json");
			return;
		}

		json responseJ;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(bodypath)) {
			responseJ.push_back(entry.path().string());
		}

		res.status = 200;
		res.set_content(responseJ.dump(), "application/json");
		return;
	}

	if (Type == "CheckFile") {
		if (!body.contains("p")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}

		const std::string bodypath = body["p"];
		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}

		res.status = 200;
		if (std::filesystem::is_directory(bodypath)) {
			res.set_content("FileDir", "text/plain");
			return;
		}
		if (std::filesystem::is_regular_file(bodypath)) {
			res.set_content("NormalFile", "text/plain");
			return;
		}
		return;
	}

	if (Type == "MakeFolder") {
		if (!body.contains("p")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}

		const std::string bodypath = body["p"];
		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}

		if (std::filesystem::is_directory(bodypath)) {
			return;
		}

		if (std::filesystem::is_regular_file(bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"A file with the same name already exists"})", "application/json");
			return;
		}

		try {
			if (std::filesystem::create_directory(bodypath)) {
				res.status = 201;
			}
			else {
				res.status = 500;
				res.set_content(R"({"error":"Couldn't create a new directory"})", "application/json");
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			res.status = 500;
			res.set_content("{\"error\":\"Couldn't create a new directory: " + std::string(e.what()) + "\"})", "application/json");
		}
		return;
	}

	if (Type == "DeleteFolder") {
		if (!body.contains("p")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing path argument"})", "application/json");
			return;
		}

		const std::string bodypath = body["p"];
		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}

		if (!std::filesystem::is_directory(bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"The file path provided doesn't exists"})", "application/json");
			return;
		}

		try {
			if (std::filesystem::remove_all(bodypath)) {
				res.status = 200;
			}
			else {
				res.status = 500;
				res.set_content(R"({"error":"Couldn't delete directory"})", "application/json");
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			res.status = 500;
			res.set_content("{\"error\":\"Couldn't delete directory: " + std::string(e.what()) + "\"})", "application/json");
		}

		return;
	}

	if (Type == "DeleteFile") { 
		if (!body.contains("p") /*path*/) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}

		const std::string bodypath = body["p"];
		if (!withinDirectory(std::filesystem::current_path(), bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Attempt to escape main directory"})", "application/json");
			return;
		}

		if (!std::filesystem::is_regular_file(bodypath)) {
			res.status = 400;
			res.set_content(R"({"error":"Given file path is not an allowed file"})", "application/json");
			return;
		}

		try {
			if (std::filesystem::remove(bodypath)) {
				res.status = 200;
			}
			else {
				res.status = 500;
				res.set_content(R"({"error":"Couldn't delete file"})", "application/json");
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			res.status = 500;
			res.set_content("{\"error\":\"Could not delete file: " + std::string(e.what()) + "\"})", "application/json");
		}
		return;
	}

	if (Type == "RealConsole") { 
		if (!body.contains("t")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing important arguments"})", "application/json");
			return;
		}
		const std::string type = body["t"];

		if (type == "cls") { // clear
			if (console_active) {
				system("cls");
			}
			return;
		}

		if (type == "crt") { // create
			activate_console();
			return;
		}

		if (type == "dst") { // destroy
			if (console_active) {
				console_active = false;
				FreeConsole();
			}
			return;
		}

		if (!body.contains("ct")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing important arguments"})", "application/json");
			return;
		}

		if (type == "prt") { // print
			activate_console();
			std::string text = body["ct"];
			text = base64::from_base64(text);
			std::cout << "[RCONSOLE PRINT]: " << text << std::endl;
		}

		if (type == "warn") {
			activate_console();
			std::string text = body["ct"];
			text = base64::from_base64(text);
			std::cout << "[RCONSOLE WARN]: " << text << std::endl;
		}

		if (type == "input") {
			activate_console();
			std::string userInput = WaitForInput();
			res.status = 200;
			res.set_content(R"({"message":")" + userInput + R"("})", "application/json");
		}

		if (type == "err") {
			activate_console();
			std::string text = body["ct"];
			text = base64::from_base64(text);
			std::cout << "[RCONSOLE ERROR]: " << text << std::endl;
		}

		if (type == "ttl") {
			const std::string title = body["ct"];
			SetConsoleTitleA(title.c_str());
		}

		res.status = 200;
		return;
	}
	if (Type == "AutoExecute") {
		std::filesystem::path mainDir;
		std::filesystem::path AXDir;

		mainDir = std::filesystem::current_path().parent_path();
		AXDir = mainDir / "Autoexec";

		if (!std::filesystem::exists(AXDir)) {
			std::filesystem::create_directory(AXDir);
		}
		else if (!std::filesystem::is_directory(AXDir)) {
			std::filesystem::remove(AXDir);
			std::filesystem::create_directory(AXDir);
		}

		std::string content;

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(AXDir)) {
			if (!entry.is_regular_file())
				continue;

			std::ifstream file(entry.path());
			if (file.is_open()) {
				std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				content += "task.spawn(function(...)" + file_contents + "\nend)\n";
				file.close();
			}
		}

		res.status = 200;
		res.set_content(content, "text/plain");
		return;
	}

};


void StartBridge()
{
	Server Bridge;

	std::filesystem::path mainDir;
	std::filesystem::path workspaceDir;

	char mainPath[MAX_PATH];
	GetModuleFileNameA(NULL, mainPath, MAX_PATH);

	mainDir = std::filesystem::path(mainPath).parent_path();
	workspaceDir = mainDir / "Workspace";
	if (!std::filesystem::exists(workspaceDir)) {
		std::filesystem::create_directory(workspaceDir);
	}
	if (!std::filesystem::is_directory(workspaceDir)) {
		std::filesystem::remove(workspaceDir);
		std::filesystem::create_directory(workspaceDir);
	}

	std::filesystem::current_path(workspaceDir);

	Bridge.Post("/setclipboard", [](const Request& req, Response& res) {
		if (req.body.empty() /*content*/) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}
		const std::string content = req.body;

		if (!OpenClipboard(nullptr)) {
			res.status = 400;
			res.set_content(R"({"error":"Couldn't open clipboard"})", "application/json");
			return;
		}

		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, content.size() + 1);

		if (!hMem) {
			res.status = 400;
			res.set_content(R"({"error":"Failed to empty clipboard to set a content to clipboard})", "application/json");
			return;
		}

		char* pMem = static_cast<char*>(GlobalLock(hMem));
		if (!pMem) {
			GlobalFree(hMem);
			CloseClipboard();

			res.status = 400;
			res.set_content(R"({"error":"Failed to set clipboard"})", "application/json");
			return;
		}

		std::copy(content.begin(), content.end(), pMem);
		pMem[content.size()] = '\0';
		GlobalUnlock(hMem);
		SetClipboardData(CF_TEXT, hMem);
		CloseClipboard();

		res.status = 200;
		return;
	});
	Bridge.Post("/compilable", [](const Request& req, Response& res) {
		if (req.body.empty() /*source*/) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}
		const std::string source = req.body;
		bool returnBytecode = false;

		if (req.has_param("btc"))
			returnBytecode = true;

		res.status = 200;
		res.set_content(IsCompilable(source, returnBytecode), "text/plain");
	});

	// Loadstring
	Bridge.Post("/loadstring", [](const Request& req, Response& res) {
		if (req.body.empty()) {
			res.status = 400;
			res.set_content(R"({"error":"Missing arguments"})", "application/json");
			return;
		}
		const std::string source = req.body;

		bool success = loadstring(source);

		if (success) {
			res.status = 200;
			res.set_content(R"({"success":"Loadstring executed"})", "application/json");
		}
		else {
			res.status = 400;
			res.set_content(R"({"error":"An error occurred with loadstring"})", "application/json");
		}
	});

	Bridge.Post("/writefile", [](const Request& req, Response& res) {
		if (req.body.empty()) {
			res.status = 400;
			res.set_content(R"({"error":"Missing content"})", "application/json");
			return;
		}

		if (!req.has_param("p")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing path"})", "application/json");
			return;
		}

		const std::string path = req.get_param_value("p");
		const std::string_view content = req.body;

		if (!withinDirectory(std::filesystem::current_path(), path)) {
			res.status = 400;
			res.set_content(R"({"error":"Path escapes the main directory"})", "application/json");
			return;
		}


		if (std::filesystem::is_directory(path)) {
			res.status = 400;
			res.set_content(R"({"error":"The path provided is a directory"})", "application/json");
			return;
		}

		try {
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (!file.is_open()) {
				throw std::ios_base::failure("Failed to open file");
			}
			file.write(content.data(), content.size());
			file.close();
		}
		catch (const std::exception& e) {
			res.status = 400;
			res.set_content(R"({"error":"Couldn't write file successfully"})", "application/json");
			return;
		}

		res.status = 200;
		res.set_content(R"({"message":"File written successfully"})", "application/json");
	});

	Bridge.Post("/send", [](const Request& req, Response& res) {
		json body;
		try {
			body = json::parse(req.body);
		}
		catch (json::parse_error&) {
			res.status = 400;
			res.set_content(R"({"error":"IReveived an invalid JSON"})", "application/json");
			return;
		}

		if (body.empty()) {
			res.status = 400;
			res.set_content(R"({"error":"Received an empty JSON"})", "application/json");
			return;
		}

		if (!body.contains("c")) {
			res.status = 400;
			res.set_content(R"({"error":"Missing (content) argument"})", "application/json");
		}
		StartServer(res, body);
	});


	Bridge.listen("localhost", 2000);
}

void StartExecution()
{
	Server Bridge;

	Bridge.Post("/Exec43/Script1/Arcre", [](const Request& req, Response& res) {
		const std::string source = req.body;
		ExecuteScript(source);
	});
	Bridge.listen("localhost", 5673);

}