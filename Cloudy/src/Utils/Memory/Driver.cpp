#include "Driver.hpp"

bool Driver_::IsMemValid(uintptr_t address)
{
    MEMORY_BASIC_INFORMATION mem_info;
    if (VirtualQueryEx(this->MainHandle, reinterpret_cast<const void*>(address), &mem_info, sizeof(mem_info)) == sizeof(mem_info))
        return mem_info.State == MEM_COMMIT && (mem_info.Type == MEM_PRIVATE || mem_info.Type == MEM_MAPPED);

    return false;
}

bool Driver_::IsPagePhys(uintptr_t address)
{
    PSAPI_WORKING_SET_EX_INFORMATION working_set_info;
    working_set_info.VirtualAddress = reinterpret_cast<void*>(address);

    if (K32QueryWorkingSetEx(this->MainHandle, &working_set_info, sizeof(working_set_info)))
        return working_set_info.VirtualAttributes.Valid;
    else
    {
        CreateException("74", "Error setting K32 Query Working.");
        return false;
    }
}

void Driver_::Inject(DWORD pid)
{
    if (this->MainHandle)
    {
        CloseHandle(this->MainHandle);
        this->MainHandle = nullptr;
    }

    if (pid && pid > 1)
    {
        this->NTDLLHANDLE = LoadLibraryA("ntdll.dll");
        if (!NTDLLHANDLE) {
            CreateException("8", "Couldn't load ntdll.");
            return;
        }

        this->MainHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        return;
    }
    else
    {
        CreateException("694", "Process id isn't valid.");
        return;
    }
}
