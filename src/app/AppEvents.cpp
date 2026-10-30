#include "AppEvents.h"
#include "AppState.h"
#include "Identity.h"

// ================================
// 事件名（与布局文档 event 字段成对）
// ================================
constexpr LPCSTR EVENT_TITLE = "VIDEOCUT_TITLEBAR";
constexpr LPCSTR EVENT_BROWSE = "VIDEOCUT_FILEBROWSE";
constexpr LPCSTR EVENT_OPEN = "VIDEOCUT_OPENFOLDER";
constexpr LPCSTR EVENT_SELECT = "VIDEOCUT_SELECTVIDEO";
constexpr LPCSTR EVENT_ADD = "VIDEOCUT_APPENDSPAN";
constexpr LPCSTR EVENT_ADJUSTSPAN = "VIDEOCUT_ADJUSTSPAN";
constexpr LPCSTR EVENT_LISTOPERATION = "VIDEOCUT_LISTOPERATION";
constexpr LPCSTR EVENT_EDIT_BEGIN = "VIDEOCUT_EDIT_BEGIN";
constexpr LPCSTR EVENT_EDIT_END = "VIDEOCUT_EDIT_END";
constexpr LPCSTR EVENT_PLAY = "VIDEOCUT_PLAY";
constexpr LPCSTR EVENT_SPEED = "VIDEOCUT_SPEED";
constexpr LPCSTR EVENT_REMIX = "VIDEOCUT_REMIX";
constexpr LPCSTR EVENT_RESET = "VIDEOCUT_RESET";

// ================================
// 处理函数（统一 pfnCtrlEvent 签名）
// ================================

// 函数: OnTitleBarEvent
// 描述: 标题栏事件（拖动 / 关闭 / 双击）
static VOID OnTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(data);
	AppState::OnTitleBar(obj, oper, param);
}
// 函数: OnBrowseEvent
// 描述: 浏览文件夹
static VOID OnBrowseEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnBrowse();
}
// 函数: OnOpenEvent
// 描述: 打开文件夹
static VOID OnOpenEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnOpenFolder();
}
// 函数: OnSelectEvent
// 描述: 视频分组变更
static VOID OnSelectEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnSelectVideo();
}
// 函数: OnAddEvent
// 描述: 添加剪辑片段
static VOID OnAddEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnAddSpan();
}
// 函数: OnSpanEvent
// 描述: 双手柄范围滑块变更
static VOID OnSpanEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnSpanSlider(obj, oper);
}
// 函数: OnListEvent
// 描述: 时间片列表行操作
static VOID OnListEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(data);
	AppState::OnListOperation(obj, oper, param);
}
// 函数: OnEditBeginEvent
// 描述: 起点毫秒输入提交
static VOID OnEditBeginEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnEditBegin();
}
// 函数: OnEditEndEvent
// 描述: 终点毫秒输入提交
static VOID OnEditEndEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnEditEnd();
}
// 函数: OnPlayEvent
// 描述: 播放 / 暂停
static VOID OnPlayEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnPlay();
}
// 函数: OnSpeedEvent
// 描述: 倍速输入提交
static VOID OnSpeedEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnSpeed();
}
// 函数: OnRemixEvent
// 描述: 执行重混
static VOID OnRemixEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnRemix();
}
// 函数: OnResetEvent
// 描述: 重设目录
static VOID OnResetEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	AppState::OnReset();
}

// 函数: GetBindings
// 描述: 取事件绑定表(见 .h)
CONST UIEventHandler::EventBinding* AppEvents::GetBindings(size_t* pnCount)
{
	static CONST UIEventHandler::EventBinding Bindings[] =
	{
		{ IDENTITY_EVENT_DRAG_WINDOW,	OnTitleBarEvent,	'\0',	'\0' },
		{ EVENT_TITLE,					OnTitleBarEvent,	'\0',	'\0' },
		{ EVENT_BROWSE,					OnBrowseEvent,		'\0',	'\0' },
		{ EVENT_OPEN,					OnOpenEvent,		'\0',	'\0' },
		{ EVENT_SELECT,					OnSelectEvent,		'\0',	'\0' },
		{ EVENT_ADD,					OnAddEvent,			'\0',	'\0' },
		{ EVENT_ADJUSTSPAN,				OnSpanEvent,		'\0',	'\0' },
		{ EVENT_LISTOPERATION,			OnListEvent,		'\0',	'\0' },
		{ EVENT_EDIT_BEGIN,				OnEditBeginEvent,	'\0',	'\0' },
		{ EVENT_EDIT_END,				OnEditEndEvent,		'\0',	'\0' },
		{ EVENT_PLAY,					OnPlayEvent,		'\0',	'\0' },
		{ EVENT_SPEED,					OnSpeedEvent,		'\0',	'\0' },
		{ EVENT_REMIX,					OnRemixEvent,		'\0',	'\0' },
		{ EVENT_RESET,					OnResetEvent,		'\0',	'\0' },
	};
	if (pnCount != NULL)
	{
		*pnCount = sizeof(Bindings) / sizeof(Bindings[0]);
	}
	return Bindings;
}
