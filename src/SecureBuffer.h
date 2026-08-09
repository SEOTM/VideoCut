#pragma once
// 包含 Windows 基础类型定义
// 同时避免包含额外的 Windows 头文件
// 注意：此文件仅定义了必要的类型和宏，以减少依赖性
// 还要减少与 Windows 相关的编译时间
// 具体类型：
// DWORD, HANDLE, BOOL, LPVOID, SIZE_T, MEMORY_BASIC_INFORMATION
#include <minwindef.h>

// ================================
// 描述: 预定义常量结构和枚举
// ================================

// 结构: BufferInfo
// 描述: 缓冲区信息结构体
struct BufferInfo
{
	VOID* buffer;   // 缓冲区指针
	size_t size;      // 缓冲区大小
};

// ================================
// 类名: SecureBuffer
// 描述: 用于存储敏感数据的安全缓冲区类
// ================================
class SecureBuffer
{
private:
	BYTE* buffer;			// 指向缓冲区的指针
	size_t capacity;		// 缓冲区的容量
	size_t used;			// 已使用的字节数
	DWORD flagProtect;		// 保护标志
	size_t alignBytes;		// 默认对齐字节

	// 函数: AlignUp
	// 描述: 将大小对齐到指定的对齐值
	static size_t AlignUp(size_t size, size_t align)
	{
		return (size + align - 1) & ~(align - 1);
	}
public:
	// 函数：SecureBuffer
	// 描述：构造函数，初始化缓冲区,分配时直接对齐到系统页
	SecureBuffer(size_t requestedSize);
	// 函数：~SecureBuffer
	// 描述：析构函数，释放缓冲区
	~SecureBuffer();
	// 函数: Allocate
	// 描述：其他函数向安全缓冲区申请内存
	BufferInfo Allocate(size_t size);
	// 函数: SecureZero
	// 描述: 安全对缓冲区内指定区域进行清零
	VOID SecureZero(size_t offset, size_t size);
	// 函数: SecureZeroAll
	// 描述: 安全对整个缓冲区进行清零
	VOID SecureZeroAll();
	// 函数: Protect
	// 描述: 设置缓冲区保护标志
	BOOL Protect(DWORD protectFlag);
	// 函数: SetReadOnly
	// 描述: 将缓冲区设置为只读
	BOOL SetReadOnly();
	// 函数: SetReadWrite
	// 描述: 将缓冲区设置为读写
	BOOL SetReadWrite();
	// 函数: SetNoAccess
	// 描述: 将缓冲区设置为不可访问
	BOOL SetNoAccess();
};