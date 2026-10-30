#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imm.h>
#include <float.h>
#include <string.h>
#include <wchar.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "framework/render/imgui/ImGuiHost.h"
#include "framework/render/imgui/ability/ImGuiRender.h"
#include "framework/render/imgui/control/Controls.h"
#include "framework/layout/LayoutRender.h"
#include "framework/base/AppEnv.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct WndUnit
{
	HWND                 hWnd = NULL;
	ATOM                 atomClass = 0;
	ImGuiContext*        ctx = NULL;
	D3D11Manager         d3d;
	HIMC                 hIMC = NULL;
	FLOAT                fFontSize = 0.0f;
	DWORD                dwRounding = 0;
	BOOL                 bRoundByRgn = FALSE;
	BOOL                 bRoundDone = FALSE;
	CHAR                 szTitle[256] = "";
	ImGuiHost::WndShowState stateShow = ImGuiHost::WND_SHOW_NEVER;
	BOOL                 bShowChangeByUs = FALSE;
};

static WndUnit Units[ImGuiHost::MAX_WND];
static DWORD dwStandardWnds = 0;
static DWORD dwActiveWnds = 0;
static ATOM atomSharedClass = 0;
static ImGuiHost::StyleFunc pfnStyleFunc = NULL;
static VOID* pStyleUser = NULL;
static ImGuiHost::SegmentFunc pfnSegmentFunc = NULL;
static VOID* pSegmentUser = NULL;
static BOOL bDarkBase = TRUE;
static BOOL bSegmentOpen = FALSE;
static DWORD dwSegmentIndex = 0;
static DWORD dwCursor = 0;
static BOOL bIMEDisabledOverride = FALSE;
static DWORD dwSegments = 0;
static DWORD dwCmdLists = 0;
static INT64 llVtxCount = 0;
static INT64 llIdxCount = 0;
static DWORD dwResizeCount = 0;
static DWORD dwResizeLastW = 0;
static DWORD dwResizeLastH = 0;
static StyleDesc styleDesc{};
static BOOL bStyleDescSet = FALSE;
static FLOAT fWndFontSize = 0.0f;
static HICON sAppIcon = NULL;
static HostApi HostTable{};

constexpr DWORD DEFAULT_WND_WIDTH = 800;
constexpr DWORD DEFAULT_WND_HEIGHT = 600;
constexpr FLOAT DEFAULT_FONT_SIZE = 18.0f;
constexpr FLOAT FONT_SIZE_MIN = 8.0f;
constexpr DWORD DEFAULT_ROUNDING = 10;
constexpr LPCWSTR CLASS_NAME_SUFFIX = L"_IMGUI_CLASS";

// 函数: FindUnitIndex
// 描述: 按窗口句柄反查标准窗口单元序号(未命中返回 -1)
static INT FindUnitIndex(HWND hWnd);

// 函数: ApplyUnitStyle
// 描述: 对指定窗口重放亮暗基础样式、高级样式回调与已登记的宿主样式
static VOID ApplyUnitStyle(DWORD index);

// 函数: LoadFonts
// 描述: 逐个试载中文字体候选并自校准 em 换算(全部失败退默认字体)
static BOOL LoadFonts(HWND hWnd, ImGuiContext* ctx, FLOAT fFontSize);

// 函数: InitImGuiUnit
// 描述: 建立单个窗口的 ImGui 面(上下文 / 字体 / 两端后端 / 样式 / IME / 能力注入)
static BOOL InitImGuiUnit(DWORD index);

// 函数: ShutdownImGuiUnit
// 描述: 关停单个窗口的 ImGui 面并归还 IME 上下文
static VOID ShutdownImGuiUnit(DWORD index);

// 函数: StartSegment
// 描述: 开启指定窗口的渲染段(两端后端开帧并结束当前 ImGui 帧的准备)
static BOOL StartSegment(DWORD index);

// 函数: FinalizeSegment
// 描述: 结算当前渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
static VOID FinalizeSegment();

// 函数: ResetSegmentState
// 描述: 复位段状态(段标记 / 段序号 / 渲染游标 / IME 覆盖)
static VOID ResetSegmentState();

// 函数: ShowWnd
// 描述: 发起指定窗口的显示或隐藏并标记本次变更的来源
static VOID ShowWnd(DWORD index, BOOL bShow);

// 函数: ApplyActiveVisibility
// 描述: 按激活窗口数应用可见性策略(不足者显示, 超出者隐藏)
static VOID ApplyActiveVisibility();

// 函数: HostWndProc
// 描述: 共用窗口过程: 记账显示状态与尺寸, 处理退出键与销毁消息, 其余交 DefWindowProcW
static LRESULT WINAPI HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 函数: EnableDpiAwareness
// 描述: 打开进程级 DPI 感知(启动首件事; 幂等)
VOID ImGuiHost::EnableDpiAwareness()
{
	ImGui_ImplWin32_EnableDpiAwareness();
}

// 函数: Register
// 描述: 填充宿主接口表并返回其只读指针(调度层取表入口; 重复调用幂等)
CONST HostApi* ImGuiHost::Register()
{
	HostTable.EnableDpiAwareness = &ImGuiHost::EnableDpiAwareness;
	HostTable.Start = &ImGuiHost::Start;
	HostTable.Run = &ImGuiHost::Run;
	HostTable.Release = &ImGuiHost::Release;
	HostTable.DestroyWnds = &ImGuiHost::DestroyWnds;
	HostTable.SetDarkMode = &ImGuiHost::SetDarkMode;
	HostTable.ApplyStyle = &ImGuiHost::ApplyStyle;
	HostTable.SetIMEDisabled = &ImGuiHost::SetIMEDisabled;
	HostTable.GetStandardWndCount = &ImGuiHost::GetStandardWndCount;
	HostTable.GetWndShowState = &ImGuiHost::GetWndShowState;
	HostTable.GetWndRect = &ImGuiHost::GetWndRect;
	HostTable.GetRenderStats = &ImGuiHost::GetRenderStats;
	HostTable.GetResizeStats = &ImGuiHost::GetResizeStats;
	HostTable.SetAppIcon = &ImGuiHost::SetAppIcon;
	HostTable.GetNativeDevice = &ImGuiHost::GetNativeDevice;
	HostTable.GetNativeDeviceContext = &ImGuiHost::GetNativeDeviceContext;
	return &HostTable;
}

// 函数: GetHostApi
// 描述: 取宿主接口表(与 Register 同源; 未填充时先填充)
CONST HostApi* ImGuiHost::GetHostApi()
{
	return Register();
}

// 函数: ApplyStyle
// 描述: 登记宿主样式并刷新已就绪窗口(返回 FALSE 只表示当前没有就绪窗口)
BOOL ImGuiHost::ApplyStyle(CONST StyleDesc& desc)
{
	styleDesc = desc;
	bStyleDescSet = TRUE;
	BOOL bRet = FALSE;
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		if (Units[i].ctx != NULL)
		{
			ApplyUnitStyle(i);
			bRet = TRUE;
		}
	}
	return bRet;
}

// 函数: Start
// 描述: 启动渲染宿主(DPI 感知 → 默认几何构造 → 声明数量 → 建窗 → 建立渲染设施; 失败内部回退)
BOOL ImGuiHost::Start(DWORD dwWndCount, LPCSTR lpszAppName, SegmentFunc pfnSegment, VOID* pUser)
{
	BOOL bRet = FALSE;
	WCHAR szClassName[128]{};
	WCHAR szTitle[128]{};
	WndSpec specs[MAX_WND]{};
	RECT rcWork{};
	INT nX = 0;
	INT nY = 0;
	INT nWideLen = 0;

	EnableDpiAwareness();
	if (lpszAppName == NULL || lpszAppName[0] == '\0' ||
		dwWndCount == 0 || dwWndCount > MAX_WND)
	{
		return bRet;
	}
	nWideLen = MultiByteToWideChar(CP_UTF8, 0, lpszAppName, -1, szTitle, (INT)(sizeof(szTitle) / sizeof(szTitle[0])));
	if (nWideLen <= 0)
	{
		return bRet;
	}
	nWideLen = MultiByteToWideChar(CP_UTF8, 0, lpszAppName, -1, szClassName,
		(INT)(sizeof(szClassName) / sizeof(szClassName[0])));
	if (nWideLen <= 0)
	{
		return bRet;
	}
	if (wcscat_s(szClassName, sizeof(szClassName) / sizeof(szClassName[0]), CLASS_NAME_SUFFIX) != 0)
	{
		return bRet;
	}
	if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0) == FALSE)
	{
		rcWork.left = 0;
		rcWork.top = 0;
		rcWork.right = GetSystemMetrics(SM_CXSCREEN);
		rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	nX = rcWork.left + (((rcWork.right - rcWork.left) - (INT)DEFAULT_WND_WIDTH) / 2);
	nY = rcWork.top + (((rcWork.bottom - rcWork.top) - (INT)DEFAULT_WND_HEIGHT) / 2);
	if (nX < rcWork.left) { nX = rcWork.left; }
	if (nY < rcWork.top) { nY = rcWork.top; }
	for (DWORD i = 0; i < dwWndCount; i++)
	{
		specs[i].lpszClassName = szClassName;
		specs[i].lpszTitle = szTitle;
		specs[i].x = nX;
		specs[i].y = nY;
		specs[i].dwWidth = DEFAULT_WND_WIDTH;
		specs[i].dwHeight = DEFAULT_WND_HEIGHT;
		specs[i].fFontSize = (fWndFontSize > 0.0f) ? fWndFontSize : DEFAULT_FONT_SIZE;
		specs[i].dwRounding = DEFAULT_ROUNDING;
	}
	SetStandardWndCount(dwWndCount);
	SetSegmentFunc(pfnSegment, pUser);
	if (!CreateWnds(specs))
	{
		goto START_CLN;
	}
	if (!Acquire())
	{
		goto START_CLN;
	}
	bRet = TRUE;
	return bRet;

START_CLN:
	DestroyWnds();
	SetSegmentFunc(NULL, NULL);
	return bRet;
}

// 函数: SetStandardWndCount
// 描述: 声明需要创建的标准窗口数量(超过上限按上限夹取)
VOID ImGuiHost::SetStandardWndCount(DWORD dwCount)
{
	if (dwCount > MAX_WND)
	{
		dwCount = MAX_WND;
	}
	dwStandardWnds = dwCount;
}

// 函数: GetStandardWndCount
// 描述: 取已声明的标准窗口数量
DWORD ImGuiHost::GetStandardWndCount()
{
	return dwStandardWnds;
}

// 函数: GetWndCount
// 描述: 取已声明的标准窗口数量(供渲染侧按数量顺序取窗口)
DWORD ImGuiHost::GetWndCount()
{
	return dwStandardWnds;
}

// 函数: SetActiveWndCount
// 描述: 声明布局激活所需的窗口数并同步各窗口可见性
VOID ImGuiHost::SetActiveWndCount(DWORD dwActive)
{
	if (dwActive > dwStandardWnds)
	{
		dwActive = dwStandardWnds;
	}
	dwActiveWnds = dwActive;
	ApplyActiveVisibility();
}

// 函数: GetActiveWndCount
// 描述: 取当前的激活窗口数
DWORD ImGuiHost::GetActiveWndCount()
{
	return dwActiveWnds;
}

// 函数: GetWndShowState
// 描述: 取指定窗口的显示状态(序号越界按从未显示处理)
ImGuiHost::WndShowState ImGuiHost::GetWndShowState(DWORD index)
{
	if (index >= MAX_WND)
	{
		return WND_SHOW_NEVER;
	}
	return Units[index].stateShow;
}

// 函数: GetWndRect
// 描述: 取指定窗口的窗口矩形与客户区矩形(客户区矩形左上恒为 0,0); 窗口未建时返回 FALSE
BOOL ImGuiHost::GetWndRect(DWORD index, RECT* prcWnd, RECT* prcClient)
{
	BOOL bRet = FALSE;
	RECT rcWnd{};
	RECT rcClient{};
	if (index >= MAX_WND || Units[index].hWnd == NULL)
	{
		return bRet;
	}
	if (GetWindowRect(Units[index].hWnd, &rcWnd) == FALSE ||
		GetClientRect(Units[index].hWnd, &rcClient) == FALSE)
	{
		return bRet;
	}
	if (prcWnd != NULL) { *prcWnd = rcWnd; }
	if (prcClient != NULL) { *prcClient = rcClient; }
	bRet = TRUE;
	return bRet;
}

// 函数: SetStyleFunc
// 描述: 注入样式槽填充回调(传 NULL 即清除注入)
VOID ImGuiHost::SetStyleFunc(StyleFunc pfnStyle, VOID* pUser)
{
	pfnStyleFunc = pfnStyle;
	pStyleUser = pUser;
}

// 函数: SetSegmentFunc
// 描述: 注入段前回调(在目标窗口后端开帧之后、ImGui 帧开始之前调用; 传 NULL 即清除注入)
VOID ImGuiHost::SetSegmentFunc(SegmentFunc pfnSegment, VOID* pUser)
{
	pfnSegmentFunc = pfnSegment;
	pSegmentUser = pUser;
}

// 函数: SetFontSize
// 描述: 登记标准窗口的字号(像素; Start 按默认几何建窗时消费; 无效值按缺省字号)
VOID ImGuiHost::SetFontSize(FLOAT fFontSize)
{
	if (!(fFontSize >= FONT_SIZE_MIN))
	{
		fWndFontSize = 0.0f;
		return;
	}
	fWndFontSize = fFontSize;
}

// 函数: SetAppIcon
// 描述: 登记应用窗口图标(见 .h)
VOID ImGuiHost::SetAppIcon(VOID* hIcon)
{
	sAppIcon = (HICON)hIcon;
}

// 函数: GetNativeDevice
// 描述: 取首个标准窗口的 D3D11 设备(见 .h)
VOID* ImGuiHost::GetNativeDevice()
{
	return (VOID*)Units[0].d3d.GetDevice();
}

// 函数: GetNativeDeviceContext
// 描述: 取首个标准窗口的 D3D11 设备上下文(见 .h)
VOID* ImGuiHost::GetNativeDeviceContext()
{
	return (VOID*)Units[0].d3d.GetDeviceContext();
}

// 函数: SetDarkMode
// 描述: 设置亮暗基础样式(入参归一化为真/假; 已就绪窗口立即刷新)
VOID ImGuiHost::SetDarkMode(BOOL bDark)
{
	CONST BOOL bNewBase = (bDark != FALSE) ? TRUE : FALSE;
	if (bNewBase == bDarkBase)
	{
		return;
	}
	bDarkBase = bNewBase;
	RefreshStyle();
}

// 函数: GetDarkMode
// 描述: 取当前亮暗基础样式
BOOL ImGuiHost::GetDarkMode()
{
	return bDarkBase;
}

// 函数: RefreshStyle
// 描述: 把当前样式重新应用到全部已就绪窗口
BOOL ImGuiHost::RefreshStyle()
{
	BOOL bRet = FALSE;
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		if (Units[i].ctx != NULL)
		{
			ApplyUnitStyle(i);
			bRet = TRUE;
		}
	}
	return bRet;
}

// 函数: CreateWnds
// 描述: 按声明数量注册共用窗口类并建窗(含圆角; 任一失败则全部回退)
BOOL ImGuiHost::CreateWnds(CONST WndSpec* pSpecs)
{
	BOOL bRet = FALSE;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	if (pSpecs == NULL || pSpecs[0].lpszClassName == NULL ||
		dwStandardWnds == 0 || dwStandardWnds > MAX_WND)
	{
		return bRet;
	}
	{
		WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, HostWndProc, 0L, 0L,
			hInstance, sAppIcon, NULL, NULL, NULL, pSpecs[0].lpszClassName, sAppIcon };
		atomSharedClass = RegisterClassEx(&wc);
		if (atomSharedClass == 0)
		{
			goto CREATE_CLN;
		}
	}
	for (DWORD i = 0; i < dwStandardWnds; i++)
	{
		WndUnit& unit = Units[i];
		unit.atomClass = atomSharedClass;
		unit.fFontSize = pSpecs[i].fFontSize;
		unit.dwRounding = pSpecs[i].dwRounding;
		unit.bRoundByRgn = FALSE;
		unit.bRoundDone = FALSE;
		unit.szTitle[0] = '\0';
		unit.stateShow = ImGuiHost::WND_SHOW_NEVER;
		unit.bShowChangeByUs = FALSE;
		unit.hWnd = CreateWindow(MAKEINTATOM(atomSharedClass), pSpecs[i].lpszTitle,
			WS_POPUP, (INT)pSpecs[i].x, (INT)pSpecs[i].y, (INT)pSpecs[i].dwWidth, (INT)pSpecs[i].dwHeight,
			NULL, NULL, hInstance, NULL);
		if (unit.hWnd == NULL)
		{
			goto CREATE_CLN;
		}
		if (sAppIcon != NULL)
		{
			SendMessageW(unit.hWnd, WM_SETICON, ICON_BIG, (LPARAM)sAppIcon);
			SendMessageW(unit.hWnd, WM_SETICON, ICON_SMALL, (LPARAM)sAppIcon);
		}
		if (unit.dwRounding > 0)
		{
			AppEnv::ApplyWindowCornerRounding(unit.hWnd, pSpecs[i].dwWidth, pSpecs[i].dwHeight,
				unit.dwRounding, &unit.bRoundByRgn);
			unit.bRoundDone = TRUE;
		}
	}
	dwActiveWnds = dwStandardWnds;
	ResetSegmentState();
	bRet = TRUE;
	return bRet;
CREATE_CLN:
	DestroyWnds();
	return bRet;
}

// 函数: Acquire
// 描述: 建立全部窗口的 D3D11 设施与 ImGui 面并注入渲染能力表
BOOL ImGuiHost::Acquire()
{
	BOOL bRet = FALSE;
	DWORD dwUnit = 0;
	if (dwStandardWnds == 0 || dwStandardWnds > MAX_WND)
	{
		return bRet;
	}
	for (dwUnit = 0; dwUnit < dwStandardWnds; dwUnit++)
	{
		WndUnit& unit = Units[dwUnit];
		if (unit.hWnd == NULL || !unit.d3d.Initialize(unit.hWnd))
		{
			goto ACQUIRE_CLN;
		}
	}
	for (dwUnit = 0; dwUnit < dwStandardWnds; dwUnit++)
	{
		if (!InitImGuiUnit(dwUnit))
		{
			goto ACQUIRE_CLN;
		}
	}
	ApplyActiveVisibility();
	ResetSegmentState();
	bRet = TRUE;
	return bRet;
ACQUIRE_CLN:
	for (dwUnit = 0; dwUnit < MAX_WND; dwUnit++)
	{
		ShutdownImGuiUnit(dwUnit);
	}
	for (dwUnit = 0; dwUnit < MAX_WND; dwUnit++)
	{
		Units[dwUnit].d3d.Shutdown();
	}
	ResetSegmentState();
	return bRet;
}

// 函数: Release
// 描述: 释放全部窗口的 ImGui 面与 D3D11 设施(幂等, 不动窗口)
VOID ImGuiHost::Release()
{
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		ShutdownImGuiUnit(i);
		Units[i].d3d.Shutdown();
	}
	ResetSegmentState();
}

// 函数: DestroyWnds
// 描述: 销毁全部标准窗口并注销共用窗口类(幂等)
VOID ImGuiHost::DestroyWnds()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		WndUnit& unit = Units[i];
		if (unit.hWnd != NULL)
		{
			DestroyWindow(unit.hWnd);
			unit.hWnd = NULL;
		}
		unit.atomClass = 0;
		unit.fFontSize = 0.0f;
		unit.dwRounding = 0;
		unit.bRoundByRgn = FALSE;
		unit.bRoundDone = FALSE;
		unit.szTitle[0] = '\0';
		unit.stateShow = ImGuiHost::WND_SHOW_NEVER;
		unit.bShowChangeByUs = FALSE;
	}
	if (atomSharedClass != 0)
	{
		UnregisterClassW(MAKEINTATOM(atomSharedClass), hInstance);
		atomSharedClass = 0;
	}
	dwActiveWnds = 0;
	ResetSegmentState();
}

// 函数: GetWindow
// 描述: 取指定窗口的窗口句柄(序号越界返回空)
HWND ImGuiHost::GetWindow(DWORD index)
{
	if (index >= MAX_WND)
	{
		return NULL;
	}
	return Units[index].hWnd;
}

// 函数: GetContext
// 描述: 取指定窗口的 ImGui 上下文(序号越界或未就绪返回空)
ImGuiContext* ImGuiHost::GetContext(DWORD index)
{
	if (index >= MAX_WND)
	{
		return NULL;
	}
	return Units[index].ctx;
}

// 函数: IsReady
// 描述: 查询指定窗口的 ImGui 面是否已就绪
BOOL ImGuiHost::IsReady(DWORD index)
{
	if (index >= MAX_WND)
	{
		return FALSE;
	}
	return (Units[index].ctx != NULL) ? TRUE : FALSE;
}

// 函数: Run
// 描述: 运行主消息循环(每帧调一次外部处理流程; 退出判据由外部持有)
VOID ImGuiHost::Run(FrameFunc pfnFrame, VOID* pUser, CONST BOOL* pbExit)
{
	if (pfnFrame == NULL || pbExit == NULL)
	{
		return;
	}
	CONST BOOL* pbExternalExit = pbExit;
	BOOL bQuit = FALSE;
	while (bQuit == FALSE && *pbExternalExit == FALSE)
	{
		dwCursor = 0;
		{
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
				{
					bQuit = TRUE;
				}
			}
		}
		if (bQuit == TRUE || *pbExternalExit != FALSE)
		{
			break;
		}
		pfnFrame(pUser);
		FinalizeSegment();
	}
	FinalizeSegment();
}

// 函数: AcquireNextWnd
// 描述: 结算上一段并取得下一个渲染目标窗口(含几何结算、标题同步与遮挡判定)
BOOL ImGuiHost::AcquireNextWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle)
{
	if (size.x <= 0.0f || size.y <= 0.0f)
	{
		return FALSE;
	}
	FinalizeSegment();
	if (dwCursor >= dwStandardWnds || dwCursor >= MAX_WND)
	{
		return FALSE;
	}
	WndUnit& unit = Units[dwCursor];
	if (unit.hWnd == NULL || unit.ctx == NULL || unit.d3d.GetSwapChain() == NULL)
	{
		return FALSE;
	}
	if (unit.d3d.IsSwapChainOccluded() &&
		unit.d3d.Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
	{
		return FALSE;
	}
	unit.d3d.SetSwapChainOccluded(FALSE);
	{
		RECT rcWnd{};
		if (GetWindowRect(unit.hWnd, &rcWnd) == FALSE)
		{
			return FALSE;
		}
		CONST INT nWantX = (INT)pos.x;
		CONST INT nWantY = (INT)pos.y;
		if (rcWnd.left != nWantX || rcWnd.top != nWantY)
		{
			if (SetWindowPos(unit.hWnd, NULL, nWantX, nWantY, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
			{
				return FALSE;
			}
		}
	}
	{
		CONST DWORD dwWantW = (DWORD)size.x;
		CONST DWORD dwWantH = (DWORD)size.y;
		RECT rcClient{};
		if (!GetClientRect(unit.hWnd, &rcClient))
		{
			return FALSE;
		}
		CONST DWORD dwCurW = (DWORD)(rcClient.right - rcClient.left);
		CONST DWORD dwCurH = (DWORD)(rcClient.bottom - rcClient.top);
		if (dwCurW != dwWantW || dwCurH != dwWantH)
		{
			if (SetWindowPos(unit.hWnd, NULL, 0, 0, (INT)dwWantW, (INT)dwWantH,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
			{
				return FALSE;
			}
			unit.d3d.SetResizeDimensions(dwWantW, dwWantH);
			unit.d3d.ProcessResize();
			dwResizeCount++;
			dwResizeLastW = dwWantW;
			dwResizeLastH = dwWantH;
		}
	}
	if (szTitle != NULL && strcmp(unit.szTitle, szTitle) != 0)
	{
		WCHAR szWideTitle[256]{};
		if (MultiByteToWideChar(CP_UTF8, 0, szTitle, -1, szWideTitle,
			(INT)(sizeof(szWideTitle) / sizeof(szWideTitle[0]))) > 0)
		{
			SetWindowTextW(unit.hWnd, szWideTitle);
		}
		strncpy_s(unit.szTitle, sizeof(unit.szTitle), szTitle, _TRUNCATE);
	}
	if (!StartSegment(dwCursor))
	{
		return FALSE;
	}
	dwCursor++;
	return TRUE;
}

// 函数: GetCurrentWnd
// 描述: 取当前渲染段的窗口句柄(无段时为空)
HWND ImGuiHost::GetCurrentWnd()
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= MAX_WND)
	{
		return NULL;
	}
	return Units[dwSegmentIndex].hWnd;
}

// 函数: GetWndPos
// 描述: 取当前段窗口的桌面位置(真实窗口矩形)
BOOL ImGuiHost::GetWndPos(vec2* pPos)
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= MAX_WND)
	{
		return FALSE;
	}
	RECT rcWnd{};
	if (Units[dwSegmentIndex].hWnd == NULL || GetWindowRect(Units[dwSegmentIndex].hWnd, &rcWnd) == FALSE)
	{
		return FALSE;
	}
	if (pPos != NULL)
	{
		pPos->x = (FLOAT)rcWnd.left;
		pPos->y = (FLOAT)rcWnd.top;
	}
	return TRUE;
}

// 函数: ResetWndCursor
// 描述: 把本帧的渲染目标取用游标归零(供宿主在同步等待等自渲染循环中重走渲染清单)
VOID ImGuiHost::ResetWndCursor()
{
	dwCursor = 0;
}

// 函数: FinalizeFrame
// 描述: 结算当前已开启的渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
VOID ImGuiHost::FinalizeFrame()
{
	FinalizeSegment();
}

// 函数: SetIMEDisabled
// 描述: 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)
VOID ImGuiHost::SetIMEDisabled(BOOL bDisabled)
{
	bIMEDisabledOverride = (bDisabled != FALSE) ? TRUE : FALSE;
}

// 函数: GetRenderStats
// 描述: 取绘制量自证计数(段数 / 绘制命令数 / 顶点数 / 索引数)
VOID ImGuiHost::GetRenderStats(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx)
{
	if (pdwSegments != NULL) { *pdwSegments = dwSegments; }
	if (pdwCmdLists != NULL) { *pdwCmdLists = dwCmdLists; }
	if (pllVtx != NULL) { *pllVtx = llVtxCount; }
	if (pllIdx != NULL) { *pllIdx = llIdxCount; }
}

// 函数: GetResizeStats
// 描述: 取窗口尺寸调整计数与最近一次调整后的客户区尺寸
VOID ImGuiHost::GetResizeStats(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH)
{
	if (pdwCount != NULL) { *pdwCount = dwResizeCount; }
	if (pdwLastW != NULL) { *pdwLastW = dwResizeLastW; }
	if (pdwLastH != NULL) { *pdwLastH = dwResizeLastH; }
}

// 函数: ShowWnd
// 描述: 发起指定窗口的显示或隐藏并标记本次变更的来源
static VOID ShowWnd(DWORD index, BOOL bShow)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.hWnd == NULL)
	{
		return;
	}
	unit.bShowChangeByUs = TRUE;
	ShowWindow(unit.hWnd, (bShow != FALSE) ? SW_SHOW : SW_HIDE);
	unit.bShowChangeByUs = FALSE;
}

// 函数: ApplyActiveVisibility
// 描述: 按激活窗口数应用可见性策略(不足者显示, 超出者隐藏)
static VOID ApplyActiveVisibility()
{
	for (DWORD i = 0; i < dwStandardWnds; i++)
	{
		WndUnit& unit = Units[i];
		if (unit.hWnd == NULL)
		{
			continue;
		}
		if (i < dwActiveWnds)
		{
			if (unit.stateShow == ImGuiHost::WND_SHOW_NEVER ||
				unit.stateShow == ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE)
			{
				ShowWnd(i, TRUE);
			}
		}
		else
		{
			if (unit.stateShow != ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE &&
				unit.stateShow != ImGuiHost::WND_SHOW_HIDDEN_BY_HOST)
			{
				ShowWnd(i, FALSE);
			}
		}
	}
}

// 函数: FindUnitIndex
// 描述: 按窗口句柄反查标准窗口单元序号(未命中返回 -1)
static INT FindUnitIndex(HWND hWnd)
{
	if (hWnd == NULL)
	{
		return -1;
	}
	for (DWORD i = 0; i < ImGuiHost::MAX_WND; i++)
	{
		if (Units[i].hWnd == hWnd)
		{
			return (INT)i;
		}
	}
	return -1;
}

// 函数: ApplyUnitStyle
// 描述: 对指定窗口重放亮暗基础样式、高级样式回调与已登记的宿主样式
static VOID ApplyUnitStyle(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx == NULL)
	{
		return;
	}
	ImGui::SetCurrentContext(unit.ctx);
	if (bDarkBase)
	{
		ImGui::StyleColorsDark();
	}
	else
	{
		ImGui::StyleColorsLight();
	}
	if (pfnStyleFunc != NULL)
	{
		pfnStyleFunc(ImGui::GetStyle(), pStyleUser);
	}
	if (bStyleDescSet)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = styleDesc.fWindowRounding;
		style.FrameRounding = styleDesc.fFrameRounding;
		style.ChildRounding = styleDesc.fChildRounding;
		style.PopupRounding = styleDesc.fPopupRounding;
		style.Colors[ImGuiCol_WindowBg] = MAKE_CR_IMVEC4(styleDesc.crWindowBg);
		style.Colors[ImGuiCol_ChildBg] = MAKE_CR_IMVEC4(styleDesc.crChildBg);
		style.Colors[ImGuiCol_FrameBg] = MAKE_CR_IMVEC4(styleDesc.crFrameBg);
		style.Colors[ImGuiCol_FrameBgHovered] = MAKE_CR_IMVEC4(styleDesc.crFrameBgHovered);
		style.Colors[ImGuiCol_FrameBgActive] = MAKE_CR_IMVEC4(styleDesc.crFrameBgActive);
		style.Colors[ImGuiCol_Button] = MAKE_CR_IMVEC4(styleDesc.crButton);
		style.Colors[ImGuiCol_ButtonHovered] = MAKE_CR_IMVEC4(styleDesc.crButtonHovered);
		style.Colors[ImGuiCol_ButtonActive] = MAKE_CR_IMVEC4(styleDesc.crButtonActive);
		style.Colors[ImGuiCol_Border] = MAKE_CR_IMVEC4(styleDesc.crBorder);
		style.Colors[ImGuiCol_Text] = MAKE_CR_IMVEC4(styleDesc.crText);
		style.Colors[ImGuiCol_TextDisabled] = MAKE_CR_IMVEC4(styleDesc.crTextDisabled);
		style.Colors[ImGuiCol_SliderGrab] = MAKE_CR_IMVEC4(styleDesc.crSliderGrab);
		style.Colors[ImGuiCol_SliderGrabActive] = MAKE_CR_IMVEC4(styleDesc.crSliderGrabActive);
		style.Colors[ImGuiCol_CheckMark] = MAKE_CR_IMVEC4(styleDesc.crCheckMark);
		style.Colors[ImGuiCol_Header] = MAKE_CR_IMVEC4(styleDesc.crHeader);
		style.Colors[ImGuiCol_HeaderHovered] = MAKE_CR_IMVEC4(styleDesc.crHeaderHovered);
		style.Colors[ImGuiCol_HeaderActive] = MAKE_CR_IMVEC4(styleDesc.crHeaderActive);
	}
}

// 函数: LoadFonts
// 描述: 逐个试载中文字体候选并自校准 em 换算(全部失败退默认字体)
static BOOL LoadFonts(HWND hWnd, ImGuiContext* ctx, FLOAT fFontSize)
{
	BOOL bRet = FALSE;
	ImGui::SetCurrentContext(ctx);
	ImGuiIO& io = ImGui::GetIO();
	CONST char* FontFiles[] = {
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\simsun.ttc",
		NULL
	};
	io.Fonts->Clear();
	io.FontDefault = NULL;
	ImFont* pFont = NULL;
	if (fFontSize < FONT_SIZE_MIN) { fFontSize = DEFAULT_FONT_SIZE; }
	for (INT i = 0; FontFiles[i] != NULL; i++)
	{
		FLOAT fEmScale = 1.0f;
		{
			ImFontConfig cfgProbe;
			ImFont* pProbe = io.Fonts->AddFontFromFileTTF(FontFiles[i], fFontSize, &cfgProbe, io.Fonts->GetGlyphRangesChineseFull());
			if (pProbe == NULL)
			{
				continue;
			}
			CONST ImVec2 tsProbe = pProbe->CalcTextSizeA(fFontSize, FLT_MAX, 0.0f, u8"汉字宽度");
			if (tsProbe.x > 0.0f)
			{
				fEmScale = fFontSize / (tsProbe.x / 4.0f);
			}
			io.Fonts->Clear();
		}
		ImFontConfig cfg;
		cfg.ExtraSizeScale = fEmScale;
		pFont = io.Fonts->AddFontFromFileTTF(FontFiles[i], fFontSize, &cfg, io.Fonts->GetGlyphRangesChineseFull());
		if (pFont) { break; }
	}
	if (pFont == NULL)
	{
		io.Fonts->AddFontDefault();
		MessageBoxW(hWnd, L"未能加载中文字体, 将使用默认字体。", L"字体警告", MB_OK | MB_ICONWARNING);
	}
	else
	{
		io.FontDefault = pFont;
	}
	bRet = TRUE;
	return bRet;
}

// 函数: InitImGuiUnit
// 描述: 建立单个窗口的 ImGui 面(上下文 / 字体 / 两端后端 / 样式 / IME / 能力注入)
static BOOL InitImGuiUnit(DWORD index)
{
	BOOL bRet = FALSE;
	if (index >= ImGuiHost::MAX_WND)
	{
		return bRet;
	}
	WndUnit& unit = Units[index];
	if (unit.hWnd == NULL || unit.d3d.GetDevice() == NULL || unit.d3d.GetDeviceContext() == NULL)
	{
		return bRet;
	}
	unit.ctx = ImGui::CreateContext();
	if (unit.ctx == NULL)
	{
		return bRet;
	}
	ImGui::SetCurrentContext(unit.ctx);
	{
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = NULL;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigErrorRecovery = false;
	}
	if (!LoadFonts(unit.hWnd, unit.ctx, unit.fFontSize))
	{
		goto INIT_CLN;
	}
	if (!ImGui_ImplWin32_Init(unit.hWnd))
	{
		goto INIT_CLN;
	}
	if (!ImGui_ImplDX11_Init(unit.d3d.GetDevice(), unit.d3d.GetDeviceContext()))
	{
		goto INIT_CLN;
	}
	ApplyUnitStyle(index);
	unit.hIMC = ImmGetContext(unit.hWnd);
	LayoutRender::SetRenderApi(ImGuiRender::GetApi());
	LayoutRender::SetCtrlApi(Controls::GetCtrlApi());
	bRet = TRUE;
	return bRet;
INIT_CLN:
	ImGui::SetCurrentContext(unit.ctx);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(unit.ctx);
	unit.ctx = NULL;
	return bRet;
}

// 函数: ShutdownImGuiUnit
// 描述: 关停单个窗口的 ImGui 面并归还 IME 上下文
static VOID ShutdownImGuiUnit(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx != NULL)
	{
		ImGui::SetCurrentContext(unit.ctx);
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(unit.ctx);
		unit.ctx = NULL;
	}
	if (unit.hIMC != NULL && unit.hWnd != NULL)
	{
		ImmReleaseContext(unit.hWnd, unit.hIMC);
	}
	unit.hIMC = NULL;
}

// 函数: StartSegment
// 描述: 开启指定窗口的渲染段(两端后端开帧并结束当前 ImGui 帧的准备)
static BOOL StartSegment(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND || bSegmentOpen == TRUE)
	{
		return FALSE;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx == NULL)
	{
		return FALSE;
	}
	unit.d3d.ProcessResize();
	ImGui::SetCurrentContext(unit.ctx);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	if (pfnSegmentFunc != NULL)
	{
		pfnSegmentFunc(index, pSegmentUser);
	}
	ImGui::NewFrame();
	bSegmentOpen = TRUE;
	dwSegmentIndex = index;
	return TRUE;
}

// 函数: FinalizeSegment
// 描述: 结算当前渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
static VOID FinalizeSegment()
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= ImGuiHost::MAX_WND)
	{
		return;
	}
	CONST DWORD dwIndex = dwSegmentIndex;
	WndUnit& unit = Units[dwIndex];
	bSegmentOpen = FALSE;
	dwSegmentIndex = 0;
	bIMEDisabledOverride = FALSE;
	if (unit.ctx == NULL || unit.d3d.GetDeviceContext() == NULL)
	{
		return;
	}
	ImGui::SetCurrentContext(unit.ctx);
	ImGui::Render();
	ImDrawData* pDrawData = ImGui::GetDrawData();
	ID3D11RenderTargetView* pRtv = unit.d3d.GetMainRenderTargetView();
	if (pRtv != NULL)
	{
		unit.d3d.GetDeviceContext()->OMSetRenderTargets(1, &pRtv, NULL);
	}
	if (pDrawData != NULL)
	{
		dwCmdLists += (DWORD)pDrawData->CmdListsCount;
		llVtxCount += (INT64)pDrawData->TotalVtxCount;
		llIdxCount += (INT64)pDrawData->TotalIdxCount;
	}
	ImGui_ImplDX11_RenderDrawData(pDrawData);
	dwSegments++;
	{
		CONST BOOL bDisable = (bIMEDisabledOverride != FALSE) ?
			TRUE : ((ImGui::GetIO().WantTextInput != false) ? TRUE : FALSE);
		if (unit.hIMC != NULL)
		{
			if (bDisable != FALSE)
			{
				if (ImmGetOpenStatus(unit.hIMC)) { ImmSetOpenStatus(unit.hIMC, FALSE); }
			}
			else
			{
				if (!ImmGetOpenStatus(unit.hIMC)) { ImmSetOpenStatus(unit.hIMC, TRUE); }
			}
		}
	}
	unit.d3d.Present(1, 0);
	{
		CONST FLOAT clearMain[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		unit.d3d.ClearRenderTargetView(clearMain);
	}
}

// 函数: ResetSegmentState
// 描述: 复位段状态(段标记 / 段序号 / 渲染游标 / IME 覆盖)
static VOID ResetSegmentState()
{
	bSegmentOpen = FALSE;
	dwSegmentIndex = 0;
	dwCursor = 0;
	bIMEDisabledOverride = FALSE;
}

// 函数: HostWndProc
// 描述: 共用窗口过程: 记账显示状态与尺寸, 处理退出键与销毁消息, 其余交 DefWindowProcW
static LRESULT WINAPI HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	INT index = FindUnitIndex(hWnd);
	if (msg == WM_SHOWWINDOW && index >= 0 && index < (INT)ImGuiHost::MAX_WND)
	{
		WndUnit& unit = Units[index];
		if (wParam != 0)
		{
			unit.stateShow = ImGuiHost::WND_SHOW_VISIBLE;
		}
		else
		{
			unit.stateShow = (unit.bShowChangeByUs != FALSE) ?
				ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE : ImGuiHost::WND_SHOW_HIDDEN_BY_HOST;
		}
	}
	if (msg == WM_SIZE && wParam != SIZE_MINIMIZED && index >= 0 && index < (INT)ImGuiHost::MAX_WND)
	{
		WndUnit& unit = Units[index];
		CONST DWORD dwW = (DWORD)LOWORD(lParam);
		CONST DWORD dwH = (DWORD)HIWORD(lParam);
		unit.d3d.SetResizeDimensions(dwW, dwH);
		if (unit.dwRounding > 0 && unit.bRoundByRgn != FALSE && unit.bRoundDone != FALSE)
		{
			AppEnv::ApplyWindowCornerRounding(hWnd, dwW, dwH, unit.dwRounding, &unit.bRoundByRgn);
		}
	}
	ImGuiContext* pPrevCtx = ImGui::GetCurrentContext();
	BOOL bSwitched = FALSE;
	if (index >= 0 && Units[index].ctx != NULL)
	{
		ImGui::SetCurrentContext(Units[index].ctx);
		bSwitched = TRUE;
	}
	if (bSwitched && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		ImGui::SetCurrentContext(pPrevCtx);
		return TRUE;
	}
	LRESULT lrRet = 0;
	switch (msg)
	{
	case WM_DESTROY:
	{
		::PostQuitMessage(0);
		lrRet = 0;
		break;
	}
	case WM_KEYDOWN:
	{
		if (wParam == VK_ESCAPE)
		{
			::PostQuitMessage(0);
			lrRet = 0;
		}
		else
		{
			lrRet = ::DefWindowProcW(hWnd, msg, wParam, lParam);
		}
		break;
	}
	default:
	{
		lrRet = ::DefWindowProcW(hWnd, msg, wParam, lParam);
		break;
	}
	}
	if (bSwitched)
	{
		ImGui::SetCurrentContext(pPrevCtx);
	}
	return lrRet;
}
