#pragma once
#include <fltkernel.h>

enum FileNameInformationOption {
	Normalized = FLT_FILE_NAME_NORMALIZED,
	Opened = FLT_FILE_NAME_OPENED,
	Short = FLT_FILE_NAME_SHORT,

	Default = FLT_FILE_NAME_QUERY_DEFAULT,
	CacheOnly = FLT_FILE_NAME_QUERY_CACHE_ONLY,
	FileSystemOnly = FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY,
	AllowCacheLookUp = FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,

	RequestFromCurrentProvider = FLT_FILE_NAME_REQUEST_FROM_CURRENT_PROVIDER,
	DoNotCache = FLT_FILE_NAME_DO_NOT_CACHE,
	Allo2WueryOnReparse = FLT_FILE_NAME_ALLOW_QUERY_ON_REPARSE

};
DEFINE_ENUM_FLAG_OPERATORS(FileNameInformationOption)

typedef struct FilterFileNameInformation {
	FilterFileNameInformation(PFLT_CALLBACK_DATA CallbackData, FileNameInformationOption Option = FileNameInformationOption::Opened |
		FileNameInformationOption::Default);

	~FilterFileNameInformation();

	operator FLT_FILE_NAME_INFORMATION() {
		return *_info;
	}

	operator bool() {
		if (_info) {
			return true;
		}
		return false;
	}

	PFLT_FILE_NAME_INFORMATION operator ->() {
		return _info;
	}

	NTSTATUS Parce();

private:
	PFLT_FILE_NAME_INFORMATION _info;
} * PFilterFileNameInformation;
