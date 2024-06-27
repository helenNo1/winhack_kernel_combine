#include "FileManage.h"
#include "IrpFile.h"
#include "ForceDelete.h"
#include "ForceKillProcess.h"
#include <ntstatus.h>


static VOID ShowError(CHAR* lpszText, NTSTATUS status)
{
	DbgPrint("%s Error! Error Code: 0x%08X\n", lpszText, status);
}


// �������ߴ��ļ�
BOOLEAN MyCreateFile(UNICODE_STRING ustrFilePath)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// �������ߴ��ļ�
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFilePath, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// �رվ��
	ObDereferenceObject(hFile);
	return TRUE;
}


// ��ȡ�ļ���С
ULONG64 MyGetFileSize(UNICODE_STRING ustrFileName)
{
	PFILE_OBJECT hFile = NULL;
	OBJECT_ATTRIBUTES objectAttributes = { 0 };
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	FILE_STANDARD_INFORMATION fsi = { 0 };

	// ��ȡ�ļ����
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFileName, &iosb, NULL, 0,
		FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return 0;
	}

	// ��ȡ�ļ���С��Ϣ
	status = IrpQueryInformationFile(hFile, &iosb, &fsi, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
	if (!NT_SUCCESS(status))
	{
		ObDereferenceObject(hFile);
		ShowError("IrpQueryInformationFile", status);
		return 0;
	}

	// �رվ��
	ObDereferenceObject(hFile);
	return fsi.EndOfFile.QuadPart;
}


// �����ļ���������
BOOLEAN MyHideFile(UNICODE_STRING ustrFileName)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	FILE_BASIC_INFORMATION fileBaseInfo = { 0 };

	// ����Դ�ļ���Ϣ����ȡ���
	status = IrpCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ustrFileName,
		&iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE |
		FILE_SHARE_DELETE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// ����IrpSetInformationFile�������ļ���Ϣ
	RtlZeroMemory(&fileBaseInfo, sizeof(fileBaseInfo));
	fileBaseInfo.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
	status = IrpSetInformationFile(hFile, &iosb, &fileBaseInfo, sizeof(fileBaseInfo), FileBasicInformation);
	if (!NT_SUCCESS(status))
	{
		ObDereferenceObject(hFile);
		ShowError("IrpSetInformationFile", status);
		return FALSE;
	}

	// �رվ��
	ObDereferenceObject(hFile);
	return TRUE;
}


// �����ļ��к��ļ�
BOOLEAN MyQueryFileAndFileFolder(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// ��ȡ�ļ����
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// �����ļ�
	// ע��˴��Ĵ�С!!!һ��Ҫ�����㹻�ڴ棬�������ExFreePool������
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// ����pDir���׵�ַ�������ͷ��ڴ�ʹ��!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// ��ȡ��Ϣ
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// ����
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// �ж��Ƿ����ϼ�Ŀ¼���Ǳ�Ŀ¼
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // ���϶���� NULL ��ֹ�ַ�
			// ����Ŀ�껺����
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// �����ڴ����ʧ�ܵ����
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// �����µĳ��Ⱥ���󳤶�
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// ������һ���ַ�����Ŀ�껺����
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// ����б���ַ���������������
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// ׷�ӵڶ����ַ�����Ŀ�껺����
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// Ŀ¼
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//�ݹ�Ŀ¼����·��
				MyQueryFileAndFileFolder(ConcatenatedPathStr);
			}
			else
			{
				// �ļ�
				DbgPrint("[FILE]\t%wZ\n", &ConcatenatedPathStr);
			}
		}
		// �������
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDirָ��ĵ�ַ�ı��ˣ���������ExFreePool(pDir)������������ԣ����뱣���׵�ַ
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// �ͷ��ڴ�, �ر��ļ����
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}

// �����ļ��к��ļ�
BOOLEAN MyQueryFileAndFileFolderThenDel(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// ��ȡ�ļ����
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// �����ļ�
	// ע��˴��Ĵ�С!!!һ��Ҫ�����㹻�ڴ棬�������ExFreePool������
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// ����pDir���׵�ַ�������ͷ��ڴ�ʹ��!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// ��ȡ��Ϣ
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// ����
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// �ж��Ƿ����ϼ�Ŀ¼���Ǳ�Ŀ¼
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // ���϶���� NULL ��ֹ�ַ�
			// ����Ŀ�껺����
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// �����ڴ����ʧ�ܵ����
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// �����µĳ��Ⱥ���󳤶�
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// ������һ���ַ�����Ŀ�껺����
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// ����б���ַ���������������
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// ׷�ӵڶ����ַ�����Ŀ�껺����
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// Ŀ¼
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//�ݹ�Ŀ¼����·��
				MyQueryFileAndFileFolderThenDel(ConcatenatedPathStr);
			}
			else
			{
				// �ļ�
				DbgPrint("[TODO DELETE FILE]\t%wZ\n", &ConcatenatedPathStr);
				ForceDeleteFile(ConcatenatedPathStr);
			}
		}
		// �������
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDirָ��ĵ�ַ�ı��ˣ���������ExFreePool(pDir)������������ԣ����뱣���׵�ַ
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// �ͷ��ڴ�, �ر��ļ����
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}

static BOOLEAN IsExeFile(UNICODE_STRING* FileName) {
	// ���� .exe ��׺
	UNICODE_STRING ExeSuffix = RTL_CONSTANT_STRING(L".exe");

	// �ļ���������Ҫ���ٺ� .exe һ����
	if (FileName->Length < ExeSuffix.Length) {
		return FALSE;
	}

	// �ļ�����׺����ָ��
	PWCHAR FileNameSuffix = FileName->Buffer + (FileName->Length / sizeof(WCHAR)) - (ExeSuffix.Length / sizeof(WCHAR));

	// �ȽϺ�׺����
	if (RtlCompareMemory(FileNameSuffix, ExeSuffix.Buffer, ExeSuffix.Length) == ExeSuffix.Length) {
		return TRUE;
	}

	return FALSE;
}

// �����ļ��к��ļ�
BOOLEAN MyQueryFileAndFileFolderThenStopProcThenDel(UNICODE_STRING rootDir)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// ��ȡ�ļ����
	status = IrpCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&rootDir, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// �����ļ�
	// ע��˴��Ĵ�С!!!һ��Ҫ�����㹻�ڴ棬�������ExFreePool������
	ULONG ulLength = (2 * 4096 + sizeof(FILE_BOTH_DIR_INFORMATION)) * 0x2000;
	PFILE_BOTH_DIR_INFORMATION pDir = ExAllocatePool(PagedPool, ulLength);
	// ����pDir���׵�ַ�������ͷ��ڴ�ʹ��!!!
	PFILE_BOTH_DIR_INFORMATION pBeginAddr = pDir;

	// ��ȡ��Ϣ
	status = IrpQueryDirectoryFile(hFile, &iosb, pDir, ulLength,
		FileBothDirectoryInformation, NULL);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(pDir);
		ObDereferenceObject(hFile);
		ShowError("IrpQueryDirectoryFile", status);
		return FALSE;
	}

	// ����
	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");
	WCHAR wcFileName[1024] = { 0 };
	while (TRUE)
	{
		// �ж��Ƿ����ϼ�Ŀ¼���Ǳ�Ŀ¼
		RtlZeroMemory(wcFileName, 1024);
		RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);
		RtlInitUnicodeString(&ustrTemp, wcFileName);
		if ((0 != RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE)) &&
			(0 != RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE)))
		{
			UNICODE_STRING sepaStr;
			RtlInitUnicodeString(&sepaStr, L"\\");
			ULONG NewStringLength = rootDir.Length + 2 + ustrTemp.Length + sizeof(WCHAR); // ���϶���� NULL ��ֹ�ַ�
			// ����Ŀ�껺����
			UNICODE_STRING ConcatenatedPathStr;
			ConcatenatedPathStr.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, NewStringLength, "Concat");
			if (ConcatenatedPathStr.Buffer == NULL) {
				// �����ڴ����ʧ�ܵ����
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			// �����µĳ��Ⱥ���󳤶�
			ConcatenatedPathStr.Length = 0;
			ConcatenatedPathStr.MaximumLength = (USHORT)NewStringLength;
			// ������һ���ַ�����Ŀ�껺����
			RtlCopyUnicodeString(&ConcatenatedPathStr, &rootDir);
			// ����б���ַ���������������
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &sepaStr);
			// ׷�ӵڶ����ַ�����Ŀ�껺����
			RtlAppendUnicodeStringToString(&ConcatenatedPathStr, &ustrTemp);


			if (pDir->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// Ŀ¼
				DbgPrint("[DIRECTORY]\t%wZ\n", &ConcatenatedPathStr);
				//�ݹ�Ŀ¼����·��
				MyQueryFileAndFileFolderThenStopProcThenDel(ConcatenatedPathStr);
			}
			else
			{
				//�ж��Ƿ���exe�ļ�����������
				if (IsExeFile(&ustrTemp)) {
					HANDLE processId;
					status = GetProcessIdByName(ustrTemp, &processId);
					if (NT_SUCCESS(status)) {
						DbgPrint("Process %ws found with PID: %d\n", ustrTemp, processId);
						// ǿ�ƽ���ָ������
						ForceKillProcess(processId);
					}
					else {
						DbgPrint("Process %ws not found. Status: 0x%x\n", ustrTemp, status);
						//break;
					}
				}

				// �ļ�
				DbgPrint("[TODO DELETE FILE]\t%wZ\n", &ConcatenatedPathStr);
				ForceDeleteFile(ConcatenatedPathStr);
			}
		}
		// �������
		if (0 == pDir->NextEntryOffset)
		{
			DbgPrint("\n[QUERY OVER]\n\n");
			break;
		}
		// pDirָ��ĵ�ַ�ı��ˣ���������ExFreePool(pDir)������������ԣ����뱣���׵�ַ
		pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
	}

	// �ͷ��ڴ�, �ر��ļ����
	ExFreePool(pBeginAddr);
	ObDereferenceObject(hFile);
	return TRUE;
}


// ��ȡ�ļ�����
BOOLEAN MyReadFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pReadData, PULONG pulReadDataSize)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// ���ļ�
	status = IrpCreateFile(&hFile, GENERIC_READ, &ustrFileName, &iosb, NULL,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
		FILE_NO_INTERMEDIATE_BUFFERING | FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// ��ȡ�ļ�����
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

	// ��ȡʵ�ʶ�ȡ������
	*pulReadDataSize = iosb.Information;

	// �رվ��
	ObDereferenceObject(hFile);

	return TRUE;
}


// ���ļ�д������
BOOLEAN MyWriteFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pWriteData, PULONG pulWriteDataSize)
{
	PFILE_OBJECT hFile = NULL;
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	// ���ļ�
	status = IrpCreateFile(&hFile, GENERIC_WRITE, &ustrFileName, &iosb, NULL,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN_IF,
		FILE_NO_INTERMEDIATE_BUFFERING | FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		ShowError("IrpCreateFile", status);
		return FALSE;
	}

	// д���ļ�����
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

	// ��ȡʵ��д�������
	*pulWriteDataSize = iosb.Information;

	// �رվ��
	ObDereferenceObject(hFile);

	return TRUE;
}
