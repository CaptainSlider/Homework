#include "FastMutex.h"

void FastMutex::Init() {
	ExInitializeFastMutex(&mutex);
}

void FastMutex::Lock() {
	ExAcquireFastMutex(&mutex);
}

void FastMutex::UnLock() {
	ExReleaseFastMutex(&mutex);
}