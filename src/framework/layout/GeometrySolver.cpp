#include "framework/layout/GeometrySolver.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

// 函数: AutoModeNameOf
// 描述: 取自动尺寸模式枚举对应的规范名(越界返回屏高模式名)
LPCSTR GeometrySolver::AutoModeNameOf(INT mode)
{
	static const LPCSTR Names[4] = {
		"AUTO_SIZE_BY_SCREEN_WIDTH",
		"AUTO_SIZE_BY_SCREEN_HEIGHT",
		"AUTO_SIZE_BY_WINDOW_WIDTH",
		"AUTO_SIZE_BY_WINDOW_HEIGHT"
	};
	if (mode < 0 || mode > 3)
	{
		mode = AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
	}
	return Names[mode];
}
// 函数: AutoModeIndex
// 描述: 取自动尺寸模式串对应的枚举索引(未命中/NULL ⇒ 屏高模式)
INT GeometrySolver::AutoModeIndex(LPCSTR mode)
{
	if (mode == NULL)
	{
		return AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
	}
	for (INT m = 0; m < 4; m++)
	{
		if (strcmp(mode, AutoModeNameOf(m)) == 0)
		{
			return m;
		}
	}
	return AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
}

// 函数: EntryBasePx
// 描述: 取布局 autosize 的换算基准像素(见 .h; WINDOW_* 模式按屏兜底)
DWORD GeometrySolver::EntryBasePx(INT mode)
{
	switch (mode)
	{
	case AutoSize::AUTO_SIZE_BY_SCREEN_WIDTH:	return ScreenRef::GetScreenWidth();
	case AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT:	return ScreenRef::GetScreenHeight();
	case AutoSize::AUTO_SIZE_BY_WINDOW_WIDTH:	return ScreenRef::GetScreenWidth();
	case AutoSize::AUTO_SIZE_BY_WINDOW_HEIGHT:	return ScreenRef::GetScreenHeight();
	default:									return ScreenRef::GetScreenHeight();
	}
}
// 函数: EntryRefSizeInfo
// 描述: 依参考尺寸/经验参数推导各参考量像素值(窗口化: 布局窗口像素与控件量同源推导)
AutoSize::SizeInfo<DWORD> GeometrySolver::EntryRefSizeInfo(CONST RefSize& refSize, DWORD base, DWORD wndW, DWORD wndH)
{
	AutoSize::SizeInfo<DWORD> si{};
	si.screenWidth = ScreenRef::GetScreenWidth();
	si.screenHeight = ScreenRef::GetScreenHeight();
	si.windowWidth = (wndW > 0) ? wndW : si.screenWidth;
	si.windowHeight = (wndH > 0) ? wndH : si.screenHeight;
	if (base < 1) { base = 1; }
	si.controlHeight = (DWORD)((unsigned __int64)base * refSize.controlHeight / 1000); if (si.controlHeight < 1) { si.controlHeight = 1; }
	si.fontSize = (DWORD)((unsigned __int64)base * refSize.fontSize / 1000); if (si.fontSize < 1) { si.fontSize = 1; }
	si.space = (DWORD)((unsigned __int64)base * refSize.space / 1000); if (si.space < 1) { si.space = 1; }
	si.padding = (DWORD)((unsigned __int64)base * refSize.padding / 1000); if (si.padding < 1) { si.padding = 1; }
	return si;
}
// 函数: RefValueToRatio
// 描述: 把"该条目单位下的参考量输入值"折算为中间态存储的千分比
DWORD GeometrySolver::RefValueToRatio(DWORD value, DWORD base, INT unit)
{
	DWORD v = value;
	if (unit != 0)
	{
		if (base < 1) { base = 1; }
		v = (DWORD)(((unsigned __int64)value * 1000 + (base / 2)) / base);
	}
	if (v > 10000) { v = 10000; }
	return v;
}

// 函数: FactorAbsPx
// 描述: 由控件几何因子 + 参考尺寸求像素: Σ(coeff[i]*ref[i]) + offset
FLOAT GeometrySolver::FactorAbsPx(CONST SizeFactor& f, CONST AutoSize::SizeInfo<FLOAT>& ref)
{
	CONST FLOAT Refs[GENFACTOR_NUM] = {
		ref.screenWidth, ref.screenHeight, ref.windowWidth, ref.windowHeight,
		ref.controlHeight, ref.fontSize, ref.space, ref.padding
	};
	DOUBLE sum = f.offset;
	for (int i = 0; i < GENFACTOR_NUM; i++)
	{
		sum += (DOUBLE)f.coeff.RefCoeffs[i] * Refs[i];
	}
	return (FLOAT)sum;
}
// 函数: ApplySpecGeometry
// 描述: 依中间态元素规格与参考尺寸就地解算一枚控件的像素几何
VOID GeometrySolver::ApplySpecGeometry(CtrlInfo& info, CONST ElementSpec* pSpec, CONST AutoSize::SizeInfo<FLOAT>& ref)
{
	if (pSpec == NULL)
	{
		return;
	}
	info.posDeclared.x = FactorAbsPx(pSpec->x, ref);
	info.posDeclared.y = FactorAbsPx(pSpec->y, ref);
	info.size.x = FactorAbsPx(pSpec->width, ref);
	info.size.y = FactorAbsPx(pSpec->height, ref);
	info.position = info.posDeclared;
}

constexpr FLOAT WND_POS_RANDOM = -1.0f;

// 函数: RandRange
// 描述: 取 [lo, hi] 内的均匀随机整数(区间为空/倒置 ⇒ 返回 lo)
static INT RandRange(INT lo, INT hi)
{
	if (hi <= lo)
	{
		return lo;
	}
	return lo + (INT)((unsigned)rand() % (unsigned)(hi - lo + 1));
}

// 函数: MakeRandomWndPos
// 描述: 在主显示器工作区内取一枚随机落点(窗口完整落在工作区内)
static vec2 MakeRandomWndPos(CONST vec2& size)
{
	static BOOL bSeeded = FALSE;
	if (bSeeded == FALSE)
	{
		srand((unsigned int)GetTickCount64());
		bSeeded = TRUE;
	}
	RECT rcWork{};
	if (ScreenRef::GetPrimaryWorkArea(&rcWork) == FALSE)
	{
		return vec2(0.0f, 0.0f);
	}
	CONST INT nX = RandRange((INT)rcWork.left, (INT)rcWork.right - (INT)size.x);
	CONST INT nY = RandRange((INT)rcWork.top, (INT)rcWork.bottom - (INT)size.y);
	return vec2((FLOAT)nX, (FLOAT)nY);
}

// 函数: ResolveWndPos
// 描述: 解算窗口件的位置记录(解析期几何解算的最后一步; 三分支)
VOID GeometrySolver::ResolveWndPos(CtrlInfo* pWnd)
{
	if (pWnd == NULL)
	{
		return;
	}
	if (pWnd->position.x == WND_POS_RANDOM && pWnd->position.y == WND_POS_RANDOM)
	{
		pWnd->position = MakeRandomWndPos(pWnd->size);
	}
	else
	{
		RECT rcWnd{};
		rcWnd.left = (LONG)pWnd->position.x;
		rcWnd.top = (LONG)pWnd->position.y;
		rcWnd.right = rcWnd.left + (LONG)pWnd->size.x;
		rcWnd.bottom = rcWnd.top + (LONG)pWnd->size.y;
		if (ScreenRef::IsRectOnAnyMonitor(rcWnd) == FALSE)
		{
			pWnd->position = MakeRandomWndPos(pWnd->size);
		}
	}
	pWnd->posDeclared = pWnd->position;
}
