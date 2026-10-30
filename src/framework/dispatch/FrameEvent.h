#pragma once
#include <minwindef.h>
#include <stdint.h>
#include "framework/base/FuncPtr.h"

class FrameEvent
{
private:
	FrameEvent() = delete;
	~FrameEvent() = delete;
	FrameEvent(const FrameEvent&) = delete;
	FrameEvent& operator=(const FrameEvent&) = delete;

public:
	struct Entry
	{
		pfnCtrlEvent pfnEvent;
		VOID* pObj;
		void* pData;
		uint64_t qwOper;
		uint64_t qwParam;
	};

	static constexpr DWORD FRAME_EVENT_CAPACITY = 64;

	// 函数: Post
	// 描述: 把一枚事件挂载到帧间队列(等待本渲染帧结束之后执行)
	static BOOL Post(pfnCtrlEvent pfnEvent, VOID* pObj, uint64_t qwOper, uint64_t qwParam, void* pData);

	// 函数: Drain
	// 描述: 执行当前全部挂载项(排空队列) —— 调用方 = 宿主主循环(两窗口的渲染帧均结束之后)
	static VOID Drain();

	// 函数: IsInFrame
	// 描述: 查询当前是否处于渲染帧内 —— 帧间事件的时机判定入口(处理函数最前面调用)
	static BOOL IsInFrame();

	// 函数: IsDraining
	// 描述: 查询帧末排空流程是否活跃(与 IsInFrame 互为反面: 排空活跃 ⇔ IsInFrame 为假)
	static BOOL IsDraining();
};
