#pragma once
#include <minwindef.h>
#include <stdint.h>
class Layout;

using pfnCtrlEvent = VOID(*)(VOID* obj, uint64_t oper, uint64_t param, void* data);

using pfnOverlayBefore = VOID(*)(Layout& layout, size_t index);

struct EventInfo
{
	pfnCtrlEvent pfnEvent;
	DWORD bufferSize;
};

struct RawDataInfo
{
	CONST BYTE * data;
	size_t size;
	BOOL isUseful;
};
struct OperDataInfo
{
	BYTE* data;
	size_t size;
};

using pfnEncoder = RawDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, OperDataInfo* pOperDataInfo, DWORD index);

using pfnDecoder = OperDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, DWORD index);
