#pragma once
#include <windows.h>

namespace AppEnv
{
	// 函数: InitConsoleUtf8
	// 描述: 把控制台输入与输出代码页都设为 UTF-8
	VOID InitConsoleUtf8();

	// 函数: IsSystemDarkMode
	// 描述: 查询系统应用主题是否为深色
	bool IsSystemDarkMode();

	// 函数: ApplyWindowCornerRounding
	// 描述: 为窗口应用圆角(优先 DWM 属性, 失败回退圆角区域裁剪)
	VOID ApplyWindowCornerRounding(HWND hWnd, DWORD w, DWORD h, DWORD space, BOOL* pbByRgn);
}
