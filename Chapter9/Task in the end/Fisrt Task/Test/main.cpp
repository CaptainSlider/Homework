#include "pch.h"

int Error(const char* msg) {
    printf("%s Erorr= %d\n", msg, GetLastError());
    return 1;
}

int wmain(int argc,wchar_t* argv[])
{
    if (argc < 1) {
        printf("Test [PID]\n");
        return 0;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _wtoi(argv[1]));
    if (hProcess == NULL)
    {
        return Error("Failed to open Process");
    }

    HMODULE Module = LoadLibraryA("user32.dll");
    FARPROC loadLibraryAddress = GetProcAddress(Module, "MessageBoxW");
    if (!loadLibraryAddress) {
        return Error("Failed to ger proc adress");
    }

    LPVOID pRemoteParam = VirtualAllocEx(hProcess, NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemoteParam == NULL)
    {
        CloseHandle(hProcess);
        return Error("Failed to Allocate");
    }

    const wchar_t pMessage[] = L"Hello, world!";
    bool statusWrite = WriteProcessMemory(hProcess, pRemoteParam, pMessage, wcslen(pMessage) * sizeof(wchar_t), NULL);
    if (!statusWrite) {
        return Error("Failed to write process memory");
    }
    
    while (CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddress, pRemoteParam, 0, NULL) != NULL) {
        printf("Driver not work");
    }
    printf("Driver Work");

    VirtualFreeEx(hProcess, pRemoteParam, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return 0;
}