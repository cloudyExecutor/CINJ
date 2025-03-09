#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <optional>
#include <thread>
#include "Instances.hpp"
#include "../../EntryPoint.hpp"
#include "../utils/memory/Driver.hpp"
#include "..\Offsets\Offsets.hpp"
#include "../../dependencies/Zstd/zstd.h"
#include "../../dependencies/xxhash.h"


std::string Decompress(const std::string_view compressed) {
    const uint8_t bytecodeSignature[4] = { 'R', 'S', 'B', '1' };
    const int bytecodeHashMultiplier = 41;
    const int bytecodeHashSeed = 42;
    std::vector<uint8_t> compressedData(compressed.begin(), compressed.end());

    // Check if compressedData has enough bytes to proceed
    if (compressedData.size() < 8) {
        std::cout << "Decompress Error: Not enough data!" << std::endl;
        return "";  // Return an empty string to indicate failure but prevent crash
    }

    std::vector<uint8_t> headerBuffer(4);

    // Safely extract header, checking if compressedData is large enough
    for (size_t i = 0; i < 4; ++i) {
        if (i >= compressedData.size()) {
            std::cout << "Decompress Error: Out of bounds on headerBuffer!" << std::endl;
            return "";  // Return empty string to prevent further processing
        }

        headerBuffer[i] = compressedData[i] ^ bytecodeSignature[i];
        headerBuffer[i] = (headerBuffer[i] - i * bytecodeHashMultiplier) % 256;
    }

    // Decrypt the rest of the compressedData with the header values
    for (size_t i = 4; i < compressedData.size(); ++i) {
        compressedData[i] ^= (headerBuffer[i % 4] + i * bytecodeHashMultiplier) % 256;
    }

    uint32_t hashValue = 0;
    for (size_t i = 0; i < 4; ++i) {
        hashValue |= headerBuffer[i] << (i * 8);
    }

    uint32_t rehash = XXH32(compressedData.data(), compressedData.size(), bytecodeHashSeed);

    uint32_t decompressedSize = 0;
    for (size_t i = 4; i < 8; ++i) {
        if (i >= compressedData.size()) {
            std::cout << "Decompress Error: Insufficient data for decompressed size!" << std::endl;
            return "";  // Return empty string to indicate failure but prevent crash
        }

        decompressedSize |= compressedData[i] << ((i - 4) * 8);
    }

    // If decompressed size is too small or invalid, return an empty string
    if (decompressedSize < 1 || decompressedSize > compressedData.size()) {
        std::cout << "Decompress Error: Invalid decompressed size!" << std::endl;
        return "";  // Return empty string to prevent decompression attempt
    }

    // Remove header from compressed data
    compressedData = std::vector<uint8_t>(compressedData.begin() + 8, compressedData.end());

    // Handle decompression with Zstd
    std::vector<uint8_t> decompressed(decompressedSize);
    size_t const actualDecompressedSize = ZSTD_decompress(decompressed.data(), decompressedSize, compressedData.data(), compressedData.size());

    // Check decompressed size is valid
    if (ZSTD_isError(actualDecompressedSize)) {
        std::cout << "Decompress Error: ZSTD decompression failed!" << std::endl;
        return "";  // Return empty string but allow execution to continue
    }

    decompressed.resize(actualDecompressedSize);
    return std::string(decompressed.begin(), decompressed.end());
}




std::string rbx::instances::GetScriptBytecode() {
    DWORD64 bytecode_pointer = Offsets::ModuleScriptBytecode;
    std::uintptr_t embeddedPtr = Driver->read<std::uintptr_t>(this->Address + bytecode_pointer);
    std::uintptr_t bytecodePtr = Driver->read<std::uintptr_t>(embeddedPtr + 0x10);
    std::uint64_t bytecodeSize = Driver->read<std::uint64_t>(embeddedPtr + 0x20);
    std::string bytecodeBuffer;
    bytecodeBuffer.resize(bytecodeSize);
    MEMORY_BASIC_INFORMATION bi;
    if (VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(bytecodePtr), &bi, sizeof(bi)) == 0) {
        return "";
    }
    static auto NtUnlockVirtualMemory = reinterpret_cast<_NtUnlockVirtualMemory*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtUnlockVirtualMemory"));
    static auto NtReadVirtualMemory = reinterpret_cast<_NtReadVirtualMemory*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtReadVirtualMemory"));
    if (NtReadVirtualMemory(handle, reinterpret_cast<LPCVOID>(bytecodePtr), bytecodeBuffer.data(), (ULONG)bytecodeSize, nullptr) != 0) {
        return "";
    }
    PVOID baddr = bi.AllocationBase;
    SIZE_T size = bi.RegionSize;
    NtUnlockVirtualMemory(handle, &baddr, &size, 1);
    return Decompress(bytecodeBuffer);
}

std::string ToRbxString(std::uintptr_t Address)
{
	const auto size = Driver->read<size_t>(Address + 0x10);

	if (size >= 16)
		Address = Driver->read<std::uintptr_t>(Address);
	std::string str;

	BYTE code = 0;
	for (std::int32_t i = 0; code = Driver->read<std::uint8_t>(Address + i); i++)
		str.push_back(code);

	return str;
}

std::vector<std::uintptr_t> rbx::instances::GetChildrenAddresses(std::uintptr_t address) {
    std::vector<std::uintptr_t> children;
    {
        std::uintptr_t childrenPtr = Driver->read<std::uintptr_t>(address + Offsets::Children);
        std::uintptr_t childrenStart = Driver->read<std::uintptr_t>(childrenPtr);
        std::uintptr_t childrenEnd = Driver->read<std::uintptr_t>(childrenPtr + 0x8) + 1;

        for (std::uintptr_t childAddress = childrenStart; childAddress < childrenEnd; childAddress += 0x10) {
            std::uintptr_t childPtr = Driver->read<std::uintptr_t>(childAddress);
            if (childPtr != 0)
                children.push_back(childPtr);
        }
    }
    return children;
}
std::uintptr_t rbx::instances::FindFirstChildAddress(std::string name) {
    std::vector<std::uintptr_t> childAddresses = GetChildrenAddresses(this->Address);
    for (std::uintptr_t address : childAddresses) {
        if (ToRbxString(Driver->read<std::uintptr_t>(address + Offsets::Name)) == name)
            return address;
    }
    return 0;
}

void rbx::instances::SetBytecode(std::vector<char> bytes, int bytecode_size)
{
    auto old_bytecode_ptr = Driver->read<long long>(this->Address + Offsets::ModuleScriptBytecode);

    auto protected_str_ptr = Driver->AllocateVirtualMemory(bytecode_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    Driver->WriteMemory(protected_str_ptr, bytes.data(), bytes.size());
    Driver->Write<unsigned long long>(old_bytecode_ptr + 0x10, protected_str_ptr);
    Driver->Write<unsigned long>(old_bytecode_ptr + 0x20, bytecode_size);
}

template <typename T>
void ReadMemory(DWORD64 Address, T* buffer, SIZE_T size = 0) {
    if (size == 0) {
        size = sizeof(T);
    }
    ReadProcessMemory(Driver->GetProcessHandle(), (LPCVOID)Address, buffer, size, NULL);
}
std::vector<char> ReadBytes(DWORD64 Address, SIZE_T size = 500) {
    std::vector<char> buffer(size, 0);
    ReadMemory(Address, buffer.data(), size);
    return buffer;
}

void rbx::instances::GetBytecode(std::vector<char>& bytecode, size_t& bytecode_size) {
    DWORD64 bytecode_pointer = Driver->read<long long>(this->Address + Offsets::ModuleScriptBytecode);
    if (bytecode_pointer > 1000) {
        bytecode = ReadBytes(bytecode_pointer + 0x10);
        ReadMemory(bytecode_pointer + 0x20, &bytecode_size);
    };
}

void rbx::instances::BypassModule() {
    Driver->Write(this->Address + Offsets::ModuleFlags, 0x100000000);
    Driver->Write(this->Address + Offsets::IsCoreScript, 0x1);
}

std::vector<rbx::instances> rbx::instances::GetChildren()
{
    std::vector<rbx::instances> container;

    auto start = Driver->read<std::uint64_t>(this->Address + Offsets::Children);

    auto end = Driver->read<std::uint64_t>(start + Offsets::ChildrenEnd);

    for (auto instances = Driver->read<std::uint64_t>(start); instances != end; instances += 16) {
        rbx::instances aee = Driver->read<rbx::instances>(instances);
        container.emplace_back(aee);
    }

    return container;
}

rbx::instances rbx::instances::FindFirstChild(std::string InstanceName)
{
    for (auto& object : this->GetChildren())
    {
        if (object.Name() == InstanceName)
        {
            return static_cast<rbx::instances>(object);
            break;
        }
    }
}

rbx::instances rbx::instances::FindFirstChildOfClass(std::string ClassName)
{
    for (auto& object : this->GetChildren())
    {
        if (object.ClassName() == ClassName)
        {
            return static_cast<rbx::instances>(object);
            break;
        }
    }
}


rbx::instances rbx::instances::ObjectValue()
{
    return Driver->read<rbx::instances>(this->Address + Offsets::ValueBase);
}

void rbx::instances::SetBoolValue(bool Boolean) {
    Driver->Write<bool>(this->Address + Offsets::ValueBase, Boolean);
}

rbx::instances rbx::instances::WaitForChild(std::string InstanceName, int timeout) {
    timeout *= 10;  
    for (int times = 0; times < timeout; ++times) {
        auto child_list = Driver->read<DWORD64>(this->Address + Offsets::Children);
        if (!child_list) continue;

        auto child_top = Driver->read<DWORD64>(child_list);
        auto child_end = Driver->read<DWORD64>(child_list + 0x8);

        for (DWORD64 child_addy = child_top; child_addy < child_end; child_addy += 0x10) {
            rbx::instances child = Driver->read<rbx::instances>(child_addy);

            if (child.Address > 1000 && child.Name() == InstanceName)
                return child;
        }
        Sleep(100); 
    }

    return rbx::instances{}; 
}

std::string rbx::instances::ClassName()
{
    auto ClassDescriptor = Driver->read<uint64_t>(this->Address + Offsets::ClassDescriptor);
    auto ClassName = Driver->read<uint64_t>(ClassDescriptor + Offsets::ClassName);

    if (ClassName)
        return ToRbxString(ClassName);

    return "None{1}";
}

void rbx::instances::spoof(rbx::instances gyat)
{
    Driver->Write<unsigned long long>(this->Address + 0x8, gyat.Address);
}

std::string rbx::instances::Name()
{
    const auto RealName = Driver->read<std::uint64_t>(this->Address + Offsets::Name);

    if (RealName)
    {
        return ToRbxString(RealName);
    }
    else
    {
        return "None";
    }

}



