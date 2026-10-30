#pragma once
#include <d3d11.h>
#include "imgui.h"

#include "framework/render/imgui/D3D11Manager.h"
#include "framework/base/ControlSpec.h"
#include "framework/render/Render.h"

class ImGuiHost
{
private:
	ImGuiHost() = delete;
	~ImGuiHost() = delete;
	ImGuiHost(const ImGuiHost&) = delete;
	ImGuiHost& operator=(const ImGuiHost&) = delete;

public:
	static constexpr DWORD MAX_WND = 8;

	struct WndSpec
	{
		LPCWSTR lpszClassName;
		LPCWSTR lpszTitle;
		INT     x;
		INT     y;
		DWORD   dwWidth;
		DWORD   dwHeight;
		FLOAT   fFontSize;
		DWORD   dwRounding;
	};

	using WndShowState = ::WndShowState;

	static constexpr WndShowState WND_SHOW_NEVER = ::WND_SHOW_NEVER;
	static constexpr WndShowState WND_SHOW_VISIBLE = ::WND_SHOW_VISIBLE;
	static constexpr WndShowState WND_SHOW_HIDDEN_BY_ACTIVE = ::WND_SHOW_HIDDEN_BY_ACTIVE;
	static constexpr WndShowState WND_SHOW_HIDDEN_BY_HOST = ::WND_SHOW_HIDDEN_BY_HOST;

	using StyleFunc = VOID (*)(ImGuiStyle& style, VOID* pUser);

	using FrameFunc = pfnFrame;

	using SegmentFunc = pfnSegment;

	// 函数: EnableDpiAwareness
	// 描述: 打开进程级 DPI 感知(启动首件事; 幂等)
	static VOID EnableDpiAwareness();

	// 函数: Register
	// 描述: 填充宿主接口表并返回其只读指针(调度层取表入口; 重复调用幂等)
	static CONST HostApi* Register();

	// 函数: GetHostApi
	// 描述: 取宿主接口表(与 Register 同源; 未填充时先填充)
	static CONST HostApi* GetHostApi();

	// 函数: ApplyStyle
	// 描述: 登记宿主样式并刷新已就绪窗口(返回 FALSE 只表示当前没有就绪窗口)
	static BOOL ApplyStyle(CONST StyleDesc& desc);

	// 函数: Start
	// 描述: 启动渲染宿主(DPI 感知 → 默认几何构造 → 声明数量 → 建窗 → 建立渲染设施; 失败内部回退)
	static BOOL Start(DWORD dwWndCount, LPCSTR lpszAppName, SegmentFunc pfnSegment, VOID* pUser);

	// 函数: SetStandardWndCount
	// 描述: 声明需要创建的标准窗口数量(超过上限按上限夹取)
	static VOID SetStandardWndCount(DWORD dwCount);

	// 函数: GetStandardWndCount
	// 描述: 取已声明的标准窗口数量
	static DWORD GetStandardWndCount();

	// 函数: GetWndCount
	// 描述: 取已声明的标准窗口数量(供渲染侧按数量顺序取窗口)
	static DWORD GetWndCount();

	// 函数: SetActiveWndCount
	// 描述: 声明布局激活所需的窗口数并同步各窗口可见性
	static VOID SetActiveWndCount(DWORD dwActive);

	// 函数: GetActiveWndCount
	// 描述: 取当前的激活窗口数
	static DWORD GetActiveWndCount();

	// 函数: GetWndShowState
	// 描述: 取指定窗口的显示状态(序号越界按从未显示处理)
	static WndShowState GetWndShowState(DWORD index);

	// 函数: GetWndRect
	// 描述: 取指定窗口的窗口矩形与客户区矩形(客户区矩形左上恒为 0,0); 窗口未建时返回 FALSE
	static BOOL GetWndRect(DWORD index, RECT* prcWnd, RECT* prcClient);

	// 函数: SetStyleFunc
	// 描述: 注入样式槽填充回调(传 NULL 即清除注入)
	static VOID SetStyleFunc(StyleFunc pfnStyle, VOID* pUser);

	// 函数: SetSegmentFunc
	// 描述: 注入段前回调(在目标窗口后端开帧之后、ImGui 帧开始之前调用; 传 NULL 即清除注入)
	static VOID SetSegmentFunc(SegmentFunc pfnSegment, VOID* pUser);

	// 函数: SetFontSize
	// 描述: 登记标准窗口的字号(像素; Start 按默认几何建窗时消费; 无效值按缺省字号)
	static VOID SetFontSize(FLOAT fFontSize);

	// 函数: SetDarkMode
	// 描述: 设置亮暗基础样式(入参归一化为真/假)
	static VOID SetDarkMode(BOOL bDark);

	// 函数: GetDarkMode
	// 描述: 取当前亮暗基础样式
	static BOOL GetDarkMode();

	// 函数: RefreshStyle
	// 描述: 把当前样式重新应用到全部已就绪窗口
	static BOOL RefreshStyle();

	// 函数: CreateWnds
	// 描述: 按声明数量注册共用窗口类并建窗(含圆角; 任一失败则全部回退)
	static BOOL CreateWnds(CONST WndSpec* pSpecs);

	// 函数: Acquire
	// 描述: 建立全部窗口的 D3D11 设施与 ImGui 面并注入渲染能力表
	static BOOL Acquire();

	// 函数: Run
	// 描述: 运行主消息循环(每帧调一次外部处理流程; 退出判据由外部持有)
	static VOID Run(FrameFunc pfnFrame, VOID* pUser, CONST BOOL* pbExit);

	// 函数: AcquireNextWnd
	// 描述: 结算上一段并取得下一个渲染目标窗口(含几何结算、标题同步与遮挡判定)
	static BOOL AcquireNextWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle);

	// 函数: GetCurrentWnd
	// 描述: 取当前渲染段的窗口句柄(无段时为空)
	static HWND GetCurrentWnd();

	// 函数: GetWndPos
	// 描述: 取当前段窗口的桌面位置(真实窗口矩形)
	static BOOL GetWndPos(vec2* pPos);

	// 函数: ResetWndCursor
	// 描述: 把本帧的渲染目标取用游标归零(供宿主在同步等待等自渲染循环中重走渲染清单)
	static VOID ResetWndCursor();

	// 函数: FinalizeFrame
	// 描述: 结算当前已开启的渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
	static VOID FinalizeFrame();

	// 函数: SetIMEDisabled
	// 描述: 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)
	static VOID SetIMEDisabled(BOOL bDisabled);

	// 函数: GetRenderStats
	// 描述: 取绘制量自证计数(段数 / 绘制命令数 / 顶点数 / 索引数)
	static VOID GetRenderStats(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx);

	// 函数: GetResizeStats
	// 描述: 取窗口尺寸调整计数与最近一次调整后的客户区尺寸
	static VOID GetResizeStats(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH);

	// 函数: Release
	// 描述: 释放全部窗口的 ImGui 面与 D3D11 设施(幂等, 不动窗口)
	static VOID Release();

	// 函数: DestroyWnds
	// 描述: 销毁全部标准窗口并注销窗口类(幂等)
	static VOID DestroyWnds();

	// 函数: SetAppIcon
	// 描述: 登记应用窗口图标(建窗前调用; 传 NULL 清除)
	static VOID SetAppIcon(VOID* hIcon);

	// 函数: GetNativeDevice
	// 描述: 取首个标准窗口的 D3D11 设备(渲染设施未就绪时返回 NULL)
	static VOID* GetNativeDevice();

	// 函数: GetNativeDeviceContext
	// 描述: 取首个标准窗口的 D3D11 设备上下文(渲染设施未就绪时返回 NULL)
	static VOID* GetNativeDeviceContext();

	// 函数: GetWindow
	// 描述: 取指定窗口的窗口句柄(序号越界返回空)
	static HWND GetWindow(DWORD index);

	// 函数: GetContext
	// 描述: 取指定窗口的 ImGui 上下文(序号越界或未就绪返回空)
	static ImGuiContext* GetContext(DWORD index);

	// 函数: IsReady
	// 描述: 查询指定窗口的 ImGui 面是否已就绪
	static BOOL IsReady(DWORD index);
};
