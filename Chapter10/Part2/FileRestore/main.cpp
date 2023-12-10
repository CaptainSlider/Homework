#include <iostream>
#include <Windows.h>
#include <string>

int PrintError(const char* msg) {
	std::cout << msg << " " << GetLastError() << std::endl;
	return 1;
}

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 2) {
		std::cout << "Using: FileRestore <pathfile>";
		return 0;
	}
	std::wstring SourceName(argv[1]);
	SourceName += L":backup";
	
	HANDLE hSourceFile = CreateFile(SourceName.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hSourceFile == INVALID_HANDLE_VALUE) {
		return PrintError("Failed to Open Target File");
	}

	HANDLE hTargetFile = CreateFile(argv[1], GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hTargetFile == INVALID_HANDLE_VALUE) {
		return PrintError("Failed to Open Source File");
	}

	LARGE_INTEGER FileSize;
	if (!GetFileSizeEx(hSourceFile, &FileSize)) {
		return PrintError("Failed to Get size");
	}
	
	ULONG bufferSize = (ULONG)min((LONGLONG)1 << 21, FileSize.QuadPart);
	std::cout << bufferSize << "\n";
	void* buffer = VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buffer) {
		return PrintError("Failed to Allocate");
	}

	DWORD bytes;
	while (FileSize.QuadPart > 0) {
		if (!ReadFile(hSourceFile, buffer, (ULONG)min((LONGLONG)bufferSize, FileSize.QuadPart), &bytes, nullptr)) {
			return PrintError("Failed to read file");
		}

		if (!WriteFile(hTargetFile, buffer, bytes, &bytes, nullptr)) {
			return PrintError("Failed to write file");
		}
		FileSize.QuadPart -= bytes;
	}

	CloseHandle(hTargetFile);
	CloseHandle(hSourceFile);
	VirtualFree(buffer, 0, MEM_RELEASE);

	return 0;
}