#include "pch.h"
#include "BlockIntegration.h"
#include <intrin.h>

DRIVER_UNLOAD BlockIntegrationUnload;
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID Context, POB_PRE_OPERATION_INFORMATION OperationInformation);

extern "C"
NTSTATUS ZwQueryInformationProcess(
	_In_      HANDLE           ProcessHandle,
	_In_      PROCESSINFOCLASS ProcessInformationClass,
	_Out_     PVOID            ProcessInformation,
	_In_      ULONG            ProcessInformationLength,
	_Out_opt_ PULONG           ReturnLength
);

extern "C"
char* PsGetProcessImageFileName(PEPROCESS eprocess);

NTSTATUS IsDebug(PEPROCESS Process, bool* isDebug);

//Globals
PVOID g_RegistrationHandle;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	auto status = STATUS_SUCCESS;

	OB_OPERATION_REGISTRATION operation[] = {
		PsProcessType,
		OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
		OnPreOpenProcess,
		nullptr
	};

	OB_CALLBACK_REGISTRATION reg = {
		OB_FLT_REGISTRATION_VERSION,
		1,
		RTL_CONSTANT_STRING(L"1212121.121212"),
		nullptr,
		operation
	};

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\BlockIntegration");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BlockIntegration");
	PDEVICE_OBJECT DeviceObject;
	bool createSymLink = false;

	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, false, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint(("failed to create device (status = %08X)", status));
			break;
		}

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("failed to create symbol link (status = %08X)", status));
			break;
		}
		createSymLink = true;
		
		status = ObRegisterCallbacks(&reg, &g_RegistrationHandle);
		if (!NT_SUCCESS(status)) {
			KdPrint(("failed to register callback (status = %08X)", status));
			break;
		}

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}

		if (createSymLink) {
			IoDeleteSymbolicLink(&symLink);
		}

	}

	DriverObject->DriverUnload = BlockIntegrationUnload;

	return status;
}

void BlockIntegrationUnload(PDRIVER_OBJECT DriverObject) {
	ObUnRegisterCallbacks(g_RegistrationHandle);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BlockIntegration");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}



OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID, POB_PRE_OPERATION_INFORMATION OperationInformation) {
	if (OperationInformation->KernelHandle) {
		return OB_PREOP_SUCCESS;
	}

	auto process = static_cast<PEPROCESS>(OperationInformation->Object);
	auto currentProcess = PsGetCurrentProcess();
	if (currentProcess == process) {
		return OB_PREOP_SUCCESS;
	}

	//if (strcmp(PsGetProcessImageFileName(process), "msedge.exe") == 0 && strcmp(PsGetProcessImageFileName(currentProcess), "msedge.exe")) {
	//	KdPrint(("Current: %s\n", PsGetProcessImageFileName(currentProcess)));
	//	KdPrint(("PROCESS: %s\n", PsGetProcessImageFileName(process)));
	//}
	

	if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE && (OperationInformation->Parameters->CreateHandleInformation.DesiredAccess & RemoteMask)) {
		bool isDebug = false;
		auto status = IsDebug(process, &isDebug);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Failed to IsDebug (status=%08X)", status));
			return OB_PREOP_SUCCESS;
		}

		if (isDebug) {
			return OB_PREOP_SUCCESS;
		}

		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~RemoteMask;
	}
	else {
		if (OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess & RemoteMask) {
			bool isDebug = false;
			auto status = IsDebug(process, &isDebug);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Failed to IsDebug (status=%08X)", status));
				return OB_PREOP_SUCCESS;
			}

			if (isDebug) {
				return OB_PREOP_SUCCESS;
			}

			OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~RemoteMask;
		}
	}

	return OB_PREOP_SUCCESS;
}

NTSTATUS IsDebug(PEPROCESS Process,bool* isDebug) {
	HANDLE hProcess;
	*isDebug = false;
	auto status = ObOpenObjectByPointer(Process, OBJ_KERNEL_HANDLE, nullptr, READ_CONTROL, nullptr, KernelMode, &hProcess);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to open object (status=%08X)", status));
		return status;
	}

	PULONG pDebugPort = nullptr;
	ULONG returnLength = 0;
	status = ZwQueryInformationProcess(hProcess, ProcessDebugPort, &pDebugPort, sizeof(pDebugPort), &returnLength);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to check debug (status=%08X)", status));
		return status;
	}

	if (pDebugPort) {
		*isDebug = true;
	}
	ObCloseHandle(hProcess, KernelMode);
	return STATUS_SUCCESS;
}