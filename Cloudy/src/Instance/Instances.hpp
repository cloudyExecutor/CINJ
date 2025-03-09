#pragma once
#include <cstdint>
#include <vector>
#include <string>

std::string Decompress(const std::string_view compressed);

namespace rbx {
	class instances {
	public:
		uintptr_t Address;
		std::string GetScriptBytecode();
		void SetBytecode(std::vector<char> bytes, int bytecode_size);
		void GetBytecode(std::vector<char>& bytecode, size_t& bytecode_size);
		void BypassModule();
		void spoof(rbx::instances gyat);
		std::vector<rbx::instances> GetChildren();
		std::vector<std::uintptr_t> GetChildrenAddresses(std::uintptr_t address);
		std::uintptr_t FindFirstChildAddress(std::string name);
		rbx::instances WaitForChild(std::string InstanceName, int timeout = 5);
		rbx::instances FindFirstChild(std::string InstanceName);
		rbx::instances FindFirstChildOfClass(std::string ClassName);
		rbx::instances ObjectValue();
		void SetBoolValue(bool Boolean);
		std::string Name();
		std::string ClassName();
	};
}