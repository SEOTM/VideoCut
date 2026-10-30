#include <windows.h>
#include "AppBuffer.h"

constexpr size_t APP_BUFFER_ALIGN = 16;
constexpr size_t APP_BUFFER_PAGE = 4096;

// 函数: AlignUpApp
// 描述: 把字节数向上对齐到指定对齐值
static size_t AlignUpApp(size_t qwSize, size_t qwAlign)
{
	return (qwSize + qwAlign - 1) & ~(qwAlign - 1);
}

// 函数: AppBuffer
// 描述: 按请求字节数建立线性分配池(见 .h)
AppBuffer::AppBuffer(size_t qwSize)
{
	pBase = NULL;
	qwCapacity = 0;
	qwUsed = 0;
	if (qwSize == 0)
	{
		return;
	}
	{
		size_t qwWant = AlignUpApp(qwSize, APP_BUFFER_PAGE);
		pBase = (BYTE*)VirtualAlloc(NULL, qwWant, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (pBase != NULL)
		{
			qwCapacity = qwWant;
		}
	}
}

// 函数: ~AppBuffer
// 描述: 归还整块池内存(见 .h)
AppBuffer::~AppBuffer()
{
	if (pBase != NULL)
	{
		VirtualFree(pBase, 0, MEM_RELEASE);
		pBase = NULL;
	}
	qwCapacity = 0;
	qwUsed = 0;
}

// 函数: Allocate
// 描述: 从池中切分一段 16 字节对齐的内存(见 .h)
BufferInfo AppBuffer::Allocate(size_t qwSize)
{
	BufferInfo ret{ NULL, 0 };
	if (pBase == NULL || qwSize == 0)
	{
		return ret;
	}
	{
		size_t qwAligned = AlignUpApp(qwSize, APP_BUFFER_ALIGN);
		if (qwAligned > qwCapacity - qwUsed)
		{
			return ret;
		}
		ret.buffer = pBase + qwUsed;
		ret.size = qwSize;
		qwUsed += qwAligned;
	}
	return ret;
}
