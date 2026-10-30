#pragma once
#include "framework/base/FuncPtr.h"
class UIEventHandler
{
private:
	UIEventHandler() = delete;
	~UIEventHandler() = delete;
	UIEventHandler(const UIEventHandler&) = delete;
	UIEventHandler& operator=(const UIEventHandler&) = delete;

public:
	struct EventBinding
	{
		LPCSTR szEvent;
		pfnCtrlEvent pfn;
		CHAR chDigitLo;
		CHAR chDigitHi;
	};

	// 函数: SetBindings
	// 描述: 注入"事件名 → 处理函数"绑定表(宿主启动期调用一次; 须早于首次装配控件)
	static VOID SetBindings(CONST EventBinding* pTable, size_t nCount);

	// 函数: BindEventHandler
	// 描述: 按事件名取绑定的事件处理函数(绑定表未覆盖的名字返回空绑定)
	static EventInfo BindEventHandler(LPCSTR eventname);
};
