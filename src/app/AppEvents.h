#pragma once
#include <stddef.h>
#include "framework/dispatch/UIEventHandler.h"

// ================================
// 类名: AppEvents
// 描述: 业务事件绑定表（事件名 → 处理函数）
// ================================
class AppEvents
{
private:
	AppEvents() = delete;
	~AppEvents() = delete;
	AppEvents(const AppEvents&) = delete;
	AppEvents& operator=(const AppEvents&) = delete;

public:
	// 函数: GetBindings
	// 描述: 取事件绑定表首址与条目数（供 Dispatch::Init 注入）
	// 参数:
	//		OUT	pnCount:	条目数输出
	// 返回:
	//		CONST UIEventHandler::EventBinding*: 绑定表首址
	static CONST UIEventHandler::EventBinding* GetBindings(size_t* pnCount);
};
