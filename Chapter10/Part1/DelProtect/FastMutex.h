#pragma once

#include <wdm.h>

class FastMutex {
public:
	void Init();

	void Lock();
	void UnLock();

private:
	FAST_MUTEX mutex;
};