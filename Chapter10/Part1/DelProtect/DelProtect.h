#pragma once

#define DRIVER_TAG 'delp'

const int MaxExecutables = 10;

struct Globals
{
	int ExeNamesCount;
	FastMutex ExeNameLock;
	WCHAR* ExeNames[MaxExecutables];
};