#pragma once

enum class ItemType : short {
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad,
	RegistrySetValueInfo
};

struct ItemHeader {
	ItemType Type;
	USHORT Size;
	LARGE_INTEGER Time;
};

struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
};

struct ProcessCreateInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ParentProcessId;
	USHORT CommandLineLength;
	USHORT CommandLineOffset;
};

struct ThreadCreateExitInfo : ItemHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};

const int MaxImageFileSize = 300;

struct ImageLoadInfo : ItemHeader {
	ULONG ProcessId;
	void* LoadAddress;
	ULONG_PTR ImageSize;
	WCHAR ImageFileName[MaxImageFileSize + 1];
};

struct RegistrySetValueInfo : ItemHeader
{
	ULONG ProcessId;
	ULONG ThreadId;
	WCHAR KeyName[256];
	WCHAR ValueName[64];
	ULONG DataType;
	UCHAR Data[128];
	ULONG DataSize;
};

#define IOCTL_PROCESS_INFO CTL_CODE(0x8000,0x800,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_THREAD_INFO CTL_CODE(0x8000,0x801,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_IMAGELOAD_INFO CTL_CODE(0x8000,0x802,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_REGISTERSETVALUE_INFO CTL_CODE(0x8000,0x803,METHOD_NEITHER,FILE_ANY_ACCESS)

#define IOCTL_UNBLOCKPROCESS_INFO CTL_CODE(0x8000,0x804,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_UNBLOCKTHREAD_INFO CTL_CODE(0x8000,0x805,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_UNBLOCKIMAGELOAD_INFO CTL_CODE(0x8000,0x806,METHOD_NEITHER,FILE_ANY_ACCESS)
#define IOCTL_UNBLOCKREGISTERSETVALUE_INFO CTL_CODE(0x8000,0x807,METHOD_NEITHER,FILE_ANY_ACCESS)