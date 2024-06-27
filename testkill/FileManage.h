#ifndef _FILE_MANAGE_H_
#define _FILE_MANAGE_H


#include <ntifs.h>
#include <ntstatus.h>

// ע��: ·������Ҫ�� \??\ ��Ϊǰ׺


// ��������ļ�
BOOLEAN MyCreateFile(UNICODE_STRING ustrFilePath);

// ��ȡ�ļ���С
ULONG64 MyGetFileSize(UNICODE_STRING ustrFileName);

// �����ļ���������
BOOLEAN MyHideFile(UNICODE_STRING ustrFileName);

// �����ļ��к��ļ�
BOOLEAN MyQueryFileAndFileFolder(UNICODE_STRING ustrPath);

//�����ļ����ļ��в�ɾ���ļ�
BOOLEAN MyQueryFileAndFileFolderThenDel(UNICODE_STRING rootDir);

//�����ļ����ļ��� ֹͣexe ��ɾ���ļ�
BOOLEAN MyQueryFileAndFileFolderThenStopProcThenDel(UNICODE_STRING rootDir);

// ��ȡ�ļ�����
BOOLEAN MyReadFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pReadData, PULONG pulReadDataSize);

// ���ļ�д������
BOOLEAN MyWriteFile(UNICODE_STRING ustrFileName, LARGE_INTEGER liOffset, PUCHAR pWriteData, PULONG pulWriteDataSize);


#endif