#pragma once
#include <minwindef.h>
#include <stddef.h>

class FileCodec
{
private:
	FileCodec() = delete;
	~FileCodec() = delete;
public:
	// 函数: Crc32Begin
	// 描述: 取 CRC32 的初始状态(流式计算入口)
	static DWORD Crc32Begin();

	// 函数: Crc32Update
	// 描述: 用一段字节推进 CRC32 状态(可连续多次调用, 以覆盖不连续的字节区间)
	static DWORD Crc32Update(DWORD dwCrc, LPCVOID data, size_t cbData);

	// 函数: Crc32Finish
	// 描述: 取 CRC32 的最终值(对状态取反)
	static DWORD Crc32Finish(DWORD dwCrc);
};
