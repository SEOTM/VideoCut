#pragma once
#include <minwindef.h>
#include <stdint.h>
#include <stddef.h>
#include "framework/base/ControlSpec.h"

constexpr DWORD CTRL_TEXT_BUF = 256;

constexpr DWORD CTRL_TEXT_MULTI_BUF = 4096;

constexpr DWORD CTRL_LABEL_BUF = 256;

inline constexpr LPCSTR CTRL_DEFAULT_ACTION_BTN = u8"动作";

inline constexpr LPCSTR CTRL_DEFAULT_BROWSE_BTN = u8"浏览";

inline constexpr LPCSTR CTRL_DEFAULT_COMBO_ITEM = u8"选项 1";

constexpr DWORD CTRL_INPUT_ACTION_ACTION = 64;

constexpr DWORD CTRL_INPUT_ACTION_SPLIT = ((CTRL_INPUT_ACTION_ACTION + 7u) & ~7u);

struct CtrlInputActionData
{
	DWORD split;
};

constexpr DWORD CTRL_INPUT_ACTION_BUF =
	(DWORD)(sizeof(CtrlInputActionData) + CTRL_INPUT_ACTION_SPLIT + CTRL_TEXT_BUF);

// 函数: CtrlInputActionBtnText
// 描述: 取载荷内按钮文本区首址(容量 CTRL_INPUT_ACTION_ACTION)
inline CHAR* CtrlInputActionBtnText(CtrlInputActionData* pData)
{
	if (pData == NULL)
	{
		return NULL;
	}
	return (CHAR*)((BYTE*)pData + sizeof(CtrlInputActionData));
}

// 函数: CtrlInputActionText
// 描述: 取载荷内输入文本区首址(容量 = 缓冲总长 - 定长头 - CTRL_INPUT_ACTION_SPLIT)
inline CHAR* CtrlInputActionText(CtrlInputActionData* pData)
{
	if (pData == NULL)
	{
		return NULL;
	}
	if (pData->split != CTRL_INPUT_ACTION_SPLIT)
	{
		return NULL;
	}
	return (CHAR*)((BYTE*)pData + sizeof(CtrlInputActionData) + CTRL_INPUT_ACTION_SPLIT);
}

struct CtrlComboData
{
	INT count;
	INT index;
	LPCSTR* items;
	LPCSTR lpBtn;
};

struct CtrlProgData
{
	int64_t current;
	int64_t expect;
	BOOL IsVisible;
};

struct CtrlRenderData
{
	void* pTexture;
	INT32 texWidth;
	INT32 texHeight;
};

struct CtrlCheckData
{
	BOOL IsChecked;
	CHAR szLabel[CTRL_LABEL_BUF];
};

struct CtrlSliderData
{
	int64_t vMin;
	int64_t lMin;
	int64_t lMax;
};

constexpr DWORD LIST_TIMESPAN_CAPACITY = 128;

// 结构: TimeSpanItem
// 描述: 时间片列表的一行(起止毫秒)
struct TimeSpanItem
{
	int64_t startMs;
	int64_t endMs;
};

// 结构: CtrlTimeBarData
// 描述: 可编辑时间条的缓冲(提示文本 + 当前毫秒 + 上限毫秒; 时码四段由控件按值拆分)
struct CtrlTimeBarData
{
	CHAR szHint[CTRL_LABEL_BUF];	// 提示文本(装配期由布局文本复制)
	int64_t valueMs;				// 当前时间(毫秒)
	int64_t limitMs;				// 上限(毫秒; 不大于 0 表示不限)
};

// 结构: CtrlSliderRangeData
// 描述: 双手柄范围滑块的缓冲(当前区间与值域)
struct CtrlSliderRangeData
{
	int64_t vMin;
	int64_t vMax;
	int64_t lMin;
	int64_t lMax;
};

struct CtrlTextRange
{
	CHAR* lpText;
	size_t cbCapacity;
};

// 函数: CtrlTextRangeOf
// 描述: 取文本类控件的文本首址与可写容量(首址 + 容量一次给出)
inline CtrlTextRange CtrlTextRangeOf(CtrlInfo* info)
{
	CtrlTextRange range{ NULL, 0 };
	if (info == NULL || info->bufferInfo.buffer == NULL || info->bufferInfo.size == 0)
	{
		return range;
	}
	if (CtrlTypeIsTextKind(info->type) == FALSE)
	{
		return range;
	}
	CHAR* lpText = NULL;
	if (info->type == CTRL_TYPE_INPUT_ACTION)
	{
		if (info->bufferInfo.size < CTRL_INPUT_ACTION_BUF)
		{
			return range;
		}
		lpText = CtrlInputActionText((CtrlInputActionData*)info->bufferInfo.buffer);
	}
	else
	{
		lpText = (CHAR*)info->bufferInfo.buffer;
	}
	if (lpText == NULL)
	{
		return range;
	}
	const size_t nOffset = (size_t)(lpText - (CHAR*)info->bufferInfo.buffer);
	if (nOffset >= (size_t)info->bufferInfo.size)
	{
		return range;
	}
	range.lpText = lpText;
	range.cbCapacity = (size_t)info->bufferInfo.size - nOffset;
	return range;
}

// 函数: CtrlTextBuffer
// 描述: 取文本类控件的文本首址(与 CtrlTextRangeOf 同源; 保留为兼容入口)
inline CHAR* CtrlTextBuffer(CtrlInfo* info)
{
	return CtrlTextRangeOf(info).lpText;
}

// 函数: CtrlTextCapacity
// 描述: 取文本类控件的可写文本容量(与 CtrlTextRangeOf 同源; 保留为兼容入口)
inline size_t CtrlTextCapacity(CtrlInfo* info)
{
	return CtrlTextRangeOf(info).cbCapacity;
}

namespace CtrlBuffer
{
	// 函数: PrepareCtrlBuffer
	// 描述: 依据控件类型为控件分配/初始化缓冲区(文本/数值/复合数据)
	BOOL PrepareCtrlBuffer(CtrlInfo* info, LPCSTR text);

	// 函数: WriteCtrlTextToBuffer
	// 描述: 把中间态文本按控件缓冲容量写回该控件(窗口非客户区控件"外观信息"刷新用; 无文本缓冲者安全忽略)
	VOID WriteCtrlTextToBuffer(CtrlInfo* pCtrl, LPCSTR text);

	// 函数: FreeCtrlBuffer
	// 描述: 回收一枚控件的自身缓冲(含下拉类控件的选项指针数组), 并把 bufferInfo 置空
	VOID FreeCtrlBuffer(CtrlInfo* info);
}
