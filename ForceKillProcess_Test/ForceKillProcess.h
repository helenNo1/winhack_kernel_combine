#ifndef _FORCE_KILL_PROCESS_H_
#define _FORCE_KILL_PROCESS_H_


#include <ntifs.h>


// ���� SYSTEM_PROCESS_INFORMATION �ṹ
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER Reserved[3];
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    PVOID Reserved2;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR Reserved3;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    PVOID Reserved5;
    SIZE_T QuotaPagedPoolUsage;
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER Reserved7[6];
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    SystemProcessorInformation = 1,
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3,
    SystemProcessInformation = 5,
    // Other enum values
} SYSTEM_INFORMATION_CLASS;

// ���� ZwQuerySystemInformation ����
NTSTATUS ZwQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

NTSTATUS GetProcessIdByName(UNICODE_STRING processName, PHANDLE processId);

// ǿ�ƽ���ָ������
NTSTATUS ForceKillProcess(HANDLE hProcessId);

// ��ȡ PspTerminateThreadByPointer ������ַ
PVOID GetPspLoadImageNotifyRoutine();

// �����������ȡ PspTerminateThreadByPointer �����ַ
PVOID SearchPspTerminateThreadByPointer(PUCHAR pSpecialData, ULONG ulSpecialDataSize);

// ָ���ڴ������������ɨ��
PVOID SearchMemory(PVOID pStartAddress, PVOID pEndAddress, PUCHAR pMemoryData, ULONG ulMemoryDataSize);


#endif