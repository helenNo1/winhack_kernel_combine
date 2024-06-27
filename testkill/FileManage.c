#include "FileManage.h"
#include "IrpFile.h"
#include "ForceDelete.h"
#include "ForceKillProcess.h"
#include <ntstatus.h>


static VOID ShowError(CHAR* lpszText, NTSTATUS status)
{
	DbgPrint("%s Error! Error Code: 0x%08X\n", lpszText, status);
}


// 创建或者打开文件
BOOLEAN MyCreateFile(UNICODE_STRING ustrFilePath)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 创建或者打开文件
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFilePath, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 关闭句柄
	ObDereferenceObject(hFile);
	return TRUE;
}


// 获取文件大小
ULONG64 MyGetFileSize(UNICODE_STRING ustrFileName)
{
	PFILE_OBJECT hFile = NULL;
	OBJECT_ATTRIBUTES objectAttributes = { 0 };
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	FILE_STANDARD_INFORMATION fsi = { 0 };

	// 获取文件句柄
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFileName, &iosb, NULL, 0,
		FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return 0;
	}

	// 获取文件大小信息
	status = IrpQueryInformationFile(hFile, &iosb, &fsi, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
	if (!NT_SUCCESS(status))
	{
		ObDereferenceObject(hFile);
		ShowError("IrpQueryInformationFile", status);
		return 0;
	}

	// 关闭句柄
	ObDereferenceObject(hFile);
	return fsi.EndOfFile.QuadPart;
}


// 设置文件隐藏属性
BOOLEAN MyHideFile(UNICODE_STRING ustrFileName)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	FILE_BASIC_INFORMATION fileBaseInfo = { 0 };

	// 设置源文件信息并获取句柄
	status = IrpCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ustrFileName,
		&iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE |
		FILE_SHARE_DELETE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 利用IrpSetInformationFile来设置文件信息
	RtlZeroMemory(&fileBaseInfo, sizeof(fileBaseInfo));
	fileBaseInfo.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
	status = IrpSetInformationFile(hFile, &iosb, &fileBaseInfo, sizeof(fileBaseInfo), FileBasicInformation);
	if (!NT_SUCCESS(status))
	{
		ObDereferenceObject(hFile);
		ShowError("IrpSetInformationFile", status);
		return FALSE;
	}

	// 关闭句柄
	ObDereferenceObject(hFile);
	return TRUE;
}


// 遍历文件夹和文件
BOOLEAN MyQueryFileAndFileFolder(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 获取文件句柄
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 遍历文件
	// 注意此处的大小!!!一定要申请足够内存，否则后面ExFreePool会蓝屏
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// 保存pDir的首地址，用来释放内存使用!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// 获取信息
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// 遍历
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// 判断是否是上级目录或是本目录
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // 加上额外的 NULL 终止字符
			// 分配目标缓冲区
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// 处理内存分配失败的情况
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// 设置新的长度和最大长度
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// 拷贝第一个字符串到目标缓冲区
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// 将反斜杠字符拷贝到缓冲区中
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// 追加第二个字符串到目标缓冲区
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// 目录
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//递归目录完整路径
				MyQueryFileAndFileFolder(ConcatenatedPathStr);
			}
			else
			{
				// 文件
				DbgPrint("[FILE]\t%wZ\n", &ConcatenatedPathStr);
			}
		}
		// 遍历完毕
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDir指向的地址改变了，所以下面ExFreePool(pDir)会出错！！！所以，必须保存首地址
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// 释放内存, 关闭文件句柄
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}

// 遍历文件夹和文件
BOOLEAN MyQueryFileAndFileFolderThenDel(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 获取文件句柄
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 遍历文件
	// 注意此处的大小!!!一定要申请足够内存，否则后面ExFreePool会蓝屏
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// 保存pDir的首地址，用来释放内存使用!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// 获取信息
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// 遍历
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// 判断是否是上级目录或是本目录
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // 加上额外的 NULL 终止字符
			// 分配目标缓冲区
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// 处理内存分配失败的情况
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// 设置新的长度和最大长度
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// 拷贝第一个字符串到目标缓冲区
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// 将反斜杠字符拷贝到缓冲区中
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// 追加第二个字符串到目标缓冲区
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// 目录
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//递归目录完整路径
				MyQueryFileAndFileFolderThenDel(ConcatenatedPathStr);
			}
			else
			{
				// 文件
				DbgPrint("[TODO DELETE FILE]\t%wZ\n", &ConcatenatedPathStr);
				ForceDeleteFile(ConcatenatedPathStr);
			}
		}
		// 遍历完毕
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDir指向的地址改变了，所以下面ExFreePool(pDir)会出错！！！所以，必须保存首地址
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// 释放内存, 关闭文件句柄
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}

static BOOLEAN IsExeFile(UNICODE_STRING* FileName) {
	// 定义 .exe 后缀
	UNICODE_STRING ExeSuffix = RTL_CONSTANT_STRING(L".exe");

	// 文件名长度需要至少和 .exe 一样长
	if (FileName->Length < ExeSuffix.Length) {
		return FALSE;
	}

	// 文件名后缀部分指针
	PWCHAR FileNameSuffix = FileName->Buffer + (FileName->Length / sizeof(WCHAR)) - (ExeSuffix.Length / sizeof(WCHAR));

	// 比较后缀部分
	if (RtlCompareMemory(FileNameSuffix, ExeSuffix.Buffer, ExeSuffix.Length) == ExeSuffix.Length) {
		return TRUE;
	}

	return FALSE;
}

// 遍历文件夹和文件
BOOLEAN MyQueryFileAndFileFolderThenStopProcThenDel(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 获取文件句柄
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 遍历文件
	// 注意此处的大小!!!一定要申请足够内存，否则后面ExFreePool会蓝屏
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// 保存pDir的首地址，用来释放内存使用!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// 获取信息
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// 遍历
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// 判断是否是上级目录或是本目录
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // 加上额外的 NULL 终止字符
			// 分配目标缓冲区
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// 处理内存分配失败的情况
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// 设置新的长度和最大长度
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// 拷贝第一个字符串到目标缓冲区
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// 将反斜杠字符拷贝到缓冲区中
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// 追加第二个字符串到目标缓冲区
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// 目录
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//递归目录完整路径
				MyQueryFileAndFileFolderThenStopProcThenDel(ConcatenatedPathStr);
			}
			else
			{
				//判断是否是exe文件并结束进程
				if (IsExeFile(&ustrTemp)) {
					HANDLE processId;
					status = GetProcessIdByName(ustrTemp, &processId);
					if (NT_SUCCESS(status)) {
						DbgPrint("Process %ws found with PID: %d\n", ustrTemp, processId);
						// 强制结束指定进程
						ForceKillProcess(processId);
					}
					else {
						DbgPrint("Process %ws not found. Status: 0x%x\n", ustrTemp, status);
						//break;
					}
				}

				// 文件
				DbgPrint("[TODO DELETE FILE]\t%wZ\n", &ConcatenatedPathStr);
				ForceDeleteFile(ConcatenatedPathStr);
			}
		}
		// 遍历完毕
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDir指向的地址改变了，所以下面ExFreePool(pDir)会出错！！！所以，必须保存首地址
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// 释放内存, 关闭文件句柄
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}


// 读取文件数据
BOOLEAN MyReadFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pReadData, PULONG pulReadDataSize)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 打开文件
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFileName, &iosb, NULL,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
		FILE_NO_INTERMEDIATE_BUFFERING | FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 读取文件数据
	RtlZeroMemory(&iosb, sizeof(iosb));
	status = IrpReadFile(hFile, &iosb,
		pReadData, *pulReadDataSize, &liOffset);
	if (!NT_SUCCESS(status))
	{
		*pulReadDataSize = iosb.Information;
		ObDereferenceObject(hFile);
		ShowError("IrpReadFile", status);
		return FALSE;
	}

	// 获取实际读取的数据
	*pulReadDataSize = iosb.Information;

	// 关闭句柄
	ObDereferenceObject(hFile);

	return TRUE;
}


// 向文件写入数据
BOOLEAN MyWriteFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pWriteData, PULONG pulWriteDataSize)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// 打开文件
	status = IrpCreateFile(&hFile, GENERIC_WRITE, &ustrFileName, &iosb, NULL,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN_IF,
		FILE_NO_INTERMEDIATE_BUFFERING | FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// 写入文件数据
	RtlZeroMemory(&iosb, sizeof(iosb));
	status = IrpWriteFile(hFile, &iosb,
		pWriteData, *pulWriteDataSize, &liOffset);
	if (!NT_SUCCESS(status))
	{
		*pulWriteDataSize = iosb.Information;
		ObDereferenceObject(hFile);
		ShowError("IrpWriteFile", status);
		return FALSE;
	}

	// 获取实际写入的数据
	*pulWriteDataSize = iosb.Information;

	// 关闭句柄
	ObDereferenceObject(hFile);

	return TRUE;
}
