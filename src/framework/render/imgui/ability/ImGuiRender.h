#pragma once
#include "framework/render/Render.h"

namespace ImGuiRender
{
	// 函数: GetApi
	// 描述: 取本实现填好的渲染能力表(函数指针表的唯一真源)
	CONST RenderApi* GetApi();
}
