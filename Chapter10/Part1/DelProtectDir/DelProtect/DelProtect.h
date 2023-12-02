#pragma once

#define DRIVER_TAG 'delp'

const int MaxDirectories = 10;

struct DirectrEntry
{
	UNICODE_STRING NtPath;
	UNICODE_STRING DosPath;

	void Free() {
		if (NtPath.Buffer) {
			ExFreePool(NtPath.Buffer);
			NtPath.Buffer = nullptr;
		}
		if (DosPath.Buffer) {
			ExFreePool(DosPath.Buffer);
			DosPath.Buffer = nullptr;
		}
	}
};
struct Globals
{
	int DirectoryCount;
	FastMutex DirectoryLock;
	DirectrEntry DirectoryNames[MaxDirectories];
};