#include "pch.h"
#include "..//ProcessProtect/ProtectCommon.h"

int Error(const char* msg) {
	printf("%s Error= %d\n", msg, GetLastError());
	return 1;
}

std::vector<DWORD> ParsePids(wchar_t* buffer[], int count) {
	std::vector<DWORD> pid;
	for (int i = 0; i < count; i++) {
		pid.push_back(_wtoi(buffer[i]));
	}
	return pid;
}

void PrintInfo(ProcessInfo* info, int count);
int wmain(int argc, wchar_t* argv[]) {
	if (argc < 2) {
		printf("Protect.exe [add | remove | clear] [pid]...\n");
		return 0;
	}

	enum class option {
		Unknow,
		Add,
		Remove,
		Clear,
		Query
	};

	option Option;
	
	if (_wcsicmp(argv[1], L"add") == 0) {
		Option = option::Add;
	}
	else if (_wcsicmp(argv[1], L"remove") == 0) {
		Option = option::Remove;
	}
	else if (_wcsicmp(argv[1], L"clear") == 0) {
		Option = option::Clear;
	}
	else if (_wcsicmp(argv[1], L"query") == 0) {
		Option = option::Query;
	}
	else {
		printf("Unknow option\n");
		printf("Protect.exe [add | remove | clear | query] [pid]...\n");
		return 0;
	}

	HANDLE hFile = CreateFile(L"\\\\.\\ProcessProtect", GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return Error("Failed to open device");
	}

	std::vector<DWORD> pids;
	ProcessInfo info;
	DWORD bytes;
	bool status = false;
	switch (Option)
	{
	case option::Add: {
		pids = ParsePids(argv + 2, argc - 2);
		status = DeviceIoControl(hFile, IOCTL_PROCESS_PROTECT_BY_PID, pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), nullptr, 0, &bytes, nullptr);
		break;
	}		
	case option::Remove:
	{
		pids = ParsePids(argv + 2, argc - 2);
		status = DeviceIoControl(hFile, IOCTL_PROCESS_UNPROTECT_BY_PID, pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), nullptr, 0, &bytes, nullptr);
		break;
	}
	case option::Clear:
	{
		pids = ParsePids(argv + 2, argc - 2);
		status = DeviceIoControl(hFile, IOCTL_PROCESS_CLEAR, pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), nullptr, 0, &bytes, nullptr);
		break;
	}
	case option::Query:
	{
		status = DeviceIoControl(hFile, IOCTL_PROCESS_QUERY_PIDS, nullptr , 0, &info, sizeof(ProcessInfo), &bytes, nullptr);
		PrintInfo(&info, bytes / sizeof(ProcessInfo));
		break;
	}
	default:
		break;
	}
	if (!status) {
		return Error("Failed to Device Io control");
	}
	CloseHandle(hFile);
	return 0;
}

void PrintInfo(ProcessInfo* info, int count) {
	printf("-----------------------\n");
	for (int i = 0; i < count; i++) {
		printf("Process Image:%s\tProcess Id:%d\n", info->ImageName[i], info->Pids[i]);
	}
	printf("-----------------------\n");
}