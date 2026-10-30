#include "framework/base/util/FileCodec.h"

// 函数: Crc32Begin
// 描述: 取 CRC32 的初始状态(流式计算入口)
DWORD FileCodec::Crc32Begin()
{
	return 0xFFFFFFFFu;
}

// 函数: Crc32Update
// 描述: 用一段字节推进 CRC32 状态(可连续多次调用)
DWORD FileCodec::Crc32Update(DWORD dwCrc, LPCVOID data, size_t cbData)
{
	CONST BYTE* pData = (CONST BYTE*)data;
	if (pData == NULL)
	{
		return dwCrc;
	}
	for (size_t i = 0; i < cbData; i++)
	{
		dwCrc ^= (DWORD)pData[i];
		for (INT b = 0; b < 8; b++)
		{
			dwCrc = (dwCrc >> 1) ^ (0xEDB88320u & (0u - (dwCrc & 1u)));
		}
	}
	return dwCrc;
}

// 函数: Crc32Finish
// 描述: 取 CRC32 的最终值(对状态取反)
DWORD FileCodec::Crc32Finish(DWORD dwCrc)
{
	return ~dwCrc;
}
