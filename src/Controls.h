#pragma once
#include "Layout.h"
#include "BasicDesign.h"
// ================================
// 库名: Controls
// 描述: 封装 Dear ImGui 控件的自定义控件库，并包含一些辅助函数和 Render 绘制函数
// ================================

namespace Controls
{
	// 函数: CheckWindowResize
	// 描述: 检测布局变化是否需要调整窗口尺寸，若需要则返回建议尺寸
	// 参数:
	//      IN  layout: 布局对象指针
	//      OUT size:   若返回 TRUE，则此处填充建议的窗口尺寸
	// 返回:
	//      TRUE:  当前布局需要调整窗口大小
	//      FALSE: 当前布局与窗口尺寸匹配，无需调整
	BOOL CheckWindowResize(Layout& layout, vec2* size);
	
	// 函数: RenderMain
	// 描述: 渲染主要窗口布局
	// 参数:
	//		IN	layout:			布局对象指针
	//		IN	refsize:			参考尺寸对象引用
	// 返回:
	//		BOOL: 返回 TRUE 表示渲染成功，FALSE 表示渲染失败
	BOOL RenderMain(Layout& layout,CONST AutoSize& refsize);

	// 函数: RenferChild
	// 描述: 渲染子窗口布局
	BOOL RenderChild(Layout& layout);

	// 函数: IsIMEDisabled
	// 描述: 检查当前是否需要禁用 IME 输入法
	BOOL IsIMEDisabled();
}

// 注: 
// 头文件仅暴露少数接口相关的函数，具体的控件的定义和实现将在 Controls.cpp 中完成
// 控件类型常量的定义由 Layout.h 中的 CtrlType 枚举类型提供，控件信息结构体由 Layout.h 中的 CtrlInfo 结构体提供
// Controls.cpp 中的控件实现将使用 Dear ImGui 的 API 来实现具体的控件行为