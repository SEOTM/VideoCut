#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "framework/render/imgui/ability/ImGuiRender.h"
#include "framework/render/imgui/ImGuiHost.h"

static ImGuiWindow* pLastWnd = NULL;

static ImVec2 toImVec2(CONST vec2& v);
static BOOL AcquireWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle);
static BOOL OpenWnd(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates);
static VOID CloseWnd();
static VOID BringWndToFront();
static BOOL OpenPanel(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack);
static VOID ClosePanel();
static VOID SetCursor(CONST vec2& pos);
static vec2 GetContentOrigin();
static VOID PushId(INT32 nId);
static VOID PushPtrId(CONST VOID* pKey);
static VOID PopId();
static uint64_t TakeItemId();
static VOID AddHitArea(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size);
static BOOL IsHitClicked();
static BOOL IsRectClicked(CONST vec2& mn, CONST vec2& mx);
static INT64 GetDrawVtxCount();

static CONST RenderApi RenderApiTable =
{
	AcquireWnd,
	OpenWnd,
	CloseWnd,
	BringWndToFront,
	OpenPanel,
	ClosePanel,
	SetCursor,
	GetContentOrigin,
	PushId,
	PushPtrId,
	PopId,
	TakeItemId,
	AddHitArea,
	IsHitClicked,
	IsRectClicked,
	GetDrawVtxCount,
};

// 函数: toImVec2
// 描述: vec2 → ImVec2(ImGui 窗口/容器 API 的入参类型)
static ImVec2 toImVec2(CONST vec2& v)
{
	return ImVec2(v.x, v.y);
}

// 函数: AcquireWnd
// 描述: 取得本次渲染的目标窗口(转调宿主的反调面) —— 渲染目标由渲染器按渲染顺序决定, 布局侧报期望几何与标题
static BOOL AcquireWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle)
{
	if (size.x <= 0.0f || size.y <= 0.0f)
	{
		return FALSE;
	}
	return ImGuiHost::AcquireNextWnd(pos, size, szTitle);
}

// 函数: OpenWnd
// 描述: 开启一个窗口(基座窗口与无独立句柄的内嵌窗口同走本项), 并记为当前绘制目标
static BOOL OpenWnd(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates)
{
	ImGuiWindowFlags flags = 0;
	if ((dwStates & RENDER_WND_BASE) != 0)
	{
		flags |= ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
	}
	if ((dwStates & RENDER_WND_NOINPUT) != 0)
	{
		flags |= ImGuiWindowFlags_NoInputs;
	}
	ImGui::SetNextWindowPos(toImVec2(pos));
	ImGui::SetNextWindowSize(toImVec2(size));
	ImGui::Begin(szName, NULL, flags);
	pLastWnd = ImGui::GetCurrentWindow();
	return TRUE;
}

// 函数: CloseWnd
// 描述: 结束当前窗口(与 OpenWnd 成对)
static VOID CloseWnd()
{
	ImGui::End();
}

// 函数: BringWndToFront
// 描述: 把最近开启的窗口置于绘制顺序最前
static VOID BringWndToFront()
{
	if (pLastWnd != NULL)
	{
		ImGui::BringWindowToDisplayFront(pLastWnd);
	}
}

// 函数: OpenPanel
// 描述: 在当前窗口内开启一个内容容器(承载内容控件的子区域), 并把它作为当前绘制目标
static BOOL OpenPanel(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack)
{
	ImGui::SetCursorPos(toImVec2(pos));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, (ImU32)crBack);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
	ImGui::BeginChild(szName, toImVec2(size), true, 0);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	return TRUE;
}

// 函数: ClosePanel
// 描述: 结束当前内容容器(与 OpenPanel 成对)
static VOID ClosePanel()
{
	ImGui::EndChild();
}

// 函数: SetCursor
// 描述: 把当前窗口/容器的绘制光标定位到指定位置
static VOID SetCursor(CONST vec2& pos)
{
	ImGui::SetCursorPos(toImVec2(pos));
}

// 函数: GetContentOrigin
// 描述: 取当前容器的内容原点(屏幕坐标) —— 即"容器内控件声明坐标的零点"
static vec2 GetContentOrigin()
{
	CONST ImVec2 origin = ImGui::GetCursorScreenPos();
	return vec2(origin.x, origin.y);
}

// 函数: PushId
// 描述: 压入一个整数 ID 命名空间(逐控件绘制前压入控件 id)
static VOID PushId(INT32 nId)
{
	ImGui::PushID((int)nId);
}

// 函数: PushPtrId
// 描述: 压入一个指针 ID 命名空间(内嵌窗口按布局对象地址隔离)
static VOID PushPtrId(CONST VOID* pKey)
{
	ImGui::PushID(pKey);
}

// 函数: PopId
// 描述: 弹出最近压入的 ID 命名空间(与 PushId / PushPtrId 成对)
static VOID PopId()
{
	ImGui::PopID();
}

// 函数: TakeItemId
// 描述: 取当前窗口最近一次提交的可交互项 ID
static uint64_t TakeItemId()
{
	return (uint64_t)ImGui::GetCurrentContext()->LastItemData.ID;
}

// 函数: AddHitArea
// 描述: 在指定矩形上补一枚无绘制命中项(InvisibleButton)
static VOID AddHitArea(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size)
{
	ImGui::SetCursorPos(toImVec2(pos));
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton((szLabel != NULL) ? szLabel : "##CLICKPROBE", toImVec2(size));
}

// 函数: IsHitClicked
// 描述: 判定最近提交的项本帧是否被左键点击
static BOOL IsHitClicked()
{
	return ImGui::IsItemClicked() ? TRUE : FALSE;
}

// 函数: IsRectClicked
// 描述: 判定指定矩形内本帧是否发生左键按下(含当前窗口裁剪)
static BOOL IsRectClicked(CONST vec2& mn, CONST vec2& mx)
{
	if (ImGui::IsMouseHoveringRect(toImVec2(mn), toImVec2(mx)) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return TRUE;
	}
	return FALSE;
}

// 函数: GetDrawVtxCount
// 描述: 取当前窗口累计的绘制顶点数
static INT64 GetDrawVtxCount()
{
	return (INT64)ImGui::GetWindowDrawList()->VtxBuffer.Size;
}

// 函数: GetApi
// 描述: 取本实现填好的渲染能力表(函数指针表的唯一真源)
CONST RenderApi* ImGuiRender::GetApi()
{
	return &RenderApiTable;
}
