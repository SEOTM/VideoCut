#pragma once
#include <minwindef.h>
#include "framework/base/BasicDesign.h"
#include "framework/layout/Layout.h"
#include "framework/base/FuncPtr.h"
#include "framework/render/Render.h"

namespace LayoutRender
{

	// 函数: SetRenderApi
	// 描述: 注入渲染能力表(幂等; 传 NULL 清除注入)
	VOID SetRenderApi(CONST RenderApi* pApi);

	// 函数: GetRenderApi
	// 描述: 取当前注入的渲染能力表
	CONST RenderApi* GetRenderApi();

	// 函数: SetCtrlApi
	// 描述: 注入控件绘制接口表(幂等; 传 NULL 清除注入)
	VOID SetCtrlApi(CONST CtrlApi* pApi);

	// 函数: GetCtrlApi
	// 描述: 取当前注入的控件绘制接口表
	CONST CtrlApi* GetCtrlApi();

	// 函数: RenderBase
	// 描述: 基础渲染: 渲染占一个标准窗口的基座布局
	BOOL RenderBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, BOOL bNoInputs = FALSE);

	// 函数: RenderPreviewBase
	// 描述: 基础渲染之上为内容控件逐枚做点击判定, 并可插入视图叠加件
	BOOL RenderPreviewBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, pfnCtrlEvent pfnClick,
		pfnOverlayBefore pfnOverlay = NULL);

	// 函数: RenderFlex
	// 描述: 渲染无独立窗口句柄的内嵌(flex)布局
	BOOL RenderFlex(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj);

	// 函数: IsIMEDisabled
	// 描述: 检查当前是否需要禁用 IME 输入法
	BOOL IsIMEDisabled();
}
