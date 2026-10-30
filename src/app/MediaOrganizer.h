#pragma once
#include "AppBuffer.h"
// ================================
// 类名: MediaOrganizer
// 描述: 基于内存池的树状多媒体文件组织器
// ================================
class MediaOrganizer
{
private:
	// ========================
	// 节点定义
	// ========================

	// 结构: FileNode
	// 描述: 文件节点
	struct FileNode
	{
		FileNode* next;		// 后指针
		LPCSTR file;		// 文件名指针
		LONG num;			// 搭载数字
	};
	// 结构: NameNode
	// 描述: 名称节点
	struct NameNode
	{
		NameNode* next;		// 后指针
		FileNode* filehead;	// 文件节点头
		LPCSTR name;		// 名称指针
		DWORD len;			// 名称长度
		DWORD count;		// 文件数量
	};

	// ========================
	// 变量定义
	// ========================
	AppBuffer* secbuf;		// 线性分配池（应用侧）
	NameNode* namehead;		// 名称节点头
	DWORD count;			// 名称节点数量
	CHAR NULLSTR[32] = "";// u8"***空白前缀***";		// 空字符串
	// ========================
	// 方法定义
	// ========================

	// 函数: SaveFullname
	// 功能: 保存完整名称
	LPCSTR SaveFullname(LPCSTR lpFullname);
	// 函数: SaveSignname
	// 功能: 保存标识名称
	LPCSTR SaveSignname(LPCSTR lpFullname, DWORD dwOffset, DWORD dwLen);
	// 函数: InsertFileNode
	// 功能: 插入文件节点，如果数字最小，替换掉头文件
	FileNode* InsertFilenode(FileNode* head,FileNode* oper);
public:
	// 函数: MediaOrganizer
	// 功能: 构造基于内存池的树状多媒体文件组织器
	MediaOrganizer(DWORD size = MAX_PATH * 3 * 2 * 256,BOOL BytesGenMode = TRUE);
	// 函数: ~MediaOrganizer
	// 功能: 析构对象
	~MediaOrganizer();
	// 函数: SaveFilename
	// 功能: 保存文件名 (文件名解析在此处执行)
	BOOL SaveFilename(LPCSTR lpFilename);
	// 函数: GetNamenum
	// 功能: 获取名称数量
	DWORD GetNamenum();
	// 函数: GetFilenumByName
	// 功能: 获取指定名称下的文件数量
	DWORD GetFilenumByName(DWORD index);
	// 函数: GetName
	// 功能: 获取指定名称
	LPCSTR GetName(DWORD index);
	// 函数: GetFilename
	// 功能: 获取指定名称下的指定文件的完整文件名
	LPCSTR GetFilename(DWORD nameindex,DWORD fileindex);
};