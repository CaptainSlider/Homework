// SysMonClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "..\SysMon\SysMonCommon.h"
#include <string>

int Error(const char* text) {
	printf("%s (%d)\n", text, ::GetLastError());
	return 1;
}

void DisplayTime(const LARGE_INTEGER& time) {
	SYSTEMTIME st;
	::FileTimeToSystemTime((FILETIME*)&time, &st);
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void DisplayBinary(const UCHAR* buffer,DWORD count) {
	for (int i = 0; i < count; ++i) {
		printf("%02X", buffer[i]);
	}
	printf("\n");
}

void DisplayInfo(BYTE* buffer, DWORD size) {
	auto count = size;
	while (count > 0) {
		auto header = (ItemHeader*)buffer;
		switch (header->Type) {
			case ItemType::ProcessExit:
			{
				DisplayTime(header->Time);
				auto info = (ProcessExitInfo*)buffer;
				printf("Process %d Exited\n", info->ProcessId);
				break;
			}

			case ItemType::ProcessCreate:
			{
				DisplayTime(header->Time);
				auto info = (ProcessCreateInfo*)buffer;
				std::wstring commandline((WCHAR*)(buffer + info->CommandLineOffset), info->CommandLineLength);
				printf("Process %d Created. Command line: %ws\n", info->ProcessId, commandline.c_str());
				break;
			}

			case ItemType::ThreadCreate:
			{
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
				printf("Thread %d Created in process %d\n", info->ThreadId, info->ProcessId);
				break;
			}

			case ItemType::ThreadExit:
			{
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
				printf("Thread %d Exited from process %d\n", info->ThreadId, info->ProcessId);
				break;
			}

			case ItemType::ImageLoad:
			{
				DisplayTime(header->Time);
				auto info = (ImageLoadInfo*)buffer;
				printf("Image loaded into process %d at address 0x%p (%ws)\n", info->ProcessId, info->LoadAddress, info->ImageFileName);
				break;
			}
			case ItemType::RegistrySetValueInfo:
			{
				DisplayTime(header->Time);
				auto info = (RegistrySetValueInfo*)buffer;
				printf("Regsiter to set value PID=%d: %ws\\%ws type:%d size:%d data:", info->ProcessId, info->KeyName, info->ValueName, info->DataType, info->DataSize);
				switch (info->DataType)
				{
				case REG_BINARY: {
					DisplayBinary(info->Data, min(info->DataSize, sizeof(info->Data)));
					break;
				}
				case REG_QWORD:
				case REG_DWORD: {
					printf("%08X\n", *(DWORD*)info->Data);
					break;
				}

				case REG_EXPAND_SZ:
				case REG_LINK:
				case REG_SZ: {
					printf("%ws\n", (WCHAR*)info->Data);
					break;
				}
				default:
				DisplayBinary(info->Data, min(info->DataSize, sizeof(info->Data)));
				break;
				}
				break;
			}
			default:
				break;
		}
		buffer += header->Size;
		count -= header->Size;
	}

}

int wmain(int argc,wchar_t* argv[]) {
	auto hFile = ::CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return Error("Failed to open file");

	enum class BlockNotifu {
		BlockImageNotify,
		BlockRegisterNotify,
		BlockProcessNotify,
		BlockThreadNotify,
		UnBlockImageNotify,
		UnBlockRegisterNotify,
		UnBlockProcessNotify,
		UnBlockThreadNotify
		
	};
	std::vector<BlockNotifu> block;
	if (argc > 1) {
		for (int i = 1; i < argc; ++i) {
			if (_wcsicmp(argv[i], L"BlockImageNotify") == 0) {
				block.push_back(BlockNotifu::BlockImageNotify);
			}
			else if (_wcsicmp(argv[i], L"BlockRegisterNotify") == 0) {
				block.push_back(BlockNotifu::BlockRegisterNotify);
			}
			else if (_wcsicmp(argv[i], L"BlockProcessNotify") == 0) {
				block.push_back(BlockNotifu::BlockProcessNotify);
			}
			else if (_wcsicmp(argv[i], L"BlockThreadNotify") == 0) {
				block.push_back(BlockNotifu::BlockThreadNotify);
			}
			//unblock
			else if (_wcsicmp(argv[i], L"UnBlockImageNotify") == 0) {
				block.push_back(BlockNotifu::UnBlockImageNotify);
			}
			else if (_wcsicmp(argv[i], L"UnBlockRegisterNotify") == 0) {
				block.push_back(BlockNotifu::UnBlockRegisterNotify);
			}
			else if (_wcsicmp(argv[i], L"UnBlockProcessNotify") == 0) {
				block.push_back(BlockNotifu::UnBlockProcessNotify);
			}
			else if (_wcsicmp(argv[i], L"UnBlockThreadNotify") == 0) {
				block.push_back(BlockNotifu::UnBlockThreadNotify);
			}
		}
	}
	for (int i = 0; i < block.size(); ++i) {
		//Block
		if (block[i] == BlockNotifu::BlockImageNotify){
			DWORD Bytes;
			if (!DeviceIoControl(hFile,IOCTL_IMAGELOAD_INFO,nullptr,0,nullptr,0,&Bytes,nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::BlockRegisterNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_REGISTERSETVALUE_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::BlockProcessNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_PROCESS_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::BlockThreadNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_THREAD_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		//Unblock
		else if (block[i] == BlockNotifu::UnBlockImageNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_UNBLOCKIMAGELOAD_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::UnBlockRegisterNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_UNBLOCKREGISTERSETVALUE_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::UnBlockProcessNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_UNBLOCKPROCESS_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
		else if (block[i] == BlockNotifu::UnBlockThreadNotify) {
			DWORD Bytes;
			if (!DeviceIoControl(hFile, IOCTL_UNBLOCKTHREAD_INFO, nullptr, 0, nullptr, 0, &Bytes, nullptr)) {
				return Error("failed to Device Io control\n");
			}
		}
	}
	BYTE buffer[1 << 16];

	while (true) {
		DWORD bytes;
		if (!::ReadFile(hFile, buffer, sizeof(buffer), &bytes, nullptr))
			return Error("Failed to read");

		if (bytes != 0)
			DisplayInfo(buffer, bytes);

		::Sleep(200);
	}
}

