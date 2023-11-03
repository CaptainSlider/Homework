#include "pch.h"
#include "../Block/BlockCommon.h"

int Error(const char* msg) {
	printf("%s: error=%d\n", msg, ::GetLastError());
	return 1;
}

int main(int argc, const char* argv[]) {
	if (argc < 1) {
		printf("Uncorrected write; \\Direct\\");
	}
	auto hFile = CreateFile(L"\\\\.\\BlockedProcess", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return Error("Failed to open device");
	}
	BlockedProcess Blocked;
	DWORD test;
	test = QueryDosDevice(L"C:", (LPWSTR)Blocked.BlockedProcessPath, _MAX_PATH);
	int numArgs;
	const LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &numArgs);
	wcscat_s(Blocked.BlockedProcessPath, wideArgv[1]);
	DWORD bytes;
	BOOL ok = WriteFile(hFile, &Blocked, sizeof(BlockedProcess), &bytes, nullptr);
	if (!ok) {
		return Error("Failed to write file");
	}
	CloseHandle(hFile);
}