#include "FileNameInformation.h"

FilterFileNameInformation::FilterFileNameInformation(PFLT_CALLBACK_DATA CallbackData, FileNameInformationOption Option) {
	auto status = FltGetFileNameInformation(CallbackData, Option, &_info);

	if (!NT_SUCCESS(status)) {
		_info = nullptr;
	}
}

FilterFileNameInformation::~FilterFileNameInformation() {
	if (_info) {
		FltReleaseFileNameInformation(_info);
	}
}

NTSTATUS FilterFileNameInformation::Parce() {
	return FltParseFileNameInformation(_info);
}