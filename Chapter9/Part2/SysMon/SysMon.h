#pragma once

#include "FastMutex.h"

#define DRIVER_PREFIX "SysMon: "
#define DRIVER_TAG 'nmys'

struct Globals {
	LIST_ENTRY ItemsHead;
	int ItemCount;
	FastMutex MutexItem;
	FastMutex MutexNotify;
	LARGE_INTEGER Cookie;
	bool ProcessInfo;
	bool ThreadInfo;
	bool ImageLoadInfo;
	bool RegisterSetValueInfo;
};

template<typename T>
struct FullItem {
	LIST_ENTRY Entry;
	T Data;
};

