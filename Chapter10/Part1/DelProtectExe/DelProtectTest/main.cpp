#include "pch.h"

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 3) {
		std::cout << "Usage: DelProtectTest.exe <method> [filename]\n";
		std::cout << "\tMethod: 1 = DeleteFile, 2 = CreatFile,  3 = SetFileInformationByHandle\n";
		return 0;
	}

	bool succes = true;
	int method = _wtoi(argv[1]);
	switch (method)
	{
	case 1: {
		succes = DeleteFile(argv[2]);
		break;
	}
	case 2: {
		HANDLE hFile = CreateFile(argv[2], DELETE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		if (hFile == INVALID_HANDLE_VALUE) {
			succes = false;
		}
		CloseHandle(hFile);
		break;
	}
	case 3: {
		HANDLE hFile = CreateFile(argv[2], DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile == INVALID_HANDLE_VALUE) {
			succes = false;
		}

		FILE_DISPOSITION_INFO info;
		info.DeleteFileW = TRUE;
		succes = SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(info));

		CloseHandle(hFile);

		break;
	}
	default:
		break;
	}

	if (succes) {
		std::cout << "Success\n";
	}
	else {
		std::cout << "Erorr: " << GetLastError() << std::endl;
		return 1;
	}

	return 0;
}
