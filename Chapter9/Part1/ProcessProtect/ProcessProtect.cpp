#include "pch.h"
#include "AutoLock.h"
#include "FastMutex.h"
#include "ProcessProtect.h"
#include "ProtectCommon.h"


OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation);
DRIVER_UNLOAD ProcessProtectUnload;
DRIVER_DISPATCH ProcessProtectCloseCreate, ProcessProtectDeviceControl;
bool AddProcess(ULONG pid);
bool RemoveProcess(ULONG pid);
bool FindProcess(ULONG pid);
extern "C"
char* PsGetProcessImageFileName(PEPROCESS eprocess);

Global g_Data;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	g_Data.Init();

	OB_OPERATION_REGISTRATION operation[] = {
	PsProcessType,
	OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
	OnPreOpenProcess,
	nullptr
	};
	OB_CALLBACK_REGISTRATION reg = {
		OB_FLT_REGISTRATION_VERSION,
		1,
		RTL_CONSTANT_STRING(L"12912.13779139"),
		nullptr,
		operation
	};

	auto status = STATUS_SUCCESS;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ProcessProtect");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	PDEVICE_OBJECT DeviceObject = nullptr;
	do
	{
		status = ObRegisterCallbacks(&reg, &g_Data.RegHandle);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to register Callback, (status = %08X)", status));
			break;
		}

		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create Device, (status = %08X)", status));
			break;
		}
		
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create Symbolic Link, (status = %08X)", status));
			break;
		}

	} while (false);
	
	if (!NT_SUCCESS(status)) {

		if (g_Data.RegHandle) {
			ObUnRegisterCallbacks(g_Data.RegHandle);
		}

		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}

		return status;

	}
	DriverObject->DriverUnload = ProcessProtectUnload;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverObject->MajorFunction[IRP_MJ_CREATE] = ProcessProtectCloseCreate;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcessProtectDeviceControl;
	return status;
}

void ProcessProtectUnload(PDRIVER_OBJECT DriverObject) {
	ObUnRegisterCallbacks(g_Data.RegHandle);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ProcessProtectCloseCreate(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}
NTSTATUS ProcessProtectDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	int lenght = 0;
	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PROCESS_PROTECT_BY_PID:
	{
		auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (size % sizeof(ULONG) != 0) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}

		auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

		AutoLock lock(g_Data.mutex);

		for (int i = 0; i < size / sizeof(ULONG); ++i) {
			auto pid = data[i];
			if (pid == 0) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			
			if (g_Data.CountPids == MaxPids) {
				status = STATUS_TOO_MANY_CONTEXT_IDS;
				break;
			}

			if (FindProcess(pid)) {
				continue;
			}

			if (!AddProcess(pid)) {
				status = STATUS_UNSUCCESSFUL;
				break;
			}
			
			lenght += sizeof(ULONG);

		}
		break;
	}

	case IOCTL_PROCESS_UNPROTECT_BY_PID:
	{
		auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (size % sizeof(ULONG) != 0) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}

		auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
		
		AutoLock lock(g_Data.mutex);

		for (int i = 0; i < size / sizeof(ULONG); ++i) {
			auto pid = data[i];
			if (pid == 0) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (!RemoveProcess(pid)) {
				continue;
			}

			lenght += sizeof(ULONG);

			if (g_Data.CountPids == 0) {
				break;
			}

		}

		break;
	}
	case IOCTL_PROCESS_CLEAR:
	{
		AutoLock lock(g_Data.mutex);
		memset(&g_Data.Pids, 0, sizeof(g_Data.Pids));
		g_Data.CountPids = 0;
		break;
	}
	case IOCTL_PROCESS_QUERY_PIDS:
	{
		auto data = (ProcessInfo*)Irp->AssociatedIrp.SystemBuffer;

		AutoLock lock(g_Data.mutex);
		for (int i = 0; i < g_Data.CountPids; ++i) {
			data->Pids[i] = g_Data.Pids[i];
			PEPROCESS process;
			PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(g_Data.Pids[i]), &process);
			char* name = PsGetProcessImageFileName(process);
			strcpy_s(data->ImageName[i],30, name);
			lenght += sizeof(ProcessInfo);
		}
		break;
	}
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	Irp->IoStatus.Information = lenght;
	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID, POB_PRE_OPERATION_INFORMATION OperationInformation) {
	auto Process = IoGetCurrentProcess();
	if (strcmp(PsGetProcessImageFileName(Process),"lsass.exe") == 0) {
		KdPrint(("Success"));
	}
	if (OperationInformation->KernelHandle) {
		return OB_PREOP_SUCCESS;
	}
	auto ProcessId = (PEPROCESS)OperationInformation->Object;
	auto pid = HandleToUlong(PsGetProcessId(ProcessId));
	AutoLock lock(g_Data.mutex);
	if (FindProcess(pid)) {
		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
	}
	return OB_PREOP_SUCCESS;
}

bool AddProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; ++i) {
		if (g_Data.Pids[i] == 0) {
			g_Data.CountPids++;
			g_Data.Pids[i] = pid;
			return true;
		}
	}
	return false;
}

bool RemoveProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; ++i) {
		if (g_Data.Pids[i] == pid) {
			g_Data.Pids[i] = 0;
			g_Data.CountPids--;
			return true;
		}
	}
	return false;
}

bool FindProcess(ULONG pid){
	for (int i = 0; i < MaxPids; ++i) {
		if (g_Data.Pids[i] == pid) {
			return true;
		}
	}
	return false;
}