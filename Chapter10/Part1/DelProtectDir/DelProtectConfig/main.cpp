#include "pch.h"
#include "../DelProtect/DelProtectCommon.h"

int PrintError(const char* text) {
	std::cout << text << " (" << GetLastError() << ")\n";
	return 1;
}

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 3) {
		std::cout << "Usage: DelProtectConfig <option> [directory]\n";
		std::cout << "\tOption : add, remove, clear\n";
		return 0;
	}

	HANDLE hFile = CreateFile(L"\\\\.\\delprotect", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return PrintError("Error to open Handle");
	}

	DWORD returnd;
	bool correctOptinon = true;
	bool success = false;
	if (_wcsicmp(argv[1], L"add") == 0) {
		success = DeviceIoControl(hFile, IOCTL_DELPROTECT_ADD_DIR, argv[2], (wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returnd, nullptr);
	}
	else if (_wcsicmp(argv[1], L"remove") == 0) {
		success = DeviceIoControl(hFile, IOCTL_DELPROTECT_REMOVE_DIR, argv[2], (wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returnd, nullptr);
	}
	else if (_wcsicmp(argv[1], L"clear") == 0) {
		success = DeviceIoControl(hFile, IOCTL_DELPROTECT_CLEAR, argv[2], (wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returnd, nullptr);
	}
	else {
		correctOptinon = false;
		std::cout << "Unknow option\n";
	}

	if (correctOptinon) {
		if (success) {
			std::cout << "Success\n";
		}
		else {
			return PrintError("Failed to DeviceIoControl");
		}
	}

	CloseHandle(hFile);
}