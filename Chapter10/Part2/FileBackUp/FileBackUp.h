#pragma once

#define DRIVER_PREFIX_CONTEXT 'cFbu'
#define DRIVER_PREFIX 'Fbu'

typedef struct FileContext
{
	bool Written;
	UNICODE_STRING name;
	Mutex mutex;
} * PFileContext;