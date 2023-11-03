#include "pch.h"
#include "FastMutex.h"

void FastMutex::Init() {
	ExInitializeFastMutex(&mutex);
}

void FastMutex::Lock() {
	ExAcquireFastMutex(&mutex);
}

void FastMutex::Unlock() {
	ExReleaseFastMutex(&mutex);
}