#include "Execution.hpp"
#include "../EntryPoint.hpp"
#include "../src/Instance/Instances.hpp"
#include "../src/Offsets/Offsets.hpp"
#include "../src/utils/ByteCompiler.hpp"
inline rbx::instances DatamodelAddress;

std::uint64_t FetchedDatamodel;

void ExecuteScript(std::string text)
{
	if (!FetchedDatamodel)
		FetchedDatamodel = GetDatamodel();

	auto game = static_cast<rbx::instances>(FetchedDatamodel);

	rbx::instances CoreGui = game.FindFirstChild("CoreGui");

	rbx::instances RobloxGui = CoreGui.FindFirstChild("RobloxGui");


	auto Folder = RobloxGui.WaitForChild("Container");

	auto Holder = Folder.WaitForChild("ScriptHolder").ObjectValue();

	size_t Sized;

	auto Compressed = Compress_Bytecode_Jest(Compile(std::string("return function()\n" + text + "\nend")), Sized);

	Holder.SetBytecode(Compressed, Sized);

	Holder.BypassModule();
}
