#include "SecureBuffer.h"
#include <sysinfoapi.h>
#include <memoryapi.h>

// ================================
// 类名: SecureBuffer
// 描述: 用于存储敏感数据的安全缓冲区类
// ================================

// 函数：SecureBuffer
// 描述：构造函数，初始化缓冲区,分配时直接对齐到系统页
SecureBuffer::SecureBuffer(size_t requestedSize)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	size_t pageSize = sysInfo.dwPageSize;
	capacity = AlignUp(requestedSize, pageSize);
	used = 0;
	buffer = (BYTE*)VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	flagProtect = PAGE_READWRITE;
	// 计算默认分配对齐字节（这里使用系统指针大小作为对齐值）
	alignBytes = sizeof(void*);	// 一般为4或8字节
}

// 函数：~SecureBuffer
// 描述：析构函数，释放缓冲区
SecureBuffer::~SecureBuffer()
{
	// 释放缓冲区内存
	if (buffer)
	{
		// 在释放前清零缓冲区
		SecureZeroAll();
		// 
		VirtualFree(buffer, 0, MEM_RELEASE);
		buffer = nullptr;
	}
}

// 函数: Allocate
// 描述：其他函数向安全缓冲区申请内存
BufferInfo SecureBuffer::Allocate(size_t size)
{
	BufferInfo info = { nullptr, 0 };
	// 注意：为了内存对齐，实际申请空间可能会大于请求的大小
	size_t alignedSize = AlignUp(size, alignBytes);
	if (used + alignedSize > capacity)
	{
		return info; // 超出容量，返回空信息
	}
	// 返回分配信息(包括指针和大小，但是大小为请求的大小)
	info.buffer = buffer + used;
	info.size = size;
	used += size;
	return info;
}

// 函数: SecureZero
// 描述: 安全对缓冲区内指定区域进行清零
VOID SecureBuffer::SecureZero(size_t offset, size_t size)
{
	if (offset + size > capacity)
	{
		return; // 超出容量，直接返回
	}
	// 这里不应该使用 for 循环和 memset，因为编译器可能会优化掉这些操作
	// 应该使用专门 RtlSecureZeroMemory 函数
	RtlSecureZeroMemory(buffer + offset, size);
	
}

// 函数: SecureZeroAll
// 描述: 安全对整个缓冲区进行清零
VOID SecureBuffer::SecureZeroAll()
{
	SecureZero(0, capacity);
}

// 函数: Protect
// 描述: 设置缓冲区保护标志
BOOL SecureBuffer::Protect(DWORD protectFlag)
{
	if (!buffer)
	{
		return FALSE; // 缓冲区未分配，返回FALSE
	}
	DWORD oldProtect;
	BOOL result = VirtualProtect(buffer, capacity, protectFlag, &oldProtect);
	if (result)
	{
		flagProtect = protectFlag;
	}
	return result;
}

// 函数: SetReadOnly
// 描述: 将缓冲区设置为只读
BOOL SecureBuffer::SetReadOnly()
{
	return Protect(PAGE_READONLY);
}

// 函数: SetReadWrite
// 描述: 将缓冲区设置为读写
BOOL SecureBuffer::SetReadWrite()
{
	return Protect(PAGE_READWRITE);
}

// 函数: SetNoAccess
// 描述: 将缓冲区设置为不可访问
BOOL SecureBuffer::SetNoAccess()
{
	return Protect(PAGE_NOACCESS);
}


