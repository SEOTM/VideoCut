#include <windows.h>
#include <stdio.h>

#include "framework/layout/LayoutRender.h"
#include "framework/base/ControlSpec.h"

static CONST RenderApi* pRenderApi = NULL;
static CONST CtrlApi* pCtrlApi = NULL;

static VOID PushClickProbeItem(CtrlInfo* ctrl, CONST vec2& orgPanel);
static VOID DispatchCtrlClick(CtrlInfo* ctrl, size_t index, uint64_t qwItemIdBefore, CONST vec2& orgContent,
	CONST vec2& orgPanel, pfnCtrlEvent pfnClick);
static BOOL RenderLayoutCore(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, DWORD dwStates,
	pfnCtrlEvent pfnClick, pfnOverlayBefore pfnOverlay);

// 函数: PushClickProbeItem
// 描述: 在控件矩形上经能力表补一枚无绘制命中项(矩形换算到内容容器本地坐标)
static VOID PushClickProbeItem(CtrlInfo* ctrl, CONST vec2& orgPanel)
{
	pRenderApi->AddHitArea(ctrl->label, ctrl->position - orgPanel, ctrl->size);
}

// 函数: DispatchCtrlClick
// 描述: 逐控件点击判定: 本帧命中即把点击事件派发给记录器
static VOID DispatchCtrlClick(CtrlInfo* ctrl, size_t index, uint64_t qwItemIdBefore, CONST vec2& orgContent,
	CONST vec2& orgPanel, pfnCtrlEvent pfnClick)
{
	if (ctrl == NULL || pfnClick == NULL ||
		ctrl->size.x <= 0.0f || ctrl->size.y <= 0.0f ||
		ctrl->type == CTRL_TYPE_INVALID)
	{
		return;
	}
	if (CtrlTypeHandlesClickItself(ctrl->type) == TRUE && ctrl->pEventHandler != NULL)
	{
		return;
	}
	if (CtrlTypeIsRawGraphic(ctrl->type) == TRUE)
	{
		PushClickProbeItem(ctrl, orgPanel);
	}
	else
	{
		CONST BOOL bHasItem = (pRenderApi->TakeItemId() != qwItemIdBefore) ? TRUE : FALSE;
		if (bHasItem == FALSE)
		{
			PushClickProbeItem(ctrl, orgPanel);
		}
	}
	if (pRenderApi->IsHitClicked() == TRUE)
	{
		pfnClick(ctrl, OperCode::MakeCodeCommon(OPER_COMMON_CLICKED), (uint64_t)index, ctrl->bufferInfo.buffer);
	}
	else if (CtrlTypeIsCompound(ctrl->type) == TRUE)
	{
		CONST vec2 mn(orgContent.x + (ctrl->position.x - orgPanel.x),
			orgContent.y + (ctrl->position.y - orgPanel.y));
		CONST vec2 mx(mn.x + ctrl->size.x, mn.y + ctrl->size.y);
		if (pRenderApi->IsRectClicked(mn, mx) == TRUE)
		{
			pfnClick(ctrl, OperCode::MakeCodeCommon(OPER_COMMON_CLICKED), (uint64_t)index, ctrl->bufferInfo.buffer);
		}
	}
}

// 函数: RenderLayoutCore
// 描述: 布局渲染核心(文件内部): 开窗 + 标题栏 + 内容容器, 并遍历渲染内容控件
static BOOL RenderLayoutCore(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, DWORD dwStates,
	pfnCtrlEvent pfnClick, pfnOverlayBefore pfnOverlay)
{
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	CtrlInfo* pWnd = NULL;
	CtrlInfo* pCntr = NULL;
	vec2 orgContent(0.0f, 0.0f);
	vec2 orgPanel(0.0f, 0.0f);
	if (pRenderApi == NULL || pCtrlApi == NULL)
	{
		goto RENDER_END;
	}
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	num = layout.GetTotalNum();
	pWnd = layout.GetLayoutBase(CTRL_TYPE_WND);
	pCntr = layout.GetLayoutBase(CTRL_TYPE_CNTR);
	if (pWnd == NULL || pCntr == NULL)
	{
		goto RENDER_END;
	}
	orgPanel = vec2(0.0f, pCntr->position.y);
	pCtrlApi->UpdateContext(refsize, pProj, &pWnd->position, orgPanel);
	if (pRenderApi->AcquireWnd(pWnd->position, pWnd->size, pWnd->label) == FALSE)
	{
		goto RENDER_END;
	}
	pRenderApi->OpenWnd(pWnd->label, vec2(0.0f, 0.0f), pWnd->size, dwStates);
	{
		ctrl = layout.GetLayoutBase(CTRL_TYPE_TITLE);
		if (ctrl)
		{
			pCtrlApi->DrawCtrl(ctrl);
		}
	}
	pRenderApi->OpenPanel(pCntr->label, orgPanel, pCntr->size, 0x00000000);
	orgContent = pRenderApi->GetContentOrigin();
	{
		for (size_t i = layout.GetShellNum(); i < num; i++)
		{
			if (pfnOverlay != NULL)
			{
				pfnOverlay(layout, i);
			}
			ctrl = layout.GetCtrlInfo(i);
			if (ctrl)
			{
				pRenderApi->PushId(ctrl->id);
				CONST uint64_t qwItemIdBefore = pRenderApi->TakeItemId();
				pCtrlApi->DrawCtrl(ctrl);
				DispatchCtrlClick(ctrl, i, qwItemIdBefore, orgContent, orgPanel, pfnClick);
				pRenderApi->PopId();
			}
		}
	}
	pRenderApi->ClosePanel();
	pRenderApi->CloseWnd();
	bRet = TRUE;
RENDER_END:
	return bRet;
}

namespace LayoutRender
{

// 函数: SetRenderApi
// 描述: 注入渲染能力表(幂等; 传 NULL 清除注入)
VOID SetRenderApi(CONST RenderApi* pApi)
{
	pRenderApi = pApi;
}

// 函数: GetRenderApi
// 描述: 取当前注入的渲染能力表
CONST RenderApi* GetRenderApi()
{
	return pRenderApi;
}

// 函数: SetCtrlApi
// 描述: 注入控件绘制接口表(幂等; 传 NULL 清除注入)
VOID SetCtrlApi(CONST CtrlApi* pApi)
{
	pCtrlApi = pApi;
}

// 函数: GetCtrlApi
// 描述: 取当前注入的控件绘制接口表
CONST CtrlApi* GetCtrlApi()
{
	return pCtrlApi;
}

// 函数: RenderBase
// 描述: 基础渲染: 渲染占一个标准窗口的基座布局
BOOL RenderBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, BOOL bNoInputs)
{
	CONST DWORD dwStates = RENDER_WND_BASE | ((bNoInputs != FALSE) ? RENDER_WND_NOINPUT : 0);
	return RenderLayoutCore(layout, refsize, pProj, dwStates, NULL, NULL);
}

// 函数: RenderPreviewBase
// 描述: 基础渲染之上为内容控件逐枚做点击判定, 并可插入视图叠加件
BOOL RenderPreviewBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, pfnCtrlEvent pfnClick,
	pfnOverlayBefore pfnOverlay)
{
	return RenderLayoutCore(layout, refsize, pProj, RENDER_WND_BASE, pfnClick, pfnOverlay);
}

// 函数: RenderFlex
// 描述: 渲染无独立窗口句柄的内嵌(flex)布局(沿用当前窗口)
BOOL RenderFlex(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj)
{
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	CtrlInfo* pCntr = NULL;
	vec2 orgPanel(0.0f, 0.0f);
	CHAR szWndName[128]{};
	if (pRenderApi == NULL || pCtrlApi == NULL)
	{
		goto RENDER_END;
	}
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	num = layout.GetTotalNum();
	pCntr = layout.GetLayoutBase(CTRL_TYPE_CNTR);
	if (pCntr == NULL)
	{
		goto RENDER_END;
	}
	orgPanel = vec2(0.0f, pCntr->position.y);
	pCtrlApi->UpdateContext(refsize, pProj, NULL, orgPanel);
	ctrl = layout.GetLayoutBase(CTRL_TYPE_WND);
	if (ctrl == NULL || ctrl->label == NULL)
	{
		goto RENDER_END;
	}
	snprintf(szWndName, sizeof(szWndName), "%s##FLEX%p", ctrl->label, (void*)&layout);
	pRenderApi->OpenWnd(szWndName, ctrl->position, ctrl->size, RENDER_WND_BASE);
	pRenderApi->PushPtrId((CONST VOID*)&layout);
	{
		ctrl = layout.GetLayoutBase(CTRL_TYPE_TITLE);
		if (ctrl)
		{
			pCtrlApi->DrawCtrl(ctrl);
		}
	}
	pRenderApi->OpenPanel(pCntr->label, orgPanel, pCntr->size, 0x00000000);
	{
		for (size_t i = layout.GetShellNum(); i < num; i++)
		{
			ctrl = layout.GetCtrlInfo(i);
			if (ctrl)
			{
				pRenderApi->PushId(ctrl->id);
				pCtrlApi->DrawCtrl(ctrl);
				pRenderApi->PopId();
			}
		}
	}
	pRenderApi->ClosePanel();
	pRenderApi->PopId();
	pRenderApi->CloseWnd();
	pRenderApi->BringWndToFront();
	bRet = TRUE;
RENDER_END:
	return bRet;
}

// 函数: IsIMEDisabled
// 描述: 检查当前是否需要禁用 IME 输入法(本帧存在编辑中的输入控件即禁用)
BOOL IsIMEDisabled()
{
	if (pCtrlApi == NULL)
	{
		return FALSE;
	}
	return pCtrlApi->HasActiveInput();
}

}
