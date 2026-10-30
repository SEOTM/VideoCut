#include "MediaOrganizer.h"
#include <new>
#include <cstdio>
// ================================
// 类名: MediaOrganizer
// 描述: 基于内存池的树状多媒体文件组织器
// ================================

// 函数: SaveFullname
// 功能: 保存完整名
LPCSTR MediaOrganizer::SaveFullname(LPCSTR lpFullname)
{
	LPCSTR ret = NULL;
	BufferInfo info = secbuf->Allocate(strlen(lpFullname) + 1);
	if (info.buffer)
	{
		strcpy_s((LPSTR)info.buffer, info.size, lpFullname);
		ret = (LPCSTR)info.buffer;
	}
	return ret;
}
// 函数: SaveSignname
// 功能: 保存标识名称
LPCSTR MediaOrganizer::SaveSignname(LPCSTR lpFullname, DWORD dwOffset, DWORD dwLen)
{
	LPCSTR ret = NULL;
	if (strlen(lpFullname) > dwOffset + dwLen)
	{
		BufferInfo buf = secbuf->Allocate(dwLen + 1);
		if (buf.buffer)
		{
			RtlZeroMemory(buf.buffer, buf.size);
			strncpy_s((LPSTR)buf.buffer, buf.size, lpFullname + dwOffset, dwLen);
			ret = (LPCSTR)buf.buffer;
		}
	}
	return ret;
}
// 函数: InsertFileNode
// 功能: 插入文件节点，如果数字最小，替换掉头文件
MediaOrganizer::FileNode* MediaOrganizer::InsertFilenode(FileNode* head, FileNode* oper)
{
	FileNode* ret = head;
	if (head || oper)
	{
		if (!head)
		{
			ret = oper;
		}
		else if (oper)
		{
			if (oper->num == 0xFFFFFFFFU)
			{
				oper->next = head;
				ret = oper;
			}
			else
			{
				FileNode* cur = head;
				FileNode* prev = NULL;
				while (cur)
				{
					if (oper->num < cur->num)
					{
						oper->next = cur;
						if (prev)
						{

							prev->next = oper;
							break;
						}
						else
						{
							ret = oper;
							break;
						}
					}
					prev = cur;
					cur = cur->next;
				}
				if (!oper->next)
				{
					prev->next = oper;
				}
			}
		}
	}
	else
	{
		ret = NULL;
	}
	return ret;
}

// 函数: MediaOrganizer
// 功能: 构造基于内存池的树状多媒体文件组织器
MediaOrganizer::MediaOrganizer(DWORD size, BOOL BytesGenMode)
{
	if (BytesGenMode)
	{
		secbuf = new AppBuffer(size);
	}
	else
	{
		DWORD dwSize = MAX_PATH * 6 + sizeof(NameNode) + sizeof(FileNode);	// 保守算法， UTF-8 汉字单个字符最大 3 字节
		dwSize *= size;
		secbuf = new AppBuffer(dwSize);
	}
	BufferInfo buf = secbuf->Allocate(sizeof(NameNode));
	namehead = new (buf.buffer) NameNode;
	namehead->count = 0;
	namehead->filehead = NULL;
	namehead->next = NULL;
	namehead->name = NULLSTR;
	namehead->len = 0;
	count = 1;
}
// 函数: ~MediaOrganizer
// 功能: 析构对象
MediaOrganizer::~MediaOrganizer()
{
	delete secbuf;
}
// 函数: SaveFilename
// 功能: 保存文件名 (文件名解析在此处执行)
BOOL MediaOrganizer::SaveFilename(LPCSTR lpFilename)
{
	LPCSTR save = SaveFullname(lpFilename);
	DWORD dwLen = (DWORD)strlen(save);
	DWORD dwNum = 0;
	DWORD dwDotOffset = dwLen;
	DWORD dwNameOffset = dwLen;
	DWORD dwNameLen = 0;
	BOOL ret = FALSE;
	// 第一次遍历，寻找特殊字符
	for (DWORD i = 0; i < dwLen; i++)
	{
		dwNameOffset--;
		CONST CHAR& c = save[dwNameOffset];
		if (c == '.' && dwDotOffset == dwLen)
		{
			dwDotOffset = dwNameOffset;
		}
		else if (c == '\\' || c == '/')
		{
			dwNameOffset++;
			break;
		}
	}
	// 第二次遍历，拆解文件名
	{
		dwNameLen = dwDotOffset - dwNameOffset;
		DWORD fac = 1;
		for (DWORD i = dwDotOffset - 1; i > dwNameOffset && (i != 0xFFFFFFFFU); i--)
		{
			CONST CHAR& c = save[i];
			if (c > 47 && c < 58)
			{
				dwNum += (c - 48) * fac;
				fac *= 10;
				dwNameLen--;
			}
			else
			{
				break;
			}
		}
		if (dwNameLen == dwDotOffset - dwNameOffset)
		{
			dwNum = -1;
		}
	}
	// 匹配 NAME 节点
	NameNode* cur;
	if (dwLen == 0)
	{
		cur = namehead;
	}
	else
	{
		cur = namehead->next;
		NameNode* prev = namehead;
		while (cur)
		{
			if (dwNameLen == cur->len)
			{
				if (!strncmp(cur->name, save + dwNameOffset, dwNameLen))
				{
					break;
				}
			}
			prev = cur;
			cur = cur->next;
		}
		if (!cur)
		{
			BufferInfo buf = secbuf->Allocate(sizeof(NameNode));
			LPCSTR name = SaveSignname(save, dwNameOffset, dwNameLen);
			if (buf.buffer && name)
			{
				cur = new (buf.buffer) NameNode;
				cur->filehead = NULL;
				cur->count = 0;
				cur->name = name;
				cur->len = dwNameLen;
				cur->next = NULL;
				prev->next = cur;
				count++;
			}
		}
	}
	// 构建 FileNode
	if (cur)
	{
		BufferInfo buf = secbuf->Allocate(sizeof(FileNode));
		if (buf.buffer)
		{
			FileNode* file = new (buf.buffer) FileNode;
			file->file = save;
			file->next = NULL;
			file->num = dwNum;
			cur->filehead = InsertFilenode(cur->filehead, file);
			if (cur->filehead)
			{
				cur->count++;
				ret = TRUE;
			}
		}
	}
	return ret;
}
// 函数: GetNamenum
// 功能: 获取名称数量
DWORD MediaOrganizer::GetNamenum()
{
	return count;
}
// 函数: GetFilenumByName
// 功能: 获取指定名称下的文件数量
DWORD MediaOrganizer::GetFilenumByName(DWORD index)
{
	DWORD ret = 0;
	if (index < count)
	{
		NameNode* cur = namehead->next;
		for (DWORD i = 0; i < index && cur; i++)
		{
			cur = cur->next;
		}
		if (cur)
		{
			ret = cur->count;
		}
	}
	return ret;
}
// 函数: GetName
// 功能: 获取指定名称
LPCSTR MediaOrganizer::GetName(DWORD index)
{
	LPCSTR ret = NULLSTR;
	if (index < count)
	{
		NameNode* cur = namehead->next;
		for (DWORD i = 0; i < index && cur; i++)
		{
			cur = cur->next;
		}
		if (cur)
		{
			ret = cur->name;
		}
	}
	return ret;
}
// 函数: GetFilename
// 功能: 获取指定名称下的指定文件的完整文件名
LPCSTR MediaOrganizer::GetFilename(DWORD nameindex, DWORD fileindex)
{
	LPCSTR ret = NULL;
	if (nameindex < count)
	{
		NameNode* cur = namehead->next;
		for (DWORD i = 0; i < nameindex && cur; i++)
		{
			cur = cur->next;
		}
		if (cur)
		{
			FileNode* fcur = cur->filehead;
			for (DWORD j = 0; j < fileindex && fcur; j++)
			{
				fcur = fcur->next;
			}
			if (fcur)
			{
				ret = fcur->file;
			}
		}
	}
	return ret;
}
