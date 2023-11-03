#include "pch.h"
#include "BlockCommon.h"
#include "Block.h"

DRIVER_DISPATCH BlockedProcessCloseCreated, BlockedProcessWrite;
void BlockedProcessUnload(PDRIVER_OBJECT DriverObject);
void OnImageNotify(PUNICODE_STRING FullImageFileName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);

BlockedProcess g_blockProcess;
extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING PathRegister) {
	UNREFERENCED_PARAMETER(PathRegister);

	auto status = STATUS_SUCCESS;

	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\BlockedProcess");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BlockedProcess");

	bool symLinkCreated = false, processCallback = false;

	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Failed to create Device (0x%08X)", status));
			break;
		}

		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Failed to created Symbolic link (0x%08X)", status));
			break;
		}
		symLinkCreated = true;

		status = PsSetLoadImageNotifyRoutine(OnImageNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Failed to create Process Notify (0x%08X)", status));
			break;
		}
		processCallback = true;

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
		if (symLinkCreated) {
			IoDeleteSymbolicLink(&symLink);
		}
		if (processCallback) {
			PsRemoveLoadImageNotifyRoutine(OnImageNotify);
		}
	}
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverObject->MajorFunction[IRP_MJ_CREATE] = BlockedProcessCloseCreated;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = BlockedProcessWrite;

	DriverObject->DriverUnload = BlockedProcessUnload;

	return status;
}

NTSTATUS BlockedProcessCloseCreated(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}

NTSTATUS BlockedProcessWrite(PDEVICE_OBJECT, PIRP Irp) {
	auto status = STATUS_SUCCESS;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto lenght = stack->Parameters.Write.Length;

	auto info = (BlockedProcess*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (info) {
		wcsncpy_s(g_blockProcess.BlockedProcessPath, _MAX_PATH, info->BlockedProcessPath, _MAX_PATH);
	}
	else {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	Irp->IoStatus.Information = lenght;
	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, 0);
	return status;
}

void BlockedProcessUnload(PDRIVER_OBJECT DriverObject) {
	PsRemoveLoadImageNotifyRoutine(OnImageNotify);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\BlockedProcess");
	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);
}

void OnImageNotify(PUNICODE_STRING FullImageFileName, HANDLE ProcessId, PIMAGE_INFO) {
	if (FullImageFileName == nullptr) {
		return;
	}
	auto status = STATUS_SUCCESS;
	WCHAR path[_MAX_PATH];
	wcsncpy_s(path, _MAX_PATH, FullImageFileName->Buffer, FullImageFileName->Length);
	int result = wcscmp(path, g_blockProcess.BlockedProcessPath);
	if (result == 0) {
		HANDLE hProcess;
		OBJECT_ATTRIBUTES obProcess;
		InitializeObjectAttributes(&obProcess, NULL, 0, NULL, NULL);
		CLIENT_ID Id;
		Id.UniqueProcess = ProcessId;
		Id.UniqueThread = 0;
		status = ZwOpenProcess(&hProcess, DELETE, &obProcess, &Id);
		ZwTerminateProcess(hProcess,status);
		ZwClose(hProcess);
	}

	else {
		return;
	}
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to terminateprocess (0x%08X)", status));
	}

}
