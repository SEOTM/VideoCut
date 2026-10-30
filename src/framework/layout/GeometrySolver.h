#pragma once
#include <windows.h>
#include "framework/base/BasicDesign.h"
#include "framework/base/ControlSpec.h"
#include "framework/layout/LayoutModel.h"

namespace GeometrySolver
{
	// 函数: AutoModeNameOf
	// 描述: 取 autosize 模式枚举对应的规范名(越界 ⇒ 屏高模式名)
	LPCSTR AutoModeNameOf(INT mode);
	// 函数: AutoModeIndex
	// 描述: 取 autosize 模式串对应的 AutoSize::AutoSizeMode 枚举索引(未命中/NULL ⇒ 屏高模式)
	INT AutoModeIndex(LPCSTR mode);

	// 函数: EntryBasePx
	// 描述: 取布局 autosize 的换算基准像素(与控件量同源)
	DWORD EntryBasePx(INT mode);
	// 函数: EntryRefSizeInfo
	// 描述: 依参考尺寸/经验参数(中间态 RefSize)推导各参考量像素值(窗口化: 布局窗口像素与控件量同源推导)
	AutoSize::SizeInfo<DWORD> EntryRefSizeInfo(CONST RefSize& refSize, DWORD base, DWORD wndW, DWORD wndH);
	// 函数: RefValueToRatio
	// 描述: 把"该条目单位下的参考量输入值"折算为中间态存储的千分比(单位只决定入参解释, 存储恒千分比)
	DWORD RefValueToRatio(DWORD value, DWORD base, INT unit);

	// 函数: FactorAbsPx
	// 描述: 由控件几何因子(SizeFactor) + 参考尺寸求像素: Σ(coeff[i] * ref[i]) + offset
	FLOAT FactorAbsPx(CONST SizeFactor& f, CONST AutoSize::SizeInfo<FLOAT>& ref);
	// 函数: ApplySpecGeometry
	// 描述: 依中间态元素规格与参考尺寸就地解算一枚控件的像素几何(整条重解与单枚重解共用本式)
	VOID ApplySpecGeometry(CtrlInfo& info, CONST ElementSpec* pSpec, CONST AutoSize::SizeInfo<FLOAT>& ref);

	// 函数: ResolveWndPos
	// 描述: 解算窗口件的位置记录(解析期几何解算的最后一步; 随机 / 采用声明 / 越屏降级三分支)
	VOID ResolveWndPos(CtrlInfo* pWnd);
}
