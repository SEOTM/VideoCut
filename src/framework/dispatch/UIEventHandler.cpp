#include <windows.h>
#include <string.h>
#include "framework/dispatch/UIEventHandler.h"

static CONST UIEventHandler::EventBinding* sBindings = NULL;
static size_t sBindingCount = 0;

// 函数: SetBindings
// 描述: 注入"事件名 → 处理函数"绑定表(见 .h)
VOID UIEventHandler::SetBindings(CONST EventBinding* pTable, size_t nCount)
{
	sBindings = pTable;
	sBindingCount = (pTable != NULL) ? nCount : 0;
}

// 函数: BindEventHandler
// 描述: 按事件名取绑定的事件处理函数(线性查表; 命中即止)
EventInfo UIEventHandler::BindEventHandler(LPCSTR eventname)
{
	EventInfo ret{};
	if (eventname == NULL)
	{
		return ret;
	}
	for (size_t i = 0; i < sBindingCount; i++)
	{
		CONST EventBinding& b = sBindings[i];
		if (b.szEvent == NULL || b.pfn == NULL)
		{
			continue;
		}
		if (b.chDigitLo == '\0')
		{
			if (strcmp(eventname, b.szEvent) == 0)
			{
				ret.pfnEvent = b.pfn;
				break;
			}
		}
		else
		{
			CONST size_t n = strlen(b.szEvent);
			if (strncmp(eventname, b.szEvent, n) == 0 &&
				eventname[n] >= b.chDigitLo && eventname[n] <= b.chDigitHi &&
				eventname[n + 1] == '\0')
			{
				ret.pfnEvent = b.pfn;
				break;
			}
		}
	}
	return ret;
}
