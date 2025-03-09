#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <optional>
#include <thread>

#include "Roblox.hpp"
#include "../../../EntryPoint.hpp"


class Driver_
{
private:

    HANDLE MainHandle = nullptr; 
    HMODULE NTDLLHANDLE;
    bool IsMemValid(uintptr_t address);
    bool IsPagePhys(uintptr_t address);
public:
    template <class Ty>
    Ty read(const uintptr_t address)
    {
        Ty value = {};

        for (auto i = 0; i < 5;)
        {
            if (!this->IsMemValid(address) || !this->IsPagePhys(address))
            {
                i++;
            }
            else
                break;
        }

        MEMORY_BASIC_INFORMATION mem_basic_info;

        static auto NtUnlockVirtualMemory = reinterpret_cast<_NtUnlockVirtualMemory*>(GetProcAddress(this->NTDLLHANDLE, "NtUnlockVirtualMemory"));
        NtUnlockVirtualMemory(this->MainHandle, &mem_basic_info.AllocationBase, &mem_basic_info.RegionSize, 1);

        VirtualQueryEx(this->MainHandle, reinterpret_cast<const void*>(address), &mem_basic_info, sizeof(MEMORY_BASIC_INFORMATION));
        ReadProcessMemory(this->MainHandle, reinterpret_cast<const void*>(address), &value, sizeof(Ty), nullptr);

        NtUnlockVirtualMemory(this->MainHandle, &mem_basic_info.AllocationBase, &mem_basic_info.RegionSize, 1);

        return value;
    }

    template <class Ty>
    void Write(const uintptr_t address, const Ty& value)
    {
        for (auto i = 0; i < 5;)
        {
            if (!this->IsMemValid(address) || !this->IsPagePhys(address))
            {
                i++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
                break;
        }

        MEMORY_BASIC_INFORMATION mem_basic_info;

        VirtualQueryEx(this->MainHandle, reinterpret_cast<const void*>(address), &mem_basic_info, sizeof(MEMORY_BASIC_INFORMATION));
        WriteProcessMemory(this->MainHandle, reinterpret_cast<LPVOID>(address), &value, sizeof(Ty), nullptr);

        static auto NtUnlockVirtualMemory = reinterpret_cast<_NtUnlockVirtualMemory*>(GetProcAddress(this->NTDLLHANDLE, "NtUnlockVirtualMemory"));
        NtUnlockVirtualMemory(this->MainHandle, &mem_basic_info.AllocationBase, &mem_basic_info.RegionSize, 1);
    }

    uintptr_t AllocateVirtualMemory(size_t size, DWORD allocation_type = MEM_COMMIT | MEM_RESERVE, DWORD protect = PAGE_READWRITE) {
        if (!MainHandle) {
            CreateException("0", "Process handle not initialized.");
            return 0;
        }

        void* allocated_memory = VirtualAllocEx(MainHandle, nullptr, size, allocation_type, protect);

        return reinterpret_cast<uintptr_t>(allocated_memory);
    }

    template <typename T>
    void WriteMemory(const uintptr_t destination, const T* source, const size_t size) {
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (!this->IsMemValid(destination) || !this->IsPagePhys(destination)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else {
                break;
            }
        }

        MEMORY_BASIC_INFORMATION mem_basic_info;
        VirtualQueryEx(this->MainHandle, reinterpret_cast<const void*>(destination), &mem_basic_info, sizeof(MEMORY_BASIC_INFORMATION));

        WriteProcessMemory(this->MainHandle, reinterpret_cast<LPVOID>(destination), reinterpret_cast<LPCVOID>(source), size, nullptr);

        static auto NtUnlockVirtualMemory = reinterpret_cast<_NtUnlockVirtualMemory*>(GetProcAddress(this->NTDLLHANDLE, "NtUnlockVirtualMemory"));
        if (NtUnlockVirtualMemory) {
            NtUnlockVirtualMemory(this->MainHandle, &mem_basic_info.AllocationBase, &mem_basic_info.RegionSize, 1);
        }
    }

    void Inject(DWORD process_id);

    HANDLE GetProcessHandle() const { return MainHandle; }
    bool IsMemoryValid(uintptr_t address) { return IsMemValid(address); }
    bool IsPageInPhys(uintptr_t address) { return IsPagePhys(address); }
};

inline auto Driver = std::make_unique<Driver_>();