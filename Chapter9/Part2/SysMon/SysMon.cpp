#include "pch.h"
#include "SysMon.h"
#include "SysMonCommon.h"
#include "AutoLock.h"

DRIVER_UNLOAD SysMonUnload;
DRIVER_DISPATCH SysMonCreateClose, SysMonRead, SysMonDeviceControl;

NTSTATUS OnRegistreNotify(_In_ PVOID Context, _In_opt_ PVOID Argument1, _In_opt_ PVOID Argument2);
void OnProcessNotify(_Inout_ PEPROCESS Process, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create);
void OnImageLoadNotify(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo);
void PushItem(LIST_ENTRY* entry);

Globals g_Globals;

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	auto status = STATUS_SUCCESS;

	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.MutexItem.Init();
	g_Globals.MutexNotify.Init();

	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	g_Globals.ImageLoadInfo = false;
	g_Globals.ProcessInfo = false;
	g_Globals.ThreadInfo = false;
	g_Globals.RegisterSetValueInfo = false;
	bool symLinkCreated = false;
	bool processCallbacks = false, threadCallbacks = false, imagineCallback = false;

	do {
		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\sysmon");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create sym link (0x%08X)\n", status));
			break;
		}
		symLinkCreated = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to register process callback (0x%08X)\n", status));
			break;
		}
		processCallbacks = true;

		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to set thread callback (status=%08X)\n", status));
			break;
		}
		threadCallbacks = true;

		status = PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to set image load callback (status=%08X)\n", status));
			break;
		}
		imagineCallback = true;

		UNICODE_STRING Altitude = RTL_CONSTANT_STRING(L"1213214.2313123");
		status = CmRegisterCallbackEx(OnRegistreNotify, &Altitude, DriverObject, nullptr, &g_Globals.Cookie, nullptr);
		if (NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to set registre callback (status = %08X)\n", status));
			break;
		}
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (imagineCallback) {
			PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
		}
		if (threadCallbacks)
			PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
		if (processCallbacks)
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
	}

	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SysMonDeviceControl;
	return status;
}

NTSTATUS SysMonCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}
NTSTATUS SysMonDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PROCESS_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ProcessInfo = true;
		break;
	}
	case IOCTL_THREAD_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ThreadInfo = true;
		break;
	}
	case IOCTL_IMAGELOAD_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ImageLoadInfo = true;
		break;
	}
	case IOCTL_REGISTERSETVALUE_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.RegisterSetValueInfo = true;
		break;
	}
	//Unblock
	case IOCTL_UNBLOCKPROCESS_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ProcessInfo = false;
		break;
	}
	case IOCTL_UNBLOCKTHREAD_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ThreadInfo = false;
		break;
	}
	case IOCTL_UNBLOCKIMAGELOAD_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.ImageLoadInfo = false;
		break;
	}
	case IOCTL_UNBLOCKREGISTERSETVALUE_INFO: {
		AutoLock lock(g_Globals.MutexItem);
		g_Globals.RegisterSetValueInfo = false;
		break;
	}
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
NTSTATUS SysMonRead(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	auto status = STATUS_SUCCESS;
	auto count = 0;
	NT_ASSERT(Irp->MdlAddress);		// we're using Direct I/O

	auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		AutoLock lock(g_Globals.MutexItem);
		while (true) {
			if (IsListEmpty(&g_Globals.ItemsHead))	// can also check g_Globals.ItemCount
				break;

			auto entry = RemoveHeadList(&g_Globals.ItemsHead);
			auto info = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
			auto size = info->Data.Size;
			if (len < size) {
				// user's buffer full, insert item back
				InsertHeadList(&g_Globals.ItemsHead, entry);
				break;
			}
			g_Globals.ItemCount--;
			::memcpy(buffer, &info->Data, size);
			len -= size;
			buffer += size;
			count += size;
			ExFreePool(info);
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = count;
	IoCompleteRequest(Irp, 0);
	return status;
}

void SysMonUnload(PDRIVER_OBJECT DriverObject) {
	CmUnRegisterCallback(g_Globals.Cookie);
	PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_Globals.ItemsHead)) {
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}
}

_Use_decl_annotations_
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	AutoLock<FastMutex> lock(g_Globals.MutexNotify);
	if (g_Globals.ProcessInfo) {
		return;
	}
	UNREFERENCED_PARAMETER(Process);

	if (CreateInfo) {
		// process created
		USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
		USHORT commandLineSize = 0;
		if (CreateInfo->CommandLine) {
			commandLineSize = CreateInfo->CommandLine->Length;
			allocSize += commandLineSize;
		}
		auto info = (FullItem<ProcessCreateInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, allocSize, DRIVER_TAG);
		if (info == nullptr) {
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			return;
		}

		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessCreate;
		item.Size = sizeof(ProcessCreateInfo) + commandLineSize;
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);

		if (commandLineSize > 0) {
			::memcpy((UCHAR*)&item + sizeof(item), CreateInfo->CommandLine->Buffer, commandLineSize);
			item.CommandLineLength = commandLineSize / sizeof(WCHAR);	// length in WCHARs
			item.CommandLineOffset = sizeof(item);
		}
		else {
			item.CommandLineLength = 0;
		}
		PushItem(&info->Entry);
	}
	else {
		// process exited
		auto info = (FullItem<ProcessExitInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (info == nullptr) {
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			return;
		}

		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.ProcessId = HandleToULong(ProcessId);
		item.Size = sizeof(ProcessExitInfo);

		PushItem(&info->Entry);
	}
}
_Use_decl_annotations_
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
	AutoLock<FastMutex> lock(g_Globals.MutexNotify);
	if (g_Globals.ThreadInfo) {
		return;
	}
	auto size = sizeof(FullItem<ThreadCreateExitInfo>);
	auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_TAG);
	if (info == nullptr) {
		KdPrint((DRIVER_PREFIX "Failed to allocate memory\n"));
		return;
	}
	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.Time);
	item.Size = sizeof(item);
	item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;
	item.ProcessId = HandleToULong(ProcessId);
	item.ThreadId = HandleToULong(ThreadId);

	PushItem(&info->Entry);
}
_Use_decl_annotations_
void OnImageLoadNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
	AutoLock<FastMutex> lock(g_Globals.MutexNotify);
	if (g_Globals.ImageLoadInfo) {
		return;
	}

	if (ProcessId == nullptr) {
		// system image, ignore
		return;
	}

	auto size = sizeof(FullItem<ImageLoadInfo>);
	auto info = (FullItem<ImageLoadInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_TAG);
	if (info == nullptr) {
		KdPrint((DRIVER_PREFIX "Failed to allocate memory\n"));
		return;
	}

	::memset(info, 0, size);

	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.Time);
	item.Size = sizeof(item);
	item.Type = ItemType::ImageLoad;
	item.ProcessId = HandleToULong(ProcessId);
	item.ImageSize = ImageInfo->ImageSize;
	item.LoadAddress = ImageInfo->ImageBase;

	if (FullImageName) {
		::memcpy(item.ImageFileName, FullImageName->Buffer, min(FullImageName->Length, MaxImageFileSize * sizeof(WCHAR)));
	}
	else {
		::wcscpy_s(item.ImageFileName, L"(unknown)");
	}

	PushItem(&info->Entry);
}
NTSTATUS OnRegistreNotify(_In_ PVOID /*Context*/, _In_opt_ PVOID Argument1, _In_opt_ PVOID Argument2) {
	AutoLock<FastMutex> lock(g_Globals.MutexNotify);
	if (g_Globals.RegisterSetValueInfo) {
		return STATUS_SUCCESS;
	}
	static const WCHAR machine[] = L"\\REGISTRY\\MACHINE\\";

	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)Argument1)
	{
	case RegNtPostSetValueKey: {
		auto args = (REG_POST_OPERATION_INFORMATION*)Argument2;
		if (!NT_SUCCESS(args->Status)) {
			break;
		}

		PCUNICODE_STRING name;
		if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&g_Globals.Cookie, args->Object, nullptr, &name, 0))) {
			if (wcsncmp(name->Buffer, machine, ARRAYSIZE(machine) - 1) == 0) {

				auto preInfo = (REG_SET_VALUE_KEY_INFORMATION*)args->PreInformation;
				NT_ASSERT(preInfo);

				auto size = sizeof(FullItem<RegistrySetValueInfo>);
				auto info = (FullItem<RegistrySetValueInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_TAG);
				
				if (info == nullptr) {
					break;
				}

				RtlZeroMemory(info, size);

				auto& item = info->Data;

				item.Type = ItemType::RegistrySetValueInfo;
				item.Size = sizeof(item);
				KeQuerySystemTimePrecise(&item.Time);
				
				wcsncpy_s(item.KeyName, name->Buffer, name->Length / sizeof(WCHAR) - 1);
				wcsncpy_s(item.ValueName, preInfo->ValueName->Buffer, preInfo->ValueName->Length / sizeof(WCHAR) - 1);

				item.ProcessId = HandleToUlong(PsGetCurrentProcessId());
				item.ThreadId = HandleToUlong(PsGetCurrentThreadId());

				item.DataSize = preInfo->DataSize;
				item.DataType = preInfo->Type;
				memcpy(item.Data, preInfo->Data, min(item.DataSize, sizeof(item.Data)));

				PushItem(&info->Entry);
			}
			CmCallbackReleaseKeyObjectIDEx(name);
		}
		break;
	}
	default:
		break;
	}
	return STATUS_SUCCESS;
}

void PushItem(LIST_ENTRY* entry) {
	AutoLock<FastMutex> lock(g_Globals.MutexItem);
	if (g_Globals.ItemCount > 1024) {
		// too many items, remove oldest one
		auto head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;
		auto item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
		ExFreePool(item);
	}
	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}
