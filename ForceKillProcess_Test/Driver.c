#include "ForceKillProcess.h"
#include "FileProtect.h"
#include "Driver.h"
#include "ForceDelete.h"
#include "FileManage.h"



PFILE_OBJECT g_pFileObject = NULL;

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	DbgPrint("Enter DriverUnload\n");

	// 关闭保护文件
	if (g_pFileObject)
	{
		UnprotectFile(g_pFileObject);
	}

	if (pDriverObject->DeviceObject)
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}
	UNICODE_STRING ustrSymName;
	RtlInitUnicodeString(&ustrSymName, SYM_NAME);
	IoDeleteSymbolicLink(&ustrSymName);

	DbgPrint("Leave DriverUnload\n");
}


NTSTATUS DriverDefaultHandle(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	DbgPrint("Enter DriverDefaultHandle\n");
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	DbgPrint("Leave DriverDefaultHandle\n");
	return status;
}

NTSTATUS DriverControlHandle(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	DbgPrint("Enter DriverControlHandle\n");
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
	ULONG ulInputLen = pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
	ULONG ulOutputLen = pIoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG ulControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
	PVOID pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	ULONG ulInfo = 0;

	switch (ulControlCode)
	{
	case IOCTL_TEST:
	{
		break;
	}
	default:
		break;
	}

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = ulInfo;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	DbgPrint("Leave DriverControlHandle\n");
	return status;
}

NTSTATUS CreateDevice(PDRIVER_OBJECT pDriverObject)
{
	DbgPrint("Enter CreateDevice\n");
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT pDevObj = NULL;
	UNICODE_STRING ustrDevName, ustrSymName;
	RtlInitUnicodeString(&ustrDevName, DEV_NAME);
	RtlInitUnicodeString(&ustrSymName, SYM_NAME);

	status = IoCreateDevice(pDriverObject, 0, &ustrDevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice Error[0x%X]\n", status);
		return status;
	}

	status = IoCreateSymbolicLink(&ustrSymName, &ustrDevName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateSymbolicLink Error[0x%X]\n", status);
		return status;
	}

	DbgPrint("Leave CreateDevice\n");
	return status;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
{
	DbgPrint("Enter DriverEntry\n");

	NTSTATUS status = STATUS_SUCCESS;
	pDriverObject->DriverUnload = DriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDefaultHandle;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDefaultHandle;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverControlHandle;

	/*
	for (ULONG i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		pDriverObject->MajorFunction[i] = DriverDefaultHandle;
	}*/



	status = CreateDevice(pDriverObject);

	// 保护文件
	/*
	UNICODE_STRING protectFileName;
	RtlInitUnicodeString(&protectFileName, L"C:\\Windows\\System32\\testkill.sys");
	g_pFileObject = ProtectFile(protectFileName);
	*/
	
	/*
	PCWSTR huorongProcessName[3] = { L"HipsDaemon.exe" , L"HipsTray.exe",L"wsctrlsvc.exe" };
	//强制结束进程
	HANDLE processId;
	LARGE_INTEGER interval;
	interval.QuadPart = -30 * 1000 * 1000 * 10; // 10 seconds in 100-nanosecond intervals
	// 
	//while (1) {
	for (int killcount = 0; killcount < 3; killcount++) {
		for (int i = 0; i < 3; i++) {
			status = GetProcessIdByName(huorongProcessName[i], &processId);
			if (NT_SUCCESS(status)) {
				DbgPrint("Process %ws found with PID: %d\n", huorongProcessName[i], processId);
				// 强制结束指定进程
				ForceKillProcess(processId);
			}
			else {
				DbgPrint("Process %ws not found. Status: 0x%x\n", huorongProcessName[i], status);
				break;
			}
		}
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
	*/
	
	
	/*
	UNICODE_STRING forceDelFileName;
	RtlInitUnicodeString(&forceDelFileName, L"C:\\Program Files (x86)\\Huorong\\Sysdiag\\bin\\wsctrlsvc.exe");
	status = ForceDeleteFile(forceDelFileName);
	//RtlInitUnicodeString(&forceDelFileName, L"C:\\Program Files (x86)\\Huorong\\Sysdiag\\bin\\HipsDaemon.exe");
	RtlInitUnicodeString(&forceDelFileName, L"C:\\Program Files (x86)\\Huorong\\Sysdiag\\bin\\HipsMain.exe");
	status = ForceDeleteFile(forceDelFileName);
	*/

	//强制删除火绒文件夹
	UNICODE_STRING forceDelDir;
	RtlInitUnicodeString(&forceDelDir, L"C:\\Program Files (x86)\\Huorong");
	MyQueryFileAndFileFolderThenStopProcThenDel(forceDelDir);

	DbgPrint("Leave DriverEntry\n");
	return status;
}



