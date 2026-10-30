#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/dispatch/Dispatch.h"
#include "framework/base/AppEnv.h"
#include "framework/base/ControlSpec.h"
#include "framework/layout/LayoutRender.h"

constexpr DWORD PROJ_LAYERS = 16;
constexpr DWORD PROJ_COLOR_BASE = 0xFF141414u;
constexpr DWORD PROJ_COLOR_LIGHT = 0xFFEFEFEFu;
constexpr DWORD PROJ_L_WINDOW = PROJ_LAYER_BACKGROUND;
constexpr DWORD PROJ_L_CHILD = PROJ_LAYER_BACKGROUND + 1;
constexpr DWORD PROJ_L_FRAME = PROJ_LAYER_BACKGROUND + 1;
constexpr DWORD PROJ_L_DISABLED = 10;
constexpr DWORD PROOF_EVERY = 60;
constexpr size_t LOG_PREFIX_CAPACITY = 32;
constexpr FLOAT FONT_SIZE_MIN = 12.0f;
constexpr FLOAT FONT_SIZE_FALLBACK = 18.0f;

Dispatch::DispatchData* Dispatch::data = NULL;
static CHAR szLogPrefix[LOG_PREFIX_CAPACITY] = "";

// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线: 前期缓、后期陡)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers);

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers);

// 函数: ShowStateName
// 描述: 取显示状态的中文短名(渲染自证用)
static LPCSTR ShowStateName(WndShowState state);

// 函数: FallbackRef
// 描述: 取缺省参考尺寸(未装配布局或索引越界时的回退量; 函数内静态实例)
static CONST AutoSize& FallbackRef();

// 函数: Init
// 描述: 初始化调度层(控制台 UTF-8 → DPI 感知 → 事件绑定表注入 → 场景装配 → 宿主注入与样式登记 → 无人值守参数)
BOOL Dispatch::Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings,
	CONST LayoutSource& source)
{
	BOOL bRet = FALSE;
	BOOL bNeedDoc = FALSE;
	size_t nBindings = 0;
	bNeedDoc = (source.kind == LAYOUT_SOURCE_DEFAULT || source.kind == LAYOUT_SOURCE_MEMORY) ? TRUE : FALSE;
	if (pHost == NULL || pfnBindings == NULL || cfg.dwWndCount == 0 ||
		(bNeedDoc != FALSE && cfg.szDocName == NULL))
	{
		return bRet;
	}
	AppEnv::InitConsoleUtf8();
	lstrcpynA(szLogPrefix, (cfg.szLogPrefix != NULL) ? cfg.szLogPrefix : "", (INT)LOG_PREFIX_CAPACITY);
	pHost->EnableDpiAwareness();
	{
		CONST UIEventHandler::EventBinding* pBindings = pfnBindings(&nBindings);
		UIEventHandler::SetBindings(pBindings, nBindings);
		Log("事件绑定表: %zu 条\n", nBindings);
	}
	if (!LoadScene(cfg, source))
	{
		goto INIT_END;
	}
	data->pHost = pHost;
	EnsureProjCR();
	data->bDarkMode = (data->bDarkFollowSystem != FALSE)
		? (AppEnv::IsSystemDarkMode() ? TRUE : FALSE) : data->bDarkMode;
	data->pHost->SetDarkMode(data->bDarkMode);
	{
		StyleDesc desc{};
		if (ApplyHostStyle(desc))
		{
			data->pHost->ApplyStyle(desc);
		}
	}
	Log("字号: %.1f (来源: 布局参考量)\n", GetFontSize());
	{
		CHAR szEnv[32]{};
		if (data->szAutoCloseEnv[0] != '\0' && GetEnvironmentVariableA(data->szAutoCloseEnv, szEnv, 32) > 0)
		{
			data->dwAutoCloseMs = (DWORD)atoi(szEnv);
		}
		Log("窗口就绪: 渲染 = 声明式布局路径(LayoutRender::RenderBase; 目标窗口由渲染器决定)\n");
		Log("交互: ESC 或 Alt+F4 退出\n");
	}
	data->qwStartTick = GetTickCount64();
	data->bExit = FALSE;
	bRet = TRUE;

INIT_END:
	if (!bRet)
	{
		Release();
	}
	return bRet;
}

// 函数: Release
// 描述: 释放调度资源(投影配色 → 场景与成员块 → 会话分配器; 幂等)
VOID Dispatch::Release()
{
	if (data != NULL && data->bReady != FALSE && data->dwFrameCount > 0)
	{
		PrintRenderProof("收尾");
	}
	if (data != NULL && data->pProjCR != NULL)
	{
		ProjCR::Release(data->pProjCR);
		data->pProjCR = NULL;
	}
	UnloadScene();
}

// 函数: SetDarkMode
// 描述: 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)
VOID Dispatch::SetDarkMode(BOOL bDark)
{
	if (data == NULL)
	{
		return;
	}
	data->bDarkFollowSystem = FALSE;
	data->bDarkMode = (bDark != FALSE) ? TRUE : FALSE;
	EnsureProjCR();
	if (data->pHost != NULL)
	{
		data->pHost->SetDarkMode(data->bDarkMode);
		StyleDesc desc{};
		if (ApplyHostStyle(desc))
		{
			data->pHost->ApplyStyle(desc);
		}
	}
}

// 函数: Frame
// 描述: 每帧处理流程(建窗几何自证 → 数据层更新 → 布局渲染清单 → 可见性缓存 → IME 覆盖 → 渲染自证 → 退出判据)
VOID Dispatch::Frame(VOID* pUser)
{
	UNREFERENCED_PARAMETER(pUser);
	if (data == NULL || data->pHost == NULL)
	{
		return;
	}
	if (data->dwFrameCount == 0)
	{
		PrintWndGeometry();
	}
	EnsureProjCR();
	{
		BOOL bSegmentOpen = FALSE;
		for (DWORD i = 0; i < data->dwSlotCount; i++)
		{
			CONST LayoutSlot& slot = data->pSlots[i];
			if (slot.pLayout == NULL || slot.bVisible == FALSE)
			{
				continue;
			}
			if (slot.bNested != FALSE)
			{
				if (bSegmentOpen != FALSE)
				{
					LayoutRender::RenderFlex(*slot.pLayout, slot.ref, data->pProjCR);
				}
				continue;
			}
			if (LayoutRender::RenderBase(*slot.pLayout, slot.ref, data->pProjCR) != FALSE)
			{
				bSegmentOpen = TRUE;
			}
		}
	}
	RefreshWndShowCache();
	data->pHost->SetIMEDisabled(LayoutRender::IsIMEDisabled());
	data->dwFrameCount++;
	if (data->dwFrameCount == 1 || (data->dwFrameCount % PROOF_EVERY) == 0)
	{
		// PrintRenderProof("周期");
	}
	if (data->dwAutoCloseMs > 0 && (GetTickCount64() - data->qwStartTick) >= data->dwAutoCloseMs)
	{
		data->bExit = TRUE;
	}
}

// 函数: OnSegment
// 描述: 段前回调范例挂载点(后端开帧之后、UI 库帧开始之前; 当前为空实现)
VOID Dispatch::OnSegment(DWORD index, VOID* pUser)
{
	UNREFERENCED_PARAMETER(index);
	UNREFERENCED_PARAMETER(pUser);
}

// 函数: FallbackRef
// 描述: 取缺省参考尺寸(未装配布局或索引越界时的回退量; 函数内静态实例)
static CONST AutoSize& FallbackRef()
{
	static AutoSize sFallback = AutoSize::CreateByRatio(
		570, 430, 25, 13, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	return sFallback;
}

// 函数: GetLayoutCount
// 描述: 取布局数量(延迟装配且未提交时返回 0)
DWORD Dispatch::GetLayoutCount()
{
	if (data == NULL)
	{
		return 0;
	}
	return data->dwSlotCount;
}

// 函数: GetLayoutAt
// 描述: 取指定槽的布局对象(越界或空槽返回空对象)
Layout& Dispatch::GetLayoutAt(DWORD index)
{
	if (data != NULL && index < data->dwSlotCount && data->pSlots[index].pLayout != NULL)
	{
		return *data->pSlots[index].pLayout;
	}
	return Layout::GetNullObject();
}

// 函数: GetRefAt
// 描述: 取指定槽的参考尺寸(越界返回缺省参考量)
CONST AutoSize& Dispatch::GetRefAt(DWORD index)
{
	if (data != NULL && index < data->dwSlotCount)
	{
		return data->pSlots[index].ref;
	}
	return FallbackRef();
}

// 函数: GetLayout
// 描述: 取首枚布局对象(渲染源; 未就绪时返回空对象)
Layout& Dispatch::GetLayout()
{
	return GetLayoutAt(0);
}

// 函数: GetRef
// 描述: 取首枚参考尺寸(控件几何解算与绘制取值基准; 未就绪时返回缺省参考量)
CONST AutoSize& Dispatch::GetRef()
{
	return GetRefAt(0);
}

// 函数: AttachLayouts
// 描述: 整体替换布局(清空现有槽后逐枚接管; 含运行期替换; 布局对象所有权归调用方)
BOOL Dispatch::AttachLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount)
{
	if (data == NULL || !UseLayouts(ppLayouts, pRefs, dwCount, TRUE))
	{
		return FALSE;
	}
	Log("布局提交: %u 枚(整体替换; 首枚可见; 控件=%zu 已绑定=%u)\n",
		(unsigned)dwCount, (size_t)CountTotalCtrls(), (unsigned)CountBoundCtrls());
	return TRUE;
}

// 函数: AddLayout
// 描述: 追加一枚布局(逐枚提交; 布局对象所有权归调用方; 新槽默认可见且按基座渲染)
BOOL Dispatch::AddLayout(Layout* pLayout, CONST AutoSize& ref)
{
	if (data == NULL || pLayout == NULL)
	{
		return FALSE;
	}
	if (pLayout->GetTotalNum() == 0)
	{
		Log("布局提交失败: 空布局(零控件)\n");
		return FALSE;
	}
	if (!AppendSlot(pLayout, ref, FALSE, TRUE, FALSE))
	{
		return FALSE;
	}
	Log("布局提交: 追加第 %u 枚(控件=%zu; 可见, 基座; 全槽已绑定=%u)\n",
		(unsigned)(data->dwSlotCount - 1), pLayout->GetTotalNum(), (unsigned)CountBoundCtrls());
	return TRUE;
}

// 函数: SetLayoutVisible
// 描述: 设置指定槽的可见性(不可见的布局不参与本帧渲染)
BOOL Dispatch::SetLayoutVisible(DWORD index, BOOL bVisible)
{
	if (data == NULL || index >= data->dwSlotCount)
	{
		return FALSE;
	}
	data->pSlots[index].bVisible = (bVisible != FALSE) ? TRUE : FALSE;
	return TRUE;
}

// 函数: SetLayoutNested
// 描述: 设置指定槽的嵌套状态(TRUE = 按 Flex 渲染, 沿用前一枚基座布局的窗口段; FALSE = 基座渲染)
BOOL Dispatch::SetLayoutNested(DWORD index, BOOL bNested)
{
	if (data == NULL || index >= data->dwSlotCount)
	{
		return FALSE;
	}
	data->pSlots[index].bNested = (bNested != FALSE) ? TRUE : FALSE;
	return TRUE;
}

// 函数: GetLayoutState
// 描述: 取指定槽的可见性与嵌套状态(越界返回 FALSE)
BOOL Dispatch::GetLayoutState(DWORD index, BOOL* pbVisible, BOOL* pbNested)
{
	if (data == NULL || index >= data->dwSlotCount)
	{
		return FALSE;
	}
	if (pbVisible != NULL) { *pbVisible = data->pSlots[index].bVisible; }
	if (pbNested != NULL) { *pbNested = data->pSlots[index].bNested; }
	return TRUE;
}

// 函数: GetFontSize
// 描述: 取建面字号(文档 autosize 的参考字号; 无效时按缺省字号兜底)
FLOAT Dispatch::GetFontSize()
{
	CONST FLOAT fFontSize = GetRef().GetFontSize<FLOAT>();
	if (fFontSize < FONT_SIZE_MIN)
	{
		return FONT_SIZE_FALLBACK;
	}
	return fFontSize;
}

// 函数: GetWndCount
// 描述: 取本调度需要创建的标准窗口数量(未就绪时按单窗口兜底)
DWORD Dispatch::GetWndCount()
{
	if (data != NULL && data->dwWndCount > 0)
	{
		return data->dwWndCount;
	}
	return 1;
}

// 函数: GetWndRect
// 描述: 取主布局窗口矩形(桌面位置取自首枚布局的窗口件位置记录; 客户区尺寸取推导值)
BOOL Dispatch::GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h)
{
	if (data == NULL || data->bReady == FALSE)
	{
		return FALSE;
	}
	if (data->dwSlotCount > 0 && data->pSlots[0].pLayout != NULL)
	{
		CtrlInfo* pWnd = data->pSlots[0].pLayout->GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL)
		{
			data->nWndX = (LONG)pWnd->position.x;
			data->nWndY = (LONG)pWnd->position.y;
		}
	}
	if (x != NULL) { *x = data->nWndX; }
	if (y != NULL) { *y = data->nWndY; }
	if (w != NULL) { *w = data->dwWndW; }
	if (h != NULL) { *h = data->dwWndH; }
	return TRUE;
}

// 函数: GetExitAddress
// 描述: 取退出判据地址(宿主主循环每帧只读; 未就绪时返回 NULL)
CONST BOOL* Dispatch::GetExitAddress()
{
	if (data == NULL)
	{
		return NULL;
	}
	return &data->bExit;
}

// 函数: IsReady
// 描述: 调度层是否就绪(成员块已分配且至少一枚布局已装配)
BOOL Dispatch::IsReady()
{
	if (data != NULL && data->bReady != FALSE)
	{
		return TRUE;
	}
	return FALSE;
}

// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return powf(fRatio, fExponent);
}

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return 1.0f - powf(1.0f - fRatio, fExponent);
}

// 函数: EnsureProjCR
// 描述: 确保投影配色已建立并跟随当前亮暗口径(跟随系统或 SetDarkMode 登记值; 反复调用幂等)
VOID Dispatch::EnsureProjCR()
{
	if (data == NULL)
	{
		return;
	}
	CONST BOOL bDark = (data->bDarkFollowSystem != FALSE)
		? (AppEnv::IsSystemDarkMode() ? TRUE : FALSE) : data->bDarkMode;
	CONST ProjCR::SAMPLE_DIRECTION eDir =
		(bDark != FALSE) ? ProjCR::PROJ_DARK_TO_LIGHT : ProjCR::PROJ_LIGHT_TO_DARK;
	CONST ProjCR::SampleFunc pfnSample =
		(eDir == ProjCR::PROJ_DARK_TO_LIGHT) ? ProjSampleConvex : ProjSampleConcave;
	if (data->pProjCR == NULL)
	{
		ProjCR::Create(data->pProjCR, PROJ_COLOR_BASE, PROJ_COLOR_LIGHT, eDir, PROJ_LAYERS, pfnSample);
		return;
	}
	data->pProjCR->SetDirection(eDir);
	data->pProjCR->SetSampleFunc(pfnSample);
}

// 函数: ApplyHostStyle
// 描述: 按参考尺寸与投影配色填宿主样式描述(圆角与 17 个配色槽); 配色未就绪时返回 FALSE
BOOL Dispatch::ApplyHostStyle(StyleDesc& desc)
{
	CONST FLOAT fSpace = GetRef().GetSpace<FLOAT>();
	CONST FLOAT fPadding = GetRef().GetPadding<FLOAT>();
	desc.fWindowRounding = fSpace;
	desc.fFrameRounding = fPadding;
	desc.fChildRounding = fPadding;
	desc.fPopupRounding = fPadding;
	ProjCR* pCR = data->pProjCR;
	if (pCR == NULL)
	{
		return FALSE;
	}
	desc.crWindowBg = pCR->Sample(PROJ_L_WINDOW);
	desc.crChildBg = pCR->Sample(PROJ_L_CHILD);
	desc.crFrameBg = pCR->Sample(PROJ_L_FRAME);
	desc.crFrameBgHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crFrameBgActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	desc.crButton = pCR->Sample(PROJ_L_FRAME);
	desc.crButtonHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crButtonActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	desc.crBorder = pCR->SampleBorder(PROJ_L_CHILD, PROJ_L_FRAME);
	desc.crText = pCR->Sample(PROJ_L_WINDOW, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crTextDisabled = pCR->Sample(PROJ_L_DISABLED);
	desc.crSliderGrab = pCR->Sample(PROJ_L_FRAME);
	desc.crSliderGrabActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crCheckMark = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crHeader = pCR->Sample(PROJ_L_FRAME);
	desc.crHeaderHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crHeaderActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	return TRUE;
}

// 函数: ShowStateName
// 描述: 取显示状态的中文短名(渲染自证用)
static LPCSTR ShowStateName(WndShowState state)
{
	switch (state)
	{
	case WND_SHOW_VISIBLE:			return "可见";
	case WND_SHOW_HIDDEN_BY_ACTIVE:	return "被激活策略隐藏";
	case WND_SHOW_HIDDEN_BY_HOST:	return "被宿主隐藏";
	default:						return "从未显示";
	}
}

// 函数: PrintWndGeometry
// 描述: 打印首帧前的建窗几何(宿主 Start 默认几何的实测值; 首帧渲染前窗口尚未按文档几何结算)
VOID Dispatch::PrintWndGeometry()
{
	RECT rcWnd{};
	RECT rcClient{};
	if (data == NULL || data->pHost == NULL || data->pHost->GetWndRect(0, &rcWnd, &rcClient) == FALSE)
	{
		return;
	}
	Log("建窗几何: %ld,%ld %ldx%ld (来源: Start 默认几何)\n",
		(LONG)rcWnd.left, (LONG)rcWnd.top,
		(LONG)(rcClient.right - rcClient.left), (LONG)(rcClient.bottom - rcClient.top));
}

// 函数: RefreshWndShowCache
// 描述: 刷新最近一帧的窗口显示状态缓存(收尾自证在窗口销毁后仍可打印当时的可见性)
VOID Dispatch::RefreshWndShowCache()
{
	if (data == NULL || data->pHost == NULL)
	{
		return;
	}
	DWORD dwStandard = data->pHost->GetStandardWndCount();
	DWORD dwVisible = 0;
	for (DWORD i = 0; i < dwStandard; i++)
	{
		if (data->pHost->GetWndShowState(i) == WND_SHOW_VISIBLE)
		{
			dwVisible++;
		}
	}
	data->dwLastShowState = (DWORD)data->pHost->GetWndShowState(0);
	data->dwLastVisible = dwVisible;
}

// 函数: PrintRenderProof
// 描述: 渲染自证 —— 打印绘制量(渲染段数 / 绘制命令数 / 顶点数 / 索引数)、窗口尺寸调整次数、控件数
VOID Dispatch::PrintRenderProof(LPCSTR lpszTag)
{
	DWORD dwSegments = 0;
	DWORD dwCmdLists = 0;
	INT64 llVtx = 0;
	INT64 llIdx = 0;
	DWORD dwResizes = 0;
	DWORD dwLastW = 0;
	DWORD dwLastH = 0;
	DWORD dwStandard = 0;
	if (data != NULL && data->pHost != NULL)
	{
		dwStandard = data->pHost->GetStandardWndCount();
		data->pHost->GetRenderStats(&dwSegments, &dwCmdLists, &llVtx, &llIdx);
		data->pHost->GetResizeStats(&dwResizes, &dwLastW, &dwLastH);
	}
	Log("渲染自证(%s): 帧=%u 段=%u 绘制命令=%u 顶点=%lld 索引=%lld 窗口调整=%u(最近 %ux%u) 控件=%zu 窗口显示=%s 可见=%u/%u\n",
		(lpszTag != NULL) ? lpszTag : "-",
		(unsigned)((data != NULL) ? data->dwFrameCount : 0), (unsigned)dwSegments,
		(unsigned)dwCmdLists, (long long)llVtx, (long long)llIdx,
		(unsigned)dwResizes, (unsigned)dwLastW, (unsigned)dwLastH, (size_t)CountTotalCtrls(),
		ShowStateName((WndShowState)((data != NULL) ? data->dwLastShowState : 0)),
		(unsigned)((data != NULL) ? data->dwLastVisible : 0), (unsigned)dwStandard);
}

// 函数: Log
// 描述: 日志输出 —— 统一前缀 [<配置前缀>] 并按行刷新
VOID Dispatch::Log(LPCSTR lpszFormat, ...)
{
	va_list args;
	printf("[%s] ", szLogPrefix);
	va_start(args, lpszFormat);
	vprintf(lpszFormat, args);
	va_end(args);
	fflush(stdout);
}
