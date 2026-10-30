#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "framework/base/CtrlBuffer.h"
#include "framework/base/mem/AllocatorHub.h"

namespace CtrlBuffer
{

	constexpr DWORD LIST_CAPACITY = 64;

	// 函数: PrepareCtrlBuffer
	// 描述: 依据控件类型为控件分配/初始化缓冲区(文本/数值/复合数据; 见 CtrlBuffer.h)
	BOOL PrepareCtrlBuffer(CtrlInfo* info, LPCSTR text)
	{
		if (info == NULL)
		{
			return FALSE;
		}
		if (info->IsKnownType() == FALSE ||
			info->type == CTRL_TYPE_LIST)
		{
			return TRUE;
		}
		if (info->type == CTRL_TYPE_LIST_TIMESPAN)
		{
			const DWORD cbItem = (DWORD)sizeof(TimeSpanItem);
			const DWORD dwCapacity = LIST_TIMESPAN_CAPACITY;
			const DWORD cbBuffer = (DWORD)(sizeof(ListBuffer) + (size_t)cbItem * dwCapacity);
			info->bufferInfo = AllocatorHub::AllocSmall(cbBuffer);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				ListBuffer* pList = (ListBuffer*)info->bufferInfo.buffer;
				pList->dwSelected = LIST_SEL_NONE;
				pList->dwCount = 0;
				pList->dwCapacity = dwCapacity;
				pList->cbItem = cbItem;
			}
			return TRUE;
		}
		if (info->type == CTRL_TYPE_INPUT_ACTION)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_INPUT_ACTION_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CtrlInputActionData* pAction = (CtrlInputActionData*)info->bufferInfo.buffer;
				pAction->split = CTRL_INPUT_ACTION_SPLIT;
				{
					CHAR* lpBtn = CtrlInputActionBtnText(pAction);
					if (lpBtn != NULL)
					{
						strcpy_s(lpBtn, (size_t)CTRL_INPUT_ACTION_ACTION, CTRL_DEFAULT_BROWSE_BTN);
					}
				}
				{
					CHAR* lpInput = CtrlInputActionText(pAction);
					CONST size_t cbInput = (size_t)(info->bufferInfo.size - sizeof(CtrlInputActionData) - CTRL_INPUT_ACTION_SPLIT);
					if (lpInput != NULL && cbInput > 0 && text != NULL && text[0] != '\0')
					{
						strcpy_s(lpInput, cbInput, text);
					}
				}
			}
			return TRUE;
		}
		if (info->type == CTRL_TYPE_CHECKBOX)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlCheckData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CtrlCheckData* pCheck = (CtrlCheckData*)info->bufferInfo.buffer;
				if (text != NULL && text[0] != '\0')
				{
					strcpy_s(pCheck->szLabel, (size_t)CTRL_LABEL_BUF, text);
				}
			}
			return TRUE;
		}
		if (info->type == CTRL_TYPE_INPUT_MULTI)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_TEXT_MULTI_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CHAR* lpText = CtrlTextBuffer(info);
				if (lpText != NULL && text != NULL)
				{
					strcpy_s(lpText, info->bufferInfo.size, text);
				}
			}
			return TRUE;
		}
		if (CtrlTypeIsTextKind(info->type))
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_TEXT_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				char* lpText = CtrlTextBuffer(info);
				if (lpText && text)
				{
					strcpy_s(lpText, info->bufferInfo.size, text);
				}
			}
			return TRUE;
		}
		if (CtrlTypeIsList(info->type))
		{
			const DWORD cbItem = CTRL_INPUT_ACTION_BUF;
			const DWORD dwCapacity = LIST_CAPACITY;
			const DWORD cbBuffer = (DWORD)(sizeof(ListBuffer) + (size_t)cbItem * (dwCapacity + 1));
			info->bufferInfo = AllocatorHub::AllocSmall(cbBuffer);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			ListBuffer* pList = (ListBuffer*)info->bufferInfo.buffer;
			pList->dwSelected = LIST_SEL_NONE;
			pList->dwCount = 0;
			pList->dwCapacity = dwCapacity;
			pList->cbItem = cbItem;
			return TRUE;
		}
		switch (info->type)
		{
		case CTRL_TYPE_WND:
		case CTRL_TYPE_CNTR:
		case CTRL_TYPE_RECT:
		{
			return TRUE;
		}
		case CTRL_TYPE_INPUT_REAL:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(DOUBLE));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			*(DOUBLE*)info->bufferInfo.buffer = (text && text[0]) ? atof(text) : 0.0;
			return TRUE;
		}
		case CTRL_TYPE_INPUT_NUMERIC:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(INT));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			*(INT*)info->bufferInfo.buffer = (text && text[0]) ? atoi(text) : 0;
			return TRUE;
		}
		case CTRL_TYPE_COMBO:
		case CTRL_TYPE_COMBO_ACTION:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlComboData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			CtrlComboData* combo = (CtrlComboData*)info->bufferInfo.buffer;
			combo->count = 1;
			combo->index = 0;
			combo->items = NULL;
			BufferInfo arrbuf = AllocatorHub::AllocSmall(sizeof(LPCSTR));
			if (arrbuf.buffer == NULL) { return FALSE; }
			combo->items = (LPCSTR*)arrbuf.buffer;
			combo->items[0] = AllocatorHub::CopyString((text && text[0]) ? text : CTRL_DEFAULT_COMBO_ITEM);
			return TRUE;
		}
		case CTRL_TYPE_SLIDER:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlSliderData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			CtrlSliderData* slider = (CtrlSliderData*)info->bufferInfo.buffer;
			slider->vMin = (text && text[0]) ? atoi(text) : 0;
			slider->lMin = 0;
			slider->lMax = 100;
			return TRUE;
		}
		case CTRL_TYPE_SLIDER_RANGE:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlSliderRangeData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			CtrlSliderRangeData* slider = (CtrlSliderRangeData*)info->bufferInfo.buffer;
			slider->vMin = 0;
			slider->vMax = 100;
			slider->lMin = 0;
			slider->lMax = 100;
			return TRUE;
		}
		case CTRL_TYPE_TIMEBAR:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlTimeBarData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CtrlTimeBarData* pBar = (CtrlTimeBarData*)info->bufferInfo.buffer;
				if (text != NULL && text[0] != '\0')
				{
					strcpy_s(pBar->szHint, (size_t)CTRL_LABEL_BUF, text);
				}
			}
			return TRUE;
		}
		case CTRL_TYPE_PROG:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlProgData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			CtrlProgData* prog = (CtrlProgData*)info->bufferInfo.buffer;
			prog->expect = 100;
			prog->IsVisible = TRUE;
			return TRUE;
		}
		case CTRL_TYPE_RENDER:
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlRenderData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			return TRUE;
		}
		default:
			return FALSE;
		}
	}

	// 函数: WriteCtrlTextToBuffer
	// 描述: 把中间态文本按控件缓冲容量写回该控件(见 CtrlBuffer.h)
	VOID WriteCtrlTextToBuffer(CtrlInfo* pCtrl, LPCSTR text)
	{
		if (pCtrl == NULL)
		{
			return;
		}
		CtrlTextRange range = CtrlTextRangeOf(pCtrl);
		if (range.lpText == NULL || range.cbCapacity == 0)
		{
			return;
		}
		strncpy_s(range.lpText, range.cbCapacity, (text != NULL) ? text : "", _TRUNCATE);
	}

	// 函数: FreeCtrlBuffer
	// 描述: 回收一枚控件的自身缓冲(含下拉类控件的选项指针数组), 并把 bufferInfo 置空(见 CtrlBuffer.h)
	VOID FreeCtrlBuffer(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if ((info->type == CTRL_TYPE_COMBO || info->type == CTRL_TYPE_COMBO_ACTION) &&
			info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* pCombo = (CtrlComboData*)info->bufferInfo.buffer;
			if (pCombo->items != NULL)
			{
				AllocatorHub::FreeSmall(BufferInfo{ (VOID*)pCombo->items, sizeof(LPCSTR) });
			}
			pCombo->items = NULL;
		}
		AllocatorHub::FreeSmall(info->bufferInfo);
		info->bufferInfo = BufferInfo{ NULL, 0 };
	}

}
