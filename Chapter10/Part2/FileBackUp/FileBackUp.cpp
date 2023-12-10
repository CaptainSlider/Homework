/*++

Module Name:

    FileBackUp.c

Abstract:

    This is the main module of the FileBackUp miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include "Mutex.h"
#include "FileBackUp.h"
#include "FileNameInformation.h"
#include "AutoLock.h"
#include <intsafe.h>
#include "FileBackUpCommon.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/*************************************************************************
    Global values
*************************************************************************/

PFLT_PORT ServerPort;
PFLT_PORT SendClientPort;

/*************************************************************************
    Prototypes
*************************************************************************/

bool IsBackUpDirectory(PUNICODE_STRING directory);
NTSTATUS BackUpFile(PUNICODE_STRING FileName, PCFLT_RELATED_OBJECTS FltObject);
ULONG GetMaxBackUpSize();

EXTERN_C_START

NTSTATUS PortConnectNotify(
    IN PFLT_PORT ClientPort,
    IN PVOID,
    IN PVOID,
    IN ULONG,
    OUT PVOID*
);

void PortDisconnectNotify(
    IN PVOID
);

NTSTATUS PortMessageNotify(
    _In_opt_ PVOID PortCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FileBackUpInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
FileBackUpInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
FileBackUpInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
FileBackUpUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
FileBackUpInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FileBackUpPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

VOID
FileBackUpOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    );

FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FileBackUpPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

BOOLEAN
FileBackUpDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    );

FLT_PREOP_CALLBACK_STATUS
FileBackUpPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObject, PVOID* CompletionContext);
EXTERN_C_END

FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObject, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);

FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostCleanUp(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObject, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);
//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FileBackUpUnload)
#pragma alloc_text(PAGE, FileBackUpInstanceQueryTeardown)
#pragma alloc_text(PAGE, FileBackUpInstanceSetup)
#pragma alloc_text(PAGE, FileBackUpInstanceTeardownStart)
#pragma alloc_text(PAGE, FileBackUpInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_CONTEXT_REGISTRATION Contexts[] = {
    {FLT_FILE_CONTEXT, 0, nullptr, sizeof(FileContext), DRIVER_PREFIX_CONTEXT},
    {FLT_CONTEXT_END}
};

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {IRP_MJ_WRITE, FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO, FileBackUpPreWrite},
    {IRP_MJ_CREATE, 0, nullptr, FileBackUpPostCreate},
    {IRP_MJ_CLEANUP, 0, nullptr, FileBackUpPostCleanUp},

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    Contexts,                               //  Context
    Callbacks,                          //  Operation callbacks

    FileBackUpUnload,                           //  MiniFilterUnload

    FileBackUpInstanceSetup,                    //  InstanceSetup
    FileBackUpInstanceQueryTeardown,            //  InstanceQueryTeardown
    FileBackUpInstanceTeardownStart,            //  InstanceTeardownStart
    FileBackUpInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};

ULONG GetMaxBackUpSize() {
    OBJECT_ATTRIBUTES ObjectAttributes;   
    UNICODE_STRING RegisterKeyName = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\FileBackUp");
    UNICODE_STRING valueName = RTL_CONSTANT_STRING(L"BackUpSize");
    HANDLE hRegisterKey = nullptr;
    ULONG bufferLenghtNeeded = 0;
    ULONG bufferLenght = 0;
    PKEY_VALUE_PARTIAL_INFORMATION RegisterData = nullptr;

    InitializeObjectAttributes(&ObjectAttributes, &RegisterKeyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    auto status = ZwOpenKey(&hRegisterKey, KEY_READ, &ObjectAttributes);
    if (!NT_SUCCESS(status)) {
        KdPrint(("faild to open RegisterKey (0x%08X)", status));
        return 0;
    }

    status = ZwQueryValueKey(hRegisterKey, &valueName, KeyValuePartialInformation, nullptr, bufferLenght, &bufferLenghtNeeded);
    if ((status == STATUS_BUFFER_OVERFLOW) || (status == STATUS_BUFFER_TOO_SMALL)) {
        bufferLenght = bufferLenghtNeeded;

        RegisterData = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferLenght, DRIVER_PREFIX);
        if (RegisterData == nullptr)
        {
            return 0;
        }

        status = ZwQueryValueKey(hRegisterKey, &valueName, KeyValuePartialInformation, RegisterData, bufferLenght, &bufferLenghtNeeded);
        if ((status != STATUS_SUCCESS) || (bufferLenght != bufferLenghtNeeded) || (NULL == RegisterData))
        {
            ExFreePool(RegisterData);
            return 0;
        }

    }
    ULONG limit = static_cast<ULONG>(RegisterData->Data[0]);
    ExFreePool(RegisterData);
    ZwClose(hRegisterKey);

    return limit;
}
NTSTATUS RecordingTime(PUNICODE_STRING FileName, PCFLT_RELATED_OBJECTS FltObject) {
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE hTimeStream;
    IO_STATUS_BLOCK ioBlock;
    do
    {
        UNICODE_STRING TimeStream;
        const WCHAR StreamName[] = L":time";
        TimeStream.MaximumLength = FileName->Length + sizeof(StreamName);
        TimeStream.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, TimeStream.MaximumLength, DRIVER_PREFIX);
        if (TimeStream.Buffer == nullptr) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlCopyUnicodeString(&TimeStream, FileName);
        RtlAppendUnicodeToString(&TimeStream, StreamName);

        OBJECT_ATTRIBUTES attrTimeStream;
        InitializeObjectAttributes(&attrTimeStream, &TimeStream, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        status = FltCreateFile(
            FltObject->Filter,
            FltObject->Instance,
            &hTimeStream,
            GENERIC_WRITE | SYNCHRONIZE,
            &attrTimeStream,
            &ioBlock,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            0,
            FILE_OVERWRITE_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0,
            0
        );
        ExFreePool(TimeStream.Buffer);

        if (!NT_SUCCESS(status)) {
            break;
        }

        LARGE_INTEGER systemTime, localTime;
        TIME_FIELDS time;
        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &localTime);
        UNICODE_STRING FullTime;
        UNICODE_STRING BufferTime;
        RtlZeroMemory(&FullTime, sizeof(UNICODE_STRING));
        RtlZeroMemory(&BufferTime, sizeof(UNICODE_STRING));
        FullTime.Buffer = (PWCH)(ExAllocatePool2(POOL_FLAG_PAGED, 48, DRIVER_PREFIX));
        BufferTime.Buffer = (PWCH)(ExAllocatePool2(POOL_FLAG_PAGED, 16, DRIVER_PREFIX));

        FullTime.MaximumLength = 48;
        BufferTime.MaximumLength = 16;
 
        RtlZeroMemory(FullTime.Buffer, FullTime.MaximumLength);
        RtlZeroMemory(BufferTime.Buffer, BufferTime.MaximumLength);

        RtlTimeToTimeFields(&localTime, &time);
        RtlInt64ToUnicodeString((ULONGLONG)time.Hour, 0, &FullTime);

        RtlAppendUnicodeToString(&FullTime,L":");

        RtlInt64ToUnicodeString((ULONGLONG)time.Minute, 0, &BufferTime);
        RtlAppendUnicodeStringToString(&FullTime, &BufferTime);

        RtlAppendUnicodeToString(&FullTime, L":");

        RtlInt64ToUnicodeString((ULONGLONG)time.Second, 0, &BufferTime);
        RtlAppendUnicodeStringToString(&FullTime, &BufferTime);

        status = ZwWriteFile(
            hTimeStream,
            nullptr,
            nullptr,
            nullptr,
            &ioBlock,
            FullTime.Buffer,
            FullTime.MaximumLength,
            NULL,
            nullptr
        );
        ExFreePool(FullTime.Buffer);
        ExFreePool(BufferTime.Buffer);
        if (!NT_SUCCESS(status)) {
            break;
        }
        if (hTimeStream) {
            FltClose(hTimeStream);
        }
    } while (false);
    
    return status;
}
NTSTATUS BackUpFile(PUNICODE_STRING FileName, PCFLT_RELATED_OBJECTS FltObject) {

    NTSTATUS status = STATUS_SUCCESS;
    HANDLE hSourceFile = nullptr;
    HANDLE hTargetFile = 0;
    IO_STATUS_BLOCK ioBlock;
    void* buffer = nullptr;
    ULONG maxSize = GetMaxBackUpSize(); //Get size in Mb
    LARGE_INTEGER fileSize;
    status = FsRtlGetFileSize(FltObject->FileObject, &fileSize);
    if (!NT_SUCCESS(status) || fileSize.QuadPart== 0 || fileSize.QuadPart > (maxSize << 20)) {
        return status;
    }

    do
    {
        OBJECT_ATTRIBUTES attrSourceFile;
        InitializeObjectAttributes(&attrSourceFile, FileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

        status = FltCreateFile(
            FltObject->Filter,
            FltObject->Instance,
            &hSourceFile,
            FILE_READ_DATA | SYNCHRONIZE,
            &attrSourceFile,
            &ioBlock,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0,
            IO_IGNORE_SHARE_ACCESS_CHECK
        );
        
        if (!NT_SUCCESS(status)) {
            break;
        }

        UNICODE_STRING TargetName;
        const WCHAR backUpStream[] = L":backup";
        TargetName.MaximumLength = FileName->Length + sizeof(backUpStream);
        TargetName.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, TargetName.MaximumLength, DRIVER_PREFIX);
        if (TargetName.Buffer == nullptr) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyUnicodeString(&TargetName, FileName);
        RtlAppendUnicodeToString(&TargetName, backUpStream);

        OBJECT_ATTRIBUTES attrTargetFile;
        InitializeObjectAttributes(&attrTargetFile, &TargetName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

        status = FltCreateFile(
            FltObject->Filter,
            FltObject->Instance,
            &hTargetFile,
            GENERIC_WRITE | SYNCHRONIZE,
            &attrTargetFile,
            &ioBlock,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            0,
            FILE_OVERWRITE_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0,
            0      
        );

        ExFreePool(TargetName.Buffer);

        if (!NT_SUCCESS(status)) {
            break;
        }

        ULONG size = 1 << 21; //2MB
        buffer = ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_PREFIX); 
        if (buffer == nullptr) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        ULONG bytes;
        auto saveSize = fileSize;
        while (fileSize.QuadPart > 0) {
            status = ZwReadFile(
                hSourceFile,
                nullptr,
                nullptr,
                nullptr,
                &ioBlock,
                buffer,
                (ULONG)min(fileSize.QuadPart, (LONGLONG)size),
                NULL,
                nullptr
            );

            if (!NT_SUCCESS(status)) {
                break;
            }

            bytes = (ULONG)ioBlock.Information;

            status = ZwWriteFile(
                hTargetFile,
                nullptr,
                nullptr,
                nullptr,
                &ioBlock,
                buffer,
                bytes,
                NULL,
                nullptr
            );

            if (!NT_SUCCESS(status)) {
                break;
            }
            fileSize.QuadPart -= bytes;
        }
        FILE_END_OF_FILE_INFORMATION info;
        info.EndOfFile = saveSize;
        NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hTargetFile, &ioBlock, &info, sizeof(info), FileEndOfFileInformation)));

    } while (false);

    if (hTargetFile) {
        FltClose(hTargetFile);
    }
    if (hSourceFile) {
        FltClose(hSourceFile);
    }
    if (buffer) {
        ExFreePool(buffer);
    }
    return status;
}

bool IsBackUpDirectory(PUNICODE_STRING directory) {

    auto maxSize = 1000;
    if (directory->Length > maxSize) {
        return false;
    }

    auto copy = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, maxSize + sizeof(WCHAR), DRIVER_PREFIX);
    if (!copy) {
        return false;
    }

    wcsncpy_s(copy, (maxSize + 1) / sizeof(WCHAR), directory->Buffer,directory->Length / sizeof(WCHAR));

    _wcslwr(copy);

    bool isBackUp = wcsstr(copy, L"documents");

    return isBackUp;
}

FLT_PREOP_CALLBACK_STATUS FileBackUpPreWrite(PFLT_CALLBACK_DATA, PCFLT_RELATED_OBJECTS FltObject, PVOID*) {
    PFileContext context;

    auto status = FltGetFileContext(FltObject->Instance, FltObject->FileObject, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status) || context == nullptr) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    {
        AutoLock <Mutex> lock(context->mutex);
        if (!context->Written) {
            status = BackUpFile(&context->name, FltObject);
            if (!NT_SUCCESS(status)) {
                FltReleaseContext(context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            context->Written = true;
            status = RecordingTime(&context->name, FltObject);
            if (!NT_SUCCESS(status)) {
                FltReleaseContext(context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            if (SendClientPort) {
                USHORT lenName = context->name.Length;
                USHORT size = lenName + sizeof(FileBackUpMessage);

                auto msg = (FileBackUpMessage*)ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_PREFIX);
                if (msg) {
                    msg->LenghtName = lenName / sizeof(WCHAR);
                    RtlCopyMemory(msg->Name, context->name.Buffer, lenName);
                    LARGE_INTEGER timeOut;
                    timeOut.QuadPart = -10000 * 100; // 100 milsecond
                    status = FltSendMessage(gFilterHandle, &SendClientPort, msg, size, nullptr, nullptr, &timeOut);
                    ExFreePool(msg);
                }
            }
        }
    }

    FltReleaseContext(context);

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObject, PVOID, FLT_POST_OPERATION_FLAGS Flags) {

    if (Flags & FLTFL_POST_OPERATION_DRAINING) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    const auto& params = Data->Iopb->Parameters.Create;
    if (Data->RequestorMode == KernelMode || (params.SecurityContext->DesiredAccess & FILE_WRITE_DATA) == 0 || Data->IoStatus.Information == FILE_DOES_NOT_EXIST) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FilterFileNameInformation nameInfo(Data);
    if (!nameInfo) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!NT_SUCCESS(nameInfo.Parce())) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!IsBackUpDirectory(&nameInfo->ParentDir)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (nameInfo->Stream.Length > 0) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    PFileContext context;

    auto status = FltAllocateContext(FltObject->Filter, FLT_FILE_CONTEXT, sizeof(FileContext), PagedPool, (PFLT_CONTEXT *)&context);
    if (!NT_SUCCESS(status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    context->Written = false;
    context->name.MaximumLength = nameInfo->Name.Length;
    context->name.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, nameInfo->Name.Length, DRIVER_PREFIX);
    if (!context->name.Buffer) {
        FltReleaseContext(context);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    RtlCopyUnicodeString(&context->name, &nameInfo->Name);

    context->mutex.Init();

    status = FltSetFileContext(FltObject->Instance, FltObject->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, context, nullptr);
    if (!NT_SUCCESS(status)) {
        ExFreePool(context->name.Buffer);
    }
    FltReleaseContext(context);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostCleanUp(PFLT_CALLBACK_DATA, PCFLT_RELATED_OBJECTS FltObject, PVOID , FLT_POST_OPERATION_FLAGS ) {
    PFileContext context;
    auto status = FltGetFileContext(FltObject->Instance, FltObject->FileObject, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status) || context == nullptr) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (context->name.Buffer != nullptr) {
        ExFreePool(context->name.Buffer);
    }
    FltReleaseContext(context);
    FltDeleteContext(context);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS PortConnectNotify(
    IN PFLT_PORT ClientPort,
    IN PVOID,
    IN PVOID,
    IN ULONG ,
    OUT PVOID*
){
    SendClientPort = ClientPort;
    return STATUS_SUCCESS;
}
void PortDisconnectNotify(
    IN PVOID
) 
{
    FltCloseClientPort(gFilterHandle, &SendClientPort);
    SendClientPort = nullptr;
}
NTSTATUS PortMessageNotify(
    _In_opt_ PVOID ,
    _In_reads_bytes_opt_(InputBufferLength) PVOID ,
    _In_ ULONG ,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID ,
    _In_ ULONG,
    _Out_ PULONG) {
    return STATUS_SUCCESS;
}

NTSTATUS
FileBackUpInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are always created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );

    if (VolumeFilesystemType != FLT_FSTYPE_NTFS) {
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
FileBackUpInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

    If this routine is not defined in the registration structure, explicit
    detach requests via FltDetachVolume or FilterDetach will always be
    failed.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
FileBackUpInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpInstanceTeardownStart: Entered\n") );
}


VOID
FileBackUpInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS(status));

    if (!NT_SUCCESS(status)) {
        return status;
    }
    do
    {
        PSECURITY_DESCRIPTOR sd;
        status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
        if (!NT_SUCCESS(status)) {
            break;
        }
        UNICODE_STRING namePort = RTL_CONSTANT_STRING(L"\\FileBackUp");
        OBJECT_ATTRIBUTES attrPort;
        InitializeObjectAttributes(&attrPort, &namePort, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, sd);

        status = FltCreateCommunicationPort(gFilterHandle, &ServerPort, &attrPort, nullptr, PortConnectNotify, PortDisconnectNotify, PortMessageNotify, 1);
        if (!NT_SUCCESS(status)) {
            break;
        }

        status = FltStartFiltering(gFilterHandle);
    } while (false);
        
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(gFilterHandle);
    }
    

    return status;
}
NTSTATUS
FileBackUpUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpUnload: Entered\n") );
    FltCloseCommunicationPort(ServerPort);

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
FileBackUpPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpPreOperation: Entered\n") );

    //
    //  See if this is an operation we would like the operation status
    //  for.  If so request it.
    //
    //  NOTE: most filters do NOT need to do this.  You only need to make
    //        this call if, for example, you need to know if the oplock was
    //        actually granted.
    //

    if (FileBackUpDoRequestOperationStatus( Data )) {

        status = FltRequestOperationStatusCallback( Data,
                                                    FileBackUpOperationStatusCallback,
                                                    (PVOID)(++OperationStatusCtx) );
        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                          ("FileBackUp!FileBackUpPreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
                           status) );
        }
    }

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



VOID
FileBackUpOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    )
/*++

Routine Description:

    This routine is called when the given operation returns from the call
    to IoCallDriver.  This is useful for operations where STATUS_PENDING
    means the operation was successfully queued.  This is useful for OpLocks
    and directory change notification operations.

    This callback is called in the context of the originating thread and will
    never be called at DPC level.  The file object has been correctly
    referenced so that you can access it.  It will be automatically
    dereferenced upon return.

    This is non-pageable because it could be called on the paging path

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    RequesterContext - The context for the completion routine for this
        operation.

    OperationStatus -

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpOperationStatusCallback: Entered\n") );

    PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                  ("FileBackUp!FileBackUpOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                   OperationStatus,
                   RequesterContext,
                   ParameterSnapshot->MajorFunction,
                   ParameterSnapshot->MinorFunction,
                   FltGetIrpName(ParameterSnapshot->MajorFunction)) );
}


FLT_POSTOP_CALLBACK_STATUS
FileBackUpPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpPostOperation: Entered\n") );

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
FileBackUpPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackUp!FileBackUpPreOperationNoPostOperation: Entered\n") );

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
FileBackUpDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    )
/*++

Routine Description:

    This identifies those operations we want the operation status for.  These
    are typically operations that return STATUS_PENDING as a normal completion
    status.

Arguments:

Return Value:

    TRUE - If we want the operation status
    FALSE - If we don't

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

    //
    //  return boolean state based on which operations we are interested in
    //

    return (BOOLEAN)

            //
            //  Check for oplock operations
            //

             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              //
              //    Check for directy change notification
              //

              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
               (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}
