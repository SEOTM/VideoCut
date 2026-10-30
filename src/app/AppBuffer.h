#pragma once
#include <minwindef.h>
#include <stddef.h>
#include "framework/base/mem/MemoryManager.h"

// ================================
// 类名: AppBuffer
// 描述: 应用侧线性分配池（业务对象生命周期随池整体释放）
// 备注: 框架的 MemPool 只服务不超过 4096 字节的小块，路径与节点表这类大块
//       由本类按页申请、按 16 字节对齐切分；池析构时一次性归还，池内对象不单独释放
// ================================
class AppBuffer
{
private:
	BYTE* pBase;			// 池起始地址
	size_t qwCapacity;		// 池容量（字节）
	size_t qwUsed;			// 已切分字节数

	AppBuffer(const AppBuffer&) = delete;
	AppBuffer& operator=(const AppBuffer&) = delete;

public:
	// 函数: AppBuffer
	// 描述: 按请求字节数建立线性分配池（不足一页按一页申请）
	AppBuffer(size_t qwSize);
	// 函数: ~AppBuffer
	// 描述: 归还整块池内存（池内对象随之失效）
	~AppBuffer();
	// 函数: Allocate
	// 描述: 从池中切分一段 16 字节对齐的内存
	// 参数:
	//		IN	qwSize:		请求字节数
	// 返回:
	//		BufferInfo:	buffer 为 NULL 表示池内余量不足
	BufferInfo Allocate(size_t qwSize);
};
