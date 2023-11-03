#pragma once

#define DRIVER_PREFIX "ProcessProtect: "

#define PROCESS_TERMINATE 1

#include "FastMutex.h"

const int MaxPids = 10;

struct Global
{
	int CountPids;
	ULONG Pids[MaxPids];
	FastMutex mutex;
	PVOID RegHandle;

	void Init() {
		mutex.Init();
	}
};
