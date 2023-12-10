#pragma once
#include <wdm.h>

class Mutex {
public:
	void Init() {
		KeInitializeMutex(&_mutex, 0);
	}

	void Lock() {
		KeWaitForSingleObject(&_mutex, Executive, KernelMode, FALSE, nullptr);
	}

	void UnLock() {
		KeReleaseMutex(&_mutex, FALSE);
	}
private:
	KMUTEX _mutex;
};