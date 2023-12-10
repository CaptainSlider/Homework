#include <iostream>
#include <Windows.h>
#include <fltUser.h>
#include "../FileBackUp/FileBackUpCommon.h"
#pragma comment(lib, "fltlib")

void printMessage(const BYTE* buffer) {
	auto message = (FileBackUpMessage*)buffer;
	std::wstring msg(message->Name, message->LenghtName);
	std::wcout << msg.c_str() << std::endl;
}

int main() {
	HANDLE hPort;
	auto hr = FilterConnectCommunicationPort(L"\\FileBackUp", 0, nullptr, 0, nullptr, &hPort);
	if (FAILED(hr)) {
		std::cout << "Failed to connect HR= " << std::hex << hr << std::endl;
		return 1;
	}

	BYTE buffer[1 << 12];
	auto message = (FILTER_MESSAGE_HEADER*)buffer;
	while (true) {
		hr = FilterGetMessage(hPort, message, sizeof(buffer), nullptr);
		if (FAILED(hr)) {
			std::cout << "Failed to get message HR= " << hr << std::endl;
			break;
		}
		printMessage(buffer + sizeof(FILTER_MESSAGE_HEADER));
	}

	CloseHandle(hPort);

	return 0;
}