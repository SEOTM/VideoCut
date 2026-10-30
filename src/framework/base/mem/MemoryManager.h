#pragma once
#include <minwindef.h>

// 函数: AlignUp
// 描述: 把大小向上对齐到指定对齐值
inline size_t AlignUp(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

struct BufferInfo
{
	VOID*	buffer;
	size_t	size;
};

class Arena
{
public:
	Arena(size_t requestedSize);
	~Arena();

	// 函数: Allocate
	// 描述: 从线性分配器分配一段内存(16 字节对齐)
	BufferInfo Allocate(size_t size);

	// 函数: Contains
	// 描述: 判断指针是否落在本分配器范围内
	BOOL Contains(LPCVOID p) const;

	// 函数: Capacity
	// 描述: 取分配器容量(向页大小对齐后的字节数)
	size_t Capacity() const;

	// 函数: Used
	// 描述: 取已使用的字节数(按对齐后占用记账)
	size_t Used() const;

	// 函数: Reset
	// 描述: 把分配器游标复位到起点(不释放底层内存)
	VOID Reset();

private:
	BYTE*	buffer;
	size_t	capacity;
	size_t	used;
	size_t	alignBytes;

	Arena(const Arena&) = delete;
	Arena& operator=(const Arena&) = delete;
};

class MemPool
{
public:
	MemPool();
	~MemPool();

	// 函数: Allocate
	// 描述: 从池中分配一段内存(请求不超过 MAX_SMALL_SIZE)
	BufferInfo Allocate(size_t size);

	// 函数: Free
	// 描述: 回收一次 Allocate 的产物(非本池指针静默忽略)
	VOID Free(BufferInfo info);

	// 函数: Free
	// 描述: 按指针回收本池分配的块
	VOID Free(VOID* ptr);

	// 函数: Contains
	// 描述: 判断指针是否属于本池
	BOOL Contains(LPCVOID p) const;

	static constexpr size_t MAX_SMALL_SIZE = 4096;

private:
	MemPool(const MemPool&) = delete;
	MemPool& operator=(const MemPool&) = delete;

	static constexpr size_t SLAB_SIZE = 64 * 1024;
	static constexpr size_t NUM_SIZE_CLASSES = 28;

	struct FreeBlock
	{
		FreeBlock*	next;
	};

	struct SizeClass
	{
		size_t		blockSize;
		FreeBlock*	freeList;
	};

	struct BlockHeader
	{
		SizeClass*	sizeClass;
		size_t		padding;
	};

	struct alignas(16) Slab
	{
		Slab*	next;
	};

	static const size_t SIZE_CLASSES[NUM_SIZE_CLASSES];

	SizeClass	sizeClasses[NUM_SIZE_CLASSES];
	size_t		alignBytes;
	Slab*		slabList;

	// 函数: InitSizeClasses
	// 描述: 初始化尺寸类数组
	VOID InitSizeClasses();

	// 函数: FindSizeClass
	// 描述: 依请求大小找到能容纳它的最小尺寸类
	SizeClass* FindSizeClass(size_t size);

	// 函数: AllocateFromSlab
	// 描述: 申请新的 slab, 切分成块并挂入该尺寸类的空闲链表
	VOID* AllocateFromSlab(SizeClass* sc);

	// 函数: FreeSlabList
	// 描述: 释放全部 slab(Slab 头内嵌在 slab 内存中, 无需 delete)
	VOID FreeSlabList();

	// 函数: IsOwnedPointer
	// 描述: 检查指针是否落在本池某个 slab 的用户数据区范围内
	BOOL IsOwnedPointer(VOID* ptr) const;
};
