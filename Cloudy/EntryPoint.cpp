#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "src/utils/memory/Driver.hpp"
#include "src/instance/instances.hpp"
#include <tlhelp32.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <shellapi.h>
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "ws2_32.lib")

#include "dependencies/json/nlohmann/json.hpp"
using json = nlohmann::json;

#include "dependencies/json/httplib.h"
#include "src/Offsets/Offsets.hpp"
#include "dependencies/base64/base64.hpp"
#include "src/Utils/ByteCompiler.hpp"
#include "src/utils/bridge.hpp"
#include "EntryPoint.hpp"

DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD selectedProcessId = 0;
    SIZE_T maxMemoryUsage = 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnap, &pe)) {
            do {
                if (std::wstring(pe.szExeFile) == processName) {
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
                    if (hProcess) {
                        PROCESS_MEMORY_COUNTERS pmc;
                        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                            if (pmc.WorkingSetSize > maxMemoryUsage) {
                                maxMemoryUsage = pmc.WorkingSetSize;
                                selectedProcessId = pe.th32ProcessID;
                            }
                        }
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    else {
        std::wcerr << L"Failed to create process snapshot." << std::endl;
    }

    return selectedProcessId;
}

HWND ReturnedHwnd = NULL;
BOOL CALLBACK WindowsProcess(HWND hwnd, LPARAM lParam) {
    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == (DWORD)lParam) {
        ReturnedHwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND GetHandleFromProcessId(DWORD processId) {
    EnumWindows(WindowsProcess, processId);
    return ReturnedHwnd;
}

std::uint64_t datamodel;
DWORD m;
std::uint64_t GetDatamodel()
{
    HANDLE main_thread = OpenThread(THREAD_ALL_ACCESS, NULL, m);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_INTEGER;

    while (2) {
        if (main_thread)
        {
            GetThreadContext(main_thread, &ctx);
        }

        auto Datamodel = static_cast<rbx::instances> (ctx.Rcx);

        if (Datamodel.ClassName() == "DataModel") {
            datamodel = ctx.Rcx;
            return datamodel;
        }
    }

    return 0;
}

void CreateException(const std::string& numbercode, const std::string& error) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::filesystem::path mainDir;
    std::filesystem::path CrashDir;

    mainDir = std::filesystem::current_path();
    CrashDir = mainDir / "Logs";

    if (!std::filesystem::exists(CrashDir)) {
        std::filesystem::create_directory(CrashDir);
    }
    else if (!std::filesystem::is_directory(CrashDir)) {
        std::filesystem::remove(CrashDir);
        std::filesystem::create_directory(CrashDir);
    }
    auto t = std::time(nullptr);
    struct tm timeinfo;
    time_t rawtime;
    time(&rawtime);

    localtime_s(&timeinfo, &rawtime);
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S");
    std::string timestamp = oss.str();
    std::filesystem::path filePath = CrashDir / (timestamp + ".txt");

    std::string content = "Exception Number Code: " + numbercode + "\n"
        "[Stack Frame 0] Inside null: " + numbercode + "; " + error + ": " + numbercode + "\n";

    ShowErrorr(error);

    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << content;
        outFile.close();
    }
    else {
        return;
    }

}




std::string GetLuaScript() {
    HRSRC resourceHandle = FindResourceW(NULL, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (resourceHandle == NULL)
    {
        CreateException("0", "Error getting init script");
        return "";
    }

    HGLOBAL loadedResource = LoadResource(NULL, resourceHandle);
    if (loadedResource == NULL)
    {
        CreateException("0", "Error getting init script resource");
        return "";
    }

    DWORD size = SizeofResource(NULL, resourceHandle);
    void* data = LockResource(loadedResource);

    return std::string(static_cast<char*>(data), size);
}

HANDLE handle;
const std::string SetByteString = "fghe45y6erd";
int main(int argc, char* argv[])
{

        char mainPath[MAX_PATH];
        GetModuleFileNameA(NULL, mainPath, MAX_PATH);

        std::filesystem::path mainDir;
        std::filesystem::path logs;

        std::string luaContent = GetLuaScript();

        const std::string PatchScriptSource = "return {AvatarPrompts = function() end}";

        DWORD pid = GetProcessIdByName(L"RobloxPlayerBeta.exe");
        if (pid == 0 || !pid)
        {
            ShowWarning("Roblox not open!");
            return 1;
        }

        auto RobloxHWND = GetHandleFromProcessId(pid);

        auto thread_id = GetWindowThreadProcessId(RobloxHWND, 0);
        m = thread_id;

        handle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);

        if (!handle) {
            CreateException("22", "Couldn't open Roblox process");
            return 55;
        }

        Driver->Inject(pid);

       

        auto dt = static_cast<rbx::instances>(GetDatamodel());

        rbx::instances CorePackages = dt.FindFirstChild("CorePackages");

        rbx::instances CoreGui = dt.FindFirstChild("CoreGui");

        rbx::instances RobloxGui = CoreGui.FindFirstChild("RobloxGui");

        rbx::instances Modules = RobloxGui.FindFirstChild("Modules");

        rbx::instances AvatarEditorPrompts = Modules.FindFirstChild("AvatarEditorPrompts");

        std::thread(StartBridge).detach();

        if (AvatarEditorPrompts.Address < 1000)
        {
            CreateException("9", "Coulnd't get AvatarEditorPrompts!");
            return 9;
        }

        if (dt.Name() == "LuaApp" or dt.Name() == "App")
        {

            AvatarEditorPrompts.BypassModule();

            size_t NewBytecodeSize;

            AvatarEditorPrompts.SetBytecode(Compress_Bytecode_Jest(Compile("coroutine.wrap(function(...)" + luaContent + "\nend)();" + PatchScriptSource), NewBytecodeSize), NewBytecodeSize);
        
        }
        else if (dt.Name() == "Ugc" or dt.Name() == "Game")
        {

            rbx::instances Workspace = CorePackages.FindFirstChild("Workspace"); //

            rbx::instances Packages = Workspace.FindFirstChild("Packages"); //

            rbx::instances JestGlobals = Packages.FindFirstChild("JestGlobals");

            rbx::instances PlayerList = Modules.FindFirstChild("PlayerList");

            rbx::instances PlayerListManager = PlayerList.FindFirstChild("PlayerListManager");
            size_t OldBytecodeSize;
            std::vector<char> OldBytecode;
            size_t NewBytecodeSize;

            JestGlobals.BypassModule();

            JestGlobals.GetBytecode(OldBytecode, OldBytecodeSize);

            Driver->Write<uintptr_t>(PlayerListManager.Address + 0x8, JestGlobals.Address);

            JestGlobals.SetBytecode(Compress_Bytecode_Jest(Compile("coroutine.wrap(function(...)" + luaContent + "\nend)();while wait(9e9) do wait(9e9);end"), NewBytecodeSize), NewBytecodeSize);

            AvatarEditorPrompts.SetBytecode(Compress_Bytecode_Jest(Compile("coroutine.wrap(function(...)" + luaContent + "\nend)();" + PatchScriptSource), NewBytecodeSize), NewBytecodeSize);

            SendMessageW(RobloxHWND, WM_CLOSE, NULL, NULL);

            JestGlobals.SetBytecode(OldBytecode, OldBytecodeSize);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            Driver->Write<uintptr_t>(PlayerListManager.Address + 0x8, PlayerListManager.Address);

        }



        HANDLE hPipe = CreateNamedPipe(
            TEXT("\\\\.\\pipe\\()()()()()()()()()()(7)"),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            0,
            0,
            0,
            nullptr
        );


        std::thread(StartExecution).detach();

        ShowSuccess("SUCCESS", "Injected sucessfully, enjoy.");

        while (true) {
        }
}
