#include "pch.h"
#pragma once



class MemoryManager {
public:
    std::wstring processName;
    HANDLE hProcess;
    DWORD pid;
    uintptr_t baseAddress;
    uint8_t bufferData[8]{};

    // Constructor
    MemoryManager(const std::wstring& processName)
        : processName(processName), hProcess(nullptr), pid(0), baseAddress(0) {
        pid = FindPID();
        if (pid) {
            hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
            if (hProcess) {
                baseAddress = GetModuleBaseAddress();
                if (baseAddress == 0) {
                    std::cerr << "[!] Failed to retrieve module base address.\n";
                }
                InitNTFunctions();  // Automatically initialize NT functions
                
            }
            else {
                std::cerr << "[!] Failed to open process. Error: " << GetLastError() << "\n";
            }
        }
        else {
            std::cerr << "[!] Process not found: " << wstring_to_string(processName) << "\n";
        }
    }


    // Destructor
    ~MemoryManager() {
        if (hProcess) {
            CloseHandle(hProcess);
        }
    }

    // Helper to convert wstring to string for logging
    static std::string wstring_to_string(const std::wstring& wstr) {
        std::string result(wstr.begin(), wstr.end());
        return result;
    }

    // Check if the manager is valid
    bool IsValid() const {
        return hProcess != nullptr;
    }

    // Standard Read Memory
    template <typename T>
    T ReadMemory(uintptr_t address) {
        T buffer{};
        if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr)) {
            return buffer;
        }
        else {
            std::cerr << "[!] ReadProcessMemory failed at address: 0x" << std::hex << address
                << ". Error: " << GetLastError() << "\n";
            return T{};
        }
    }

    // Standard Write Memory
    template <typename T>
    bool WriteMemory(uintptr_t address, const T& data) {
        if (WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(address), &data, sizeof(T), nullptr)) {
            return true;
        }
        else {
            std::cerr << "[!] WriteProcessMemory failed at address: 0x" << std::hex << address
                << ". Error: " << GetLastError() << "\n";
            return false;
        }
    }

    // NT Read Memory
    template <typename T>
    bool NTReadMemory(uintptr_t address, T& buffer) {
        if (!NtReadVirtualMemory) {
            std::cerr << "[!] NtReadVirtualMemory is not initialized.\n";
            return false;
        }

        SIZE_T bytesRead = 0;
        NTSTATUS status = NtReadVirtualMemory(hProcess, (PVOID)address, &buffer, sizeof(T), &bytesRead);
        if (!NT_SUCCESS(status)) {
            std::cerr << "[!] NtReadVirtualMemory failed at address: 0x" << std::hex << address
                << ". Status: 0x" << std::hex << status << "\n";
            return false;
        }
        return true;
    }

    // NT Write Memory
    template <typename T>
    bool NTWriteMemory(uintptr_t address, const T& buffer) {
        if (!NtWriteVirtualMemory) {
            std::cerr << "[!] NtWriteVirtualMemory is not initialized.\n";
            return false;
        }

        SIZE_T bytesWritten = 0;
        NTSTATUS status = NtWriteVirtualMemory(hProcess, (PVOID)address, (PVOID)&buffer, sizeof(T), &bytesWritten);
        if (!NT_SUCCESS(status)) {
            std::cerr << "[!] NtWriteVirtualMemory failed at address: 0x" << std::hex << address
                << ". Status: 0x" << std::hex << status << "\n";
            return false;
        }
        return true;
    }

    // Helper: Find PID by process name
    DWORD FindPID() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            std::cerr << "[!] Failed to create process snapshot. Error: " << GetLastError() << "\n";
            return 0;
        }

        PROCESSENTRY32W pe32 = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                    CloseHandle(hSnapshot);
                    return pe32.th32ProcessID;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return 0;
    }

    // Helper: Get the base address of the main module
    uintptr_t GetModuleBaseAddress() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            std::cerr << "[!] Failed to create module snapshot. Error: " << GetLastError() << "\n";
            return 0;
        }

        MODULEENTRY32W me32 = { sizeof(MODULEENTRY32W) };
        if (Module32FirstW(hSnapshot, &me32)) {
            CloseHandle(hSnapshot);
            return reinterpret_cast<uintptr_t>(me32.modBaseAddr);
        }

        CloseHandle(hSnapshot);
        return 0;
    }

    bool ReadFString(uintptr_t strAddress, std::vector<wchar_t>& outChars) {
        if (!strAddress) return false;

        // Step 1: Read the pointer that stores the address of the wide string
        uintptr_t strPtr = ReadMemory<uintptr_t>(strAddress);
        if (!strPtr) {
            std::cerr << "[!] Failed to read Text16Ptr from 0x" << std::hex << strAddress << "\n";
            return false;
        }

        // Step 2: Read wchar_t characters one by one
        outChars.clear();
        constexpr size_t maxLen = 512; // Prevent excessive reads
        for (size_t i = 0; i < maxLen; ++i) {
            wchar_t ch = ReadMemory<wchar_t>(strPtr + i * sizeof(wchar_t));
            if (ch == L'\0') break; // Stop at null terminator
            outChars.push_back(ch);
        }

        return !outChars.empty(); // Return false if no characters were read
    }


private:
    typedef NTSTATUS(WINAPI* NtReadVirtualMemoryPtr)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
    typedef NTSTATUS(WINAPI* NtWriteVirtualMemoryPtr)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

    HMODULE hNtDll = GetModuleHandle(L"ntdll.dll");
    NtReadVirtualMemoryPtr NtReadVirtualMemory = nullptr;
    NtWriteVirtualMemoryPtr NtWriteVirtualMemory = nullptr;

    // Initialize NT functions
    void InitNTFunctions() {
        if (!hNtDll) {
            std::cerr << "[!] Failed to load ntdll.dll. Error: " << GetLastError() << "\n";
            return;
        }

        NtReadVirtualMemory = reinterpret_cast<NtReadVirtualMemoryPtr>(
            GetProcAddress(hNtDll, "NtReadVirtualMemory"));
        NtWriteVirtualMemory = reinterpret_cast<NtWriteVirtualMemoryPtr>(
            GetProcAddress(hNtDll, "NtWriteVirtualMemory"));

        if (!NtReadVirtualMemory || !NtWriteVirtualMemory) {
            std::cerr << "[!] Failed to resolve NT memory functions. Error: " << GetLastError() << "\n";
        }
    }


};



