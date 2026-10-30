#define IMGUI_DEFINE_MATH_OPERATORS

#include <windows.h>
#include "framework/render/imgui/control/Controls.h"
#include "framework/render/imgui/control/Widgets.h"
#include "framework/base/CtrlBuffer.h"
#include "framework/render/imgui/ImGuiHost.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "framework/base/TextWidth.h"
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <algorithm>

static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder);
static VOID DrawUnknownCtrlRect(CtrlInfo* info);
static uint16_t CtrlClickCode(CtrlInfo* info, LPCSTR lpFallback);
static ListBuffer* ListBufferOf(CtrlInfo* info);
static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam);

// 函数: toImvec2
// 描述: 内置类型转化为 imgui 可识别类型
template<bool safe = false>
auto toImvec2(const vec2& vec)
{
	if constexpr (safe)
	{
		return ImVec2(vec.x, vec.y);
	}
	else
	{
		return reinterpret_cast<const ImVec2&>(vec);
	}
}

namespace Controls
{

	static CHAR ERRBUFFER[16] = u8"无效缓冲";

	constexpr BOOL GRAB_BASE_BY_WND_POS = TRUE;

	static auto sizeinfo = AutoSize::SizeInfoFLOAT{};
	static FLOAT fRounding = 0.0f;
	static CONST ProjCR* pcrHander = NULL;
	static CONST vec2* pWndPosSlot = NULL;
	static vec2 orgContent(0.0f, 0.0f);

	constexpr INT32 PROJ_L_DISABLED = 10;
	constexpr INT32 PROJ_L_BORDER = 9;
	constexpr INT32 PROJ_L_BORDER_HOVER = 11;
	constexpr INT32 PROJ_L_BORDER_ACTIVE = 13;

	// 函数: CtrlLayer
	// 描述: 取某控件在投影上的绘图层号(= PROJ_LAYER_BACKGROUND + CtrlInfo::nLayer; 唯一换算口径)
	static INT32 CtrlLayer(CONST CtrlInfo* pCtrl)
	{
		static BOOL bClampReported = FALSE;
		INT32 nRel = (pCtrl != NULL) ? pCtrl->nLayer : 0;
		INT32 nRet = CtrlLayerOf(nRel);
		if (nRet != (PROJ_LAYER_BACKGROUND + nRel) && bClampReported == FALSE)
		{
			bClampReported = TRUE;
			printf(u8"[LAYER] 层次越界: nLayer=%d ⇒ 层号钳到 %d(层号域 [%d, %d])\n",
				(int)nRel, (int)nRet, (int)PROJ_LAYER_MIN, (int)PROJ_LAYER_MAX);
			fflush(stdout);
		}
		return nRet;
	}

	// 函数: CtrlRel
	// 描述: 取某控件的层次值(相对量; CtrlInfo::nLayer 原样; 有符号)
	static INT32 CtrlRel(CONST CtrlInfo* pCtrl)
	{
		return (pCtrl != NULL) ? pCtrl->nLayer : 0;
	}

	// 函数: ClampProjLayer
	// 描述: 把绘图层号夹到合法域(投影轴表的下标防御; 取色路径的统一钳制口径)
	static inline INT32 ClampProjLayer(INT32 nLayer)
	{
		return (nLayer < PROJ_LAYER_MIN) ? PROJ_LAYER_MIN :
			((nLayer > PROJ_LAYER_MAX) ? PROJ_LAYER_MAX : nLayer);
	}

	// 函数: ProjColor
	// 描述: 取本帧投影配色的槽位色(控件库取色的唯一入口; 投影未注入时返回 0)
	static inline DWORD ProjColor(INT32 nLayer, ProjCR::CR_SLOT eSlot = ProjCR::PROJ_SLOT_SURFACE)
	{
		return (pcrHander != NULL) ? pcrHander->Sample((DWORD)ClampProjLayer(nLayer), eSlot) : 0;
	}

	enum PALETTE_STYLE
	{
		PALETTE_FLAT = 0,
		PALETTE_SUNKEN = 1,
		PALETTE_RAISED = 2,
		PALETTE_SUNKEN_RAISED = 3,
		PALETTE_RAISED_SUNKEN = 4
	};

	struct PaletteStyleDesc
	{
		PALETTE_STYLE eStyle;
		LPCSTR lpName;
		INT32 nFaceOffset;
		INT32 nSurfaceOffset;
		INT32 nBorderNeighborOffset;
		BOOL IsInnerEdge;
	};

	static constexpr PaletteStyleDesc PaletteStyles[] = {
		{ PALETTE_FLAT,				u8"平坦",		0,	0,	0,	FALSE },
		{ PALETTE_SUNKEN,			u8"下沉",		-1,	-1,	1,	FALSE },
		{ PALETTE_RAISED,			u8"上浮",		1,	1,	-1,	FALSE },
		{ PALETTE_SUNKEN_RAISED,	u8"下沉含上浮",	-1,	0,	1,	TRUE },
		{ PALETTE_RAISED_SUNKEN,	u8"上浮含下沉",	1,	0,	-1,	TRUE },
	};
	constexpr size_t PALETTE_STYLE_NUM = sizeof(PaletteStyles) / sizeof(PaletteStyles[0]);
	static_assert(PALETTE_STYLE_NUM == 5, "配色样式表必须覆盖五种样式");
	static_assert(PaletteStyles[PALETTE_FLAT].eStyle == PALETTE_FLAT, "配色样式表: 平坦样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN].eStyle == PALETTE_SUNKEN, "配色样式表: 下沉样式行错位");
	static_assert(PaletteStyles[PALETTE_RAISED].eStyle == PALETTE_RAISED, "配色样式表: 上浮样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN_RAISED].eStyle == PALETTE_SUNKEN_RAISED, "配色样式表: 下沉含上浮样式行错位");
	static_assert(PaletteStyles[PALETTE_RAISED_SUNKEN].eStyle == PALETTE_RAISED_SUNKEN, "配色样式表: 上浮含下沉样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN_RAISED].nFaceOffset != PaletteStyles[PALETTE_SUNKEN_RAISED].nSurfaceOffset,
		"配色样式表: 下沉含上浮样式必须两层面错开");
	static_assert(PaletteStyles[PALETTE_RAISED_SUNKEN].nFaceOffset != PaletteStyles[PALETTE_RAISED_SUNKEN].nSurfaceOffset,
		"配色样式表: 上浮含下沉样式必须两层面错开");

	// 函数: PaletteStyleOf
	// 描述: 取配色样式的表项(样式值未知 ⇒ 回退平坦样式, 取色路径不因样式越界而失败)
	static CONST PaletteStyleDesc& PaletteStyleOf(PALETTE_STYLE eStyle)
	{
		for (size_t i = 0; i < PALETTE_STYLE_NUM; i++)
		{
			if (PaletteStyles[i].eStyle == eStyle)
			{
				return PaletteStyles[i];
			}
		}
		return PaletteStyles[0];
	}

	// 函数: MakePalette
	// 描述: 构造部件配色方案(各绘制件共用的统一入口; 按控件层次与配色样式采样)
	static Widgets::Base::Palette MakePalette(INT32 nRel, PALETTE_STYLE eStyle)
	{
		CONST PaletteStyleDesc& style = PaletteStyleOf(eStyle);
		CONST INT32 nFaceLayer = CtrlLayerOf(nRel + style.nFaceOffset);
		CONST INT32 nSurfaceLayer = CtrlLayerOf(nRel + style.nSurfaceOffset);
		CONST INT32 nBorderNearLayer = ClampProjLayer(nFaceLayer + style.nBorderNeighborOffset);
		CONST DWORD crFaceContrast = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_CONTRAST);
		CONST DWORD crSurfaceContrast = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_CONTRAST);
		CONST DWORD crDisabled = ProjColor(PROJ_L_DISABLED);
		Widgets::Base::Palette palette{};
		palette.crBackground.crBase = ProjColor(nFaceLayer);
		palette.crBackground.crHovered = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crBackground.crActive = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crBackground.crDisable = crDisabled;
		palette.crForeground.crBase = crFaceContrast;
		palette.crForeground.crHovered = crFaceContrast;
		palette.crForeground.crActive = crFaceContrast;
		palette.crForeground.crDisable = crDisabled;
		palette.crSurface.crBase = ProjColor(nSurfaceLayer);
		palette.crSurface.crHovered = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = crDisabled;
		palette.crSurfaceForeground.crBase = crSurfaceContrast;
		palette.crSurfaceForeground.crHovered = crSurfaceContrast;
		palette.crSurfaceForeground.crActive = crSurfaceContrast;
		palette.crSurfaceForeground.crDisable = crDisabled;
		if (style.nBorderNeighborOffset == 0)
		{
			palette.crBorder.crBase = ProjColor(PROJ_L_BORDER);
			palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
			palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
			palette.crBorder.crDisable = crDisabled;
			return palette;
		}
		palette.crBorder.crBase = (pcrHander != NULL) ? pcrHander->SampleBorder((DWORD)nFaceLayer, (DWORD)nBorderNearLayer) : 0;
		palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorder.crDisable = ProjColor(nFaceLayer);
		if (style.IsInnerEdge != FALSE)
		{
			palette.crBorderInner.crBase = (pcrHander != NULL) ? pcrHander->SampleBorder((DWORD)nFaceLayer, (DWORD)nSurfaceLayer) : 0;
		}
		palette.crBorderInner.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorderInner.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorderInner.crDisable = ProjColor(nFaceLayer);
		return palette;
	}

	// 函数: TitleBar
	// 描述: 标题控件: 绘制标题文本与右上关闭钮
	VOID TitleBar(CtrlInfo* info);
	// 函数: InputReal
	// 描述: 实数输入框
	VOID InputReal(CtrlInfo* info);
	// 函数: InputNumeric
	// 描述: 整数数字输入框(仅限整数; CTRL_TYPE_INPUT_NUMERIC 渲染)
	VOID InputNumeric(CtrlInfo* info);
	// 函数: InputAction
	// 描述: 输入动作控件: 无边框单行输入 + 右侧按钮(绘制经部件 Widgets::Base::InputAction)
	VOID InputAction(CtrlInfo* info);
	// 函数: Slider
	// 描述: 单手柄滑动条(CTRL_TYPE_SLIDER): 一个值 + 一个值域
	VOID Slider(CtrlInfo* info);
	// 函数: Progress
	// 描述: 进度状态显示
	VOID Progress(CtrlInfo* info);
	// 函数: RenderSurface
	// 描述: 渲染面控件(CTRL_TYPE_RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	VOID RenderSurface(CtrlInfo* info);
	// 函数: CheckBox
	// 描述: 复选框控件(CTRL_TYPE_CHECKBOX): 方框 / 勾 / 文字由 ImGui 原生复选框绘制
	VOID CheckBox(CtrlInfo* info);
	// 函数: Combo
	// 描述: 下拉框
	VOID Combo(CtrlInfo* info);
	// 函数: ComboAction
	// 描述: 下拉动作控件: 无边框下拉框 + 右侧按钮(绘制经部件 Widgets::Base::ComboAction)
	VOID ComboAction(CtrlInfo* info);
	// 函数: TimeBar
	// 描述: 可编辑时间条控件(CTRL_TYPE_TIMEBAR): 提示文本 + 时/分/秒/毫秒四段输入, 提交时派发编辑提交码
	VOID TimeBar(CtrlInfo* info);
	// 函数: SliderRange
	// 描述: 双手柄范围滑块控件(SLIDER_RANGE): 左/右手柄分别派发专属段与编辑变更码
	VOID SliderRange(CtrlInfo* info);
	// 函数: ListTimeSpan
	// 描述: 时间片列表控件(LIST_TIMESPAN): 逐行显示起止时码, 行内删除钮与行选中经操作码回传
	VOID ListTimeSpan(CtrlInfo* info);

	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口: 外部一次性送入(单参数), 控件自行拆解任务 ——
	VOID ListInput(CtrlInfo* info);
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口: 同构 —— 外部一次性送入 + 控件自行拆解, 行块同为 INPUT_ACTION 块(项目行只读);
	VOID ListCombo(CtrlInfo* info);

	using pfnGenericCtrlDraw = VOID(*)(CtrlInfo* info);

	// 函数: Button
	// 描述: 按钮: 绘制经同名部件 Widgets::Base::Button(三态面 + 面内文字), 点击时调用该控件的事件处理器
	VOID Button(CtrlInfo* info)
	{
		LPCSTR lpText = (LPCSTR)info->bufferInfo.buffer;
		const int64_t qwPart = Widgets::Base::Button(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), lpText);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (wCommon != OPER_COMMON_NONE && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Text
	// 描述: 静态文本: 文本图元经同名部件 Widgets::Base::Text 绘制; 仅当绑定事件(pEventHandler != NULL)时
	VOID Text(CtrlInfo* info)
	{
		LPCSTR text = (LPCSTR)info->bufferInfo.buffer;
		Widgets::Base::Text(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), text);
		if (info->pEventHandler != NULL)
		{
			const uint16_t wCommon = CtrlClickCode(info, u8"##TEXT_ITEM");
			if (wCommon != OPER_COMMON_NONE)
			{
				info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: Rect
	// 描述: RECT 控件: 圆角底面 + 圆角描边(色值全取投影), 无文本/无子内容 —— 绘制矩形 = 控件声明矩形;
	VOID Rect(CtrlInfo* info)
	{
		ImVec2 o{};
		CONST INT32 nLayer = CtrlLayer(info);
		o = ImGui::GetCursorScreenPos();
		DrawRectFace(ImGui::GetWindowDrawList(), o, toImvec2(info->size),
			ProjColor(nLayer), ProjColor(PROJ_L_BORDER));
		if (info->pEventHandler != NULL)
		{
			const uint16_t wCommon = CtrlClickCode(info, u8"##RECT_ITEM");
			if (wCommon != OPER_COMMON_NONE)
			{
				info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: InputSingle
	// 描述: 单行输入控件(INPUT_SINGLE): 绘制经部件 Widgets::Base::InputText(底与边框由 ImGui 原生绘制,
	VOID InputSingle(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		const int64_t qwPart = Widgets::Base::InputText(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), lpText, nTextCap, NULL);

		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputMulti
	// 描述: 多行文本输入控件(INPUT_MULTI): 长文本(提示词 / 负面提示词 / 参数文件内容)
	VOID InputMulti(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		const int64_t qwPart = Widgets::Base::InputTextMulti(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), lpText, nTextCap, NULL);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口 —— 外部(渲染层经 DrawCtrl)一次性送入(单参数);
	VOID ListInput(CtrlInfo* info)
	{
		int64_t qwOper = 0;
		uint64_t qwParam = 0;
		DWORD dwCount = 0;
		DWORD dwSpecialFlag = 0;
		BOOL bSpecialFirst = FALSE;
		FLOAT fGap = 0.0f;
		FLOAT fStep = 0.0f;
		FLOAT fSpecialH = 0.0f;
		size_t cbItem = 0;
		VOID* lpSpecial = NULL;
		VOID* lpItemBase = NULL;
		vec2 posCur{};
		ListBuffer* pList = NULL;
		Widgets::Pipe::ListPipe pipe{};
		Widgets::Pipe::PaintWay way{};
		if (info == NULL)
		{
			return;
		}
		pList = ListBufferOf(info);
		pipe.id = info->id;
		pipe.pos = info->position;
		pipe.size = info->size;
		pipe.label = info->label ? info->label : u8"##LISTPANEL";
		pipe.fRounding = fRounding;
		pipe.fRowH = sizeinfo.controlHeight;
		pipe.crShell = ProjColor(CtrlLayerOf(CtrlRel(info)));
		way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));
		if (pList == NULL)
		{
			Widgets::Pipe::ListRow(pipe, 0, vec2(0.0f, 0.0f), pipe.fRowH, NULL, NULL, way);
			return;
		}
		cbItem = (size_t)pList->cbItem;
		dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		pipe.dwCount = dwCount;
		lpSpecial = (VOID*)(pList + 1);
		lpItemBase = (VOID*)((BYTE*)(pList + 1) + cbItem);
		dwSpecialFlag = LIST_SPECIAL_LAST | ((DWORD)(sizeinfo.controlHeight) << 16);
		fSpecialH = (FLOAT)((dwSpecialFlag >> 16) & 0xFFFF);
		bSpecialFirst = ((dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST) ? TRUE : FALSE;
		fGap = sizeinfo.space;
		fStep = pipe.fRowH + fGap;
		posCur = vec2(sizeinfo.space, sizeinfo.space);
		if (bSpecialFirst)
		{
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, 0, posCur, fSpecialH, lpSpecial,
				Widgets::Base::InputAction, way), 0, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		for (DWORD i = 0; i < dwCount; i++)
		{
			VOID* lpItem = (VOID*)((BYTE*)lpItemBase + cbItem * i);
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)i, posCur, pipe.fRowH, lpItem,
				Widgets::Base::InputAction, way), i, qwOper, qwParam);
			posCur.y += fStep;
		}
		if (bSpecialFirst == FALSE)
		{
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)dwCount, posCur, fSpecialH, lpSpecial,
				Widgets::Base::InputAction, way), dwCount, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		if (qwOper != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
		}
	}
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口 —— 同构: 外部一次性送入(单参数), 控件自行拆解任务;
	VOID ListCombo(CtrlInfo* info)
	{
		int64_t qwOper = 0;
		uint64_t qwParam = 0;
		DWORD dwCount = 0;
		DWORD dwSpecialFlag = 0;
		BOOL bSpecialFirst = FALSE;
		FLOAT fGap = 0.0f;
		FLOAT fStep = 0.0f;
		FLOAT fSpecialH = 0.0f;
		size_t cbItem = 0;
		VOID* lpSpecial = NULL;
		VOID* lpItemBase = NULL;
		vec2 posCur{};
		ListBuffer* pList = NULL;
		Widgets::Pipe::ListPipe pipe{};
		Widgets::Pipe::PaintWay way{};
		if (info == NULL)
		{
			return;
		}
		pList = ListBufferOf(info);
		pipe.id = info->id;
		pipe.pos = info->position;
		pipe.size = info->size;
		pipe.label = info->label ? info->label : u8"##LISTPANEL";
		pipe.fRounding = fRounding;
		pipe.fRowH = sizeinfo.controlHeight;
		pipe.crShell = ProjColor(CtrlLayerOf(CtrlRel(info)));
		way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));
		if (pList == NULL)
		{
			Widgets::Pipe::ListRow(pipe, 0, vec2(0.0f, 0.0f), pipe.fRowH, NULL, NULL, way);
			return;
		}
		cbItem = (size_t)pList->cbItem;
		dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		pipe.dwCount = dwCount;
		lpSpecial = (VOID*)(pList + 1);
		lpItemBase = (VOID*)((BYTE*)(pList + 1) + cbItem);
		dwSpecialFlag = LIST_SPECIAL_LAST | ((DWORD)(sizeinfo.controlHeight) << 16);
		fSpecialH = (FLOAT)((dwSpecialFlag >> 16) & 0xFFFF);
		bSpecialFirst = ((dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST) ? TRUE : FALSE;
		fGap = sizeinfo.space;
		fStep = pipe.fRowH + fGap;
		posCur = vec2(sizeinfo.space, sizeinfo.space);
		if (bSpecialFirst)
		{
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, 0, posCur, fSpecialH, lpSpecial,
				Widgets::Base::ComboAction, way), 0, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		for (DWORD i = 0; i < dwCount; i++)
		{
			VOID* lpItem = (VOID*)((BYTE*)lpItemBase + cbItem * i);
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)i, posCur, pipe.fRowH, lpItem,
				Widgets::Base::InputActionRO, way), i, qwOper, qwParam);
			posCur.y += fStep;
		}
		if (bSpecialFirst == FALSE)
		{
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)dwCount, posCur, fSpecialH, lpSpecial,
				Widgets::Base::ComboAction, way), dwCount, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		if (qwOper != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
		}
	}

	// 函数: ListRowDropCalc
	// 描述: 列表项拖拽落点 → 列表项部件返回约定的纯算式(与 ListRowDropOper 同一真源; 常量表达式,
	static constexpr int64_t ListRowDropCalc(DWORD dwSrc, DWORD dwRow, BOOL IsAfter)
	{
		CONST DWORD dwDest = (IsAfter != FALSE) ? (dwRow + 1) : dwRow;
		return (int64_t)((uint64_t)OPER_LIST_ROW_MOVED |
			((uint64_t)(dwDest & 0xFFFF) << 32) |
			((uint64_t)(dwSrc & 0xFFFF) << 48));
	}
	// 函数: ListRowDropOper
	// 描述: 列表项拖拽落点 → 列表项部件返回约定(纯换算, 不触碰 ImGui; 绘制侧唯一真源)
	int64_t ListRowDropOper(DWORD dwSrc, DWORD dwRow, BOOL IsAfter)
	{
		return ListRowDropCalc(dwSrc, dwRow, IsAfter);
	}
	static_assert((uint16_t)((uint64_t)ListRowDropCalc(5, 2, FALSE) & 0xFFFF) == OPER_LIST_ROW_MOVED, "列表重排: 专属段码错");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, FALSE) >> 32) & 0xFFFF) == 2, "列表重排: 上半落点应为该行之前");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 32) & 0xFFFF) == 3, "列表重排: 下半落点应为该行之后");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, FALSE) >> 48) & 0xFFFF) == 5, "列表重排: 源行号应落 bit[63:48]");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(99, 2, TRUE) >> 32) & 0xFFFF) ==
		(uint32_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 32) & 0xFFFF), "列表重排: 落点不得随源行号变化");
	static_assert((uint16_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 16) & 0xFFFF) == OPER_COMMON_NONE,
		"列表重排: 公共位图必须为 0(无公共码)");

	struct GenericCtrlDrawEntry
	{
		CtrlType type;
		pfnGenericCtrlDraw pfnDraw;
	};

	static const GenericCtrlDrawEntry GenericCtrlDraws[] = {
		{ CTRL_TYPE_BUTTON,			Button },
		{ CTRL_TYPE_TEXT,			Text },
		{ CTRL_TYPE_INPUT_SINGLE,	InputSingle },
		{ CTRL_TYPE_INPUT_MULTI,	InputMulti },
		{ CTRL_TYPE_INPUT_REAL,		InputReal },
		{ CTRL_TYPE_INPUT_NUMERIC,	InputNumeric },
		{ CTRL_TYPE_INPUT_ACTION,	InputAction },
		{ CTRL_TYPE_SLIDER,			Slider },
		{ CTRL_TYPE_PROG,			Progress },
		{ CTRL_TYPE_RENDER,			RenderSurface },
		{ CTRL_TYPE_CHECKBOX,		CheckBox },
		{ CTRL_TYPE_COMBO,			Combo },
		{ CTRL_TYPE_COMBO_ACTION,	ComboAction },
		{ CTRL_TYPE_TIMEBAR,		TimeBar },
		{ CTRL_TYPE_SLIDER_RANGE,	SliderRange },
		{ CTRL_TYPE_LIST_TIMESPAN,	ListTimeSpan },
		{ CTRL_TYPE_RECT,			Rect },
		{ CTRL_TYPE_TITLE,			TitleBar },
		{ CTRL_TYPE_LIST_INPUT,		ListInput },
		{ CTRL_TYPE_LIST_COMBO,		ListCombo },
	};
	constexpr size_t GENERIC_CTRL_DRAW_NUM = sizeof(GenericCtrlDraws) / sizeof(GenericCtrlDraws[0]);

	// 函数: GenericCtrlDrawOf
	// 描述: 按类型查控件绘制实现(查上方分派表)
	static Controls::pfnGenericCtrlDraw GenericCtrlDrawOf(CtrlType type)
	{
		for (size_t i = 0; i < GENERIC_CTRL_DRAW_NUM; i++)
		{
			if (GenericCtrlDraws[i].type == type)
			{
				return GenericCtrlDraws[i].pfnDraw;
			}
		}
		return NULL;
	}

	// 函数: DrawCtrl
	// 描述: 按控件类型分派绘制实现(逐控件绘制入口): 换算绘制坐标 + 定位控件矩形, 再按类型调绘制实现
	BOOL DrawCtrl(CtrlInfo* ctrl)
	{
		const BOOL bUnknownRect = (ctrl->type == CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
		const BOOL bUnusedSlot = (ctrl->type == CTRL_TYPE_INVALID) ? TRUE : FALSE;
		const pfnGenericCtrlDraw pfnDraw = bUnknownRect ? NULL : GenericCtrlDrawOf(ctrl->type);
		const BOOL bShell = (ctrl->type == CTRL_TYPE_WND || ctrl->type == CTRL_TYPE_TITLE ||
			ctrl->type == CTRL_TYPE_CNTR) ? TRUE : FALSE;
		const vec2 posEffective = ctrl->position;
		if (bShell == FALSE)
		{
			ctrl->position = posEffective - orgContent;
		}
		ImGui::SetCursorPos(toImvec2(ctrl->position));
		if (bUnknownRect)
		{
			DrawUnknownCtrlRect(ctrl);
		}
		else if (bUnusedSlot)
		{
			ImGui::Dummy(toImvec2(ctrl->size));
		}
		else if (pfnDraw != NULL)
		{
			pfnDraw(ctrl);
		}
		else
		{
			ImGui::Dummy(toImvec2(ctrl->size));
		}
		if (bShell == FALSE)
		{
			ctrl->position = posEffective;
		}
		return (bUnknownRect || pfnDraw != NULL) ? TRUE : FALSE;
	}

	// 函数: UpdateContext
	// 描述: 更新绘制上下文(每帧渲染布局之前调用一次)
	BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj, CONST vec2* pWndPos, CONST vec2& posContent)
	{
		Widgets::UpdateContext(refsize, pProj);
		sizeinfo = refsize.GetSizeInfo<FLOAT>();
		fRounding = sizeinfo.padding;
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(0.0f, 0.0f);
		style.FramePadding = ImVec2(sizeinfo.padding, sizeinfo.padding);
		style.FrameRounding = fRounding;
		pcrHander = pProj;
		pWndPosSlot = pWndPos;
		orgContent = posContent;
		return TRUE;
	}

	// 函数: HasActiveInput
	// 描述: 本帧是否存在"处于编辑中(ImGui 项 active)的输入控件"
	BOOL HasActiveInput()
	{
		return Widgets::HasActiveInput();
	}

	static CONST CtrlApi CtrlApiTable = {
		UpdateContext,
		DrawCtrl,
		HasActiveInput,
	};

	// 函数: GetCtrlApi
	// 描述: 取控件库的绘制接口表(进程级静态表; 重复调用幂等)
	CONST CtrlApi* GetCtrlApi()
	{
		return &CtrlApiTable;
	}

}

namespace Controls
{

	// 函数: TitleBar
	// 描述: 标题控件(CTRL_TYPE_TITLE): 绘制标题文本与右上关闭钮, 并回传标题状态
	VOID TitleBar(CtrlInfo* info)
	{
		static BOOL bDrag = FALSE;
		static HWND hDragWnd = NULL;
		static vec2 ptGrab(0.0f, 0.0f);

		// 计算控件尺寸
		ImVec2 size = toImvec2(info->size);
		ImVec2 pos = toImvec2(info->position);
		CONST Widgets::Base::PaintWay wayTitle = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info) - 1, PALETTE_FLAT));
		CONST Widgets::Base::PaintWay wayExit = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT));
		FLOAT exitwidth = 1.5f;
		FLOAT padding = sizeinfo.padding;
		uint16_t wCommon = OPER_COMMON_NONE;
		uint16_t wOwn = 0;

		CHAR* lpText = CtrlTextBuffer(info);
		LPCSTR label = (lpText != NULL) ? lpText : "";

		// 获取真实坐标
		ImGui::SetCursorPos(pos);
		ImVec2 position = ImGui::GetCursorScreenPos();

		// 获取绘制列表
		ImDrawList* drawlist = ImGui::GetWindowDrawList();

		drawlist->AddRectFilled(position + ImVec2(1, 1), position + size - ImVec2(1, 0), wayTitle.palette.crBackground.crBase, sizeinfo.space + 1, ImDrawFlags_RoundCornersTop);
		// drawlist->AddRectFilled(position + ImVec2(1,1), position + size, 0xFF0000FF, sizeinfo.space + 1, ImDrawFlags_RoundCornersTop);

		// 此时会破坏原边框，所以补画
		//drawlist->AddRect(position, position + size, 0xFF00FF00, sizeinfo.space, 1.0, ImDrawFlags_RoundCornersTop);
		//drawlist->AddRect(position, position + size, 0xFFFF0000, sizeinfo.space + 2, 1.0, ImDrawFlags_RoundCornersTop);

		// 绘制控件

		ImGui::InvisibleButton(label, size - ImVec2{ size.y * exitwidth, 0 });
		if (ImGui::IsItemActive())    wCommon |= OPER_COMMON_HOLD;
		if (ImGui::IsItemActivated()) wCommon |= OPER_COMMON_ACTIVE;
		if (ImGui::IsItemDeactivated()) wCommon |= OPER_COMMON_DEACTIVATED;
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			wCommon |= OPER_COMMON_DOUBLECLICK;
		}
		Widgets::Base::Text(info->id, ImVec2(pos.x + sizeinfo.space, pos.y),
			ImVec2(size.x - sizeinfo.space, size.y), wayTitle, label);
		if (Widgets::Base::ExitSign(info->id, position + ImVec2{ size.x - size.y * exitwidth, 0 }, { size.y * exitwidth, size.y },
			wayExit, u8"##TITLEBAR_EXIT", padding, 1.0f))
		{
			wOwn = OPER_STRUCT_TITLE_CLOSE;
		}
		uint64_t param = 0;
		CONST HWND hWnd = ImGuiHost::GetCurrentWnd();
		if ((wCommon & OPER_COMMON_HOLD) != 0)
		{
			POINT cur{};
			if (hWnd != NULL && GetCursorPos(&cur) != FALSE)
			{
				if (bDrag == FALSE || hDragWnd != hWnd)
				{
					vec2 posWnd;
					BOOL bGot = FALSE;
					if (GRAB_BASE_BY_WND_POS != FALSE && pWndPosSlot != NULL)
					{
						posWnd = *pWndPosSlot;
						bGot = TRUE;
					}
					else
					{
						bGot = ImGuiHost::GetWndPos(&posWnd);
					}
					if (bGot != FALSE)
					{
						ptGrab = vec2((FLOAT)cur.x - posWnd.x, (FLOAT)cur.y - posWnd.y);
						hDragWnd = hWnd;
						bDrag = TRUE;
					}
				}
				if (bDrag != FALSE && hDragWnd == hWnd)
				{
					param = PackWndPos(vec2((FLOAT)cur.x - ptGrab.x, (FLOAT)cur.y - ptGrab.y));
				}
			}
		}
		else if (bDrag != FALSE && hDragWnd == hWnd)
		{
			bDrag = FALSE;
			hDragWnd = NULL;
		}
		uint64_t oper = OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0);
		if (info->pEventHandler && oper != OPER_NONE)
		{
			info->pEventHandler(info, oper, param, info->bufferInfo.buffer);
		}
	}
	// 函数: InputReal
	// 描述: 实数输入控件(INPUT_REAL): 缓冲为 DOUBLE; 绘制经部件 Widgets::Base::InputScalar
	VOID InputReal(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		BufferInfo& bufInfo = info->bufferInfo;
		if (bufInfo.buffer == NULL || bufInfo.size < sizeof(double))
		{
			Widgets::Base::InputText(info->id, toImvec2(info->position), toImvec2(info->size),
				Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), ERRBUFFER, 0, NULL);
			return;
		}
		const int64_t qwPart = Widgets::Base::InputScalar(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			(VOID*)bufInfo.buffer, ImGuiDataType_Double, "%05.2f", NULL);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputNumeric
	// 描述: 与 InputReal 同构: 缓冲为单个 INT; 绘制经部件 Widgets::Base::InputScalar
	VOID InputNumeric(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		BufferInfo& bufInfo = info->bufferInfo;
		if (bufInfo.buffer == NULL || bufInfo.size < sizeof(INT))
		{
			Widgets::Base::InputText(info->id, toImvec2(info->position), toImvec2(info->size),
				Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), ERRBUFFER, 0, NULL);
			return;
		}
		const int64_t qwPart = Widgets::Base::InputScalar(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			(VOID*)bufInfo.buffer, ImGuiDataType_S32, "%d", NULL);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputAction
	// 描述: 输入动作控件(CTRL_TYPE_INPUT_ACTION): 按控件缓冲取输入文本与按钮文案, 调 Widgets::Base::InputAction
	VOID InputAction(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		LPCSTR lpBtn = CTRL_DEFAULT_ACTION_BTN;
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= CTRL_INPUT_ACTION_BUF)
		{
			CHAR* lpBufBtn = CtrlInputActionBtnText((CtrlInputActionData*)info->bufferInfo.buffer);
			if (lpBufBtn != NULL && lpBufBtn[0] != '\0')
			{
				lpBtn = lpBufBtn;
			}
		}
		Widgets::Base::Palette palette{};
		CONST DWORD dwCtlLayer = CtrlLayer(info);
		CONST INT32 nBtnLayer = CtrlLayerOf(info->nLayer + 1);
		palette.crBackground.crBase = ProjColor(dwCtlLayer);
		palette.crBackground.crHovered = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crBackground.crActive = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crBackground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crForeground.crBase = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_CONTRAST);
		palette.crForeground.crHovered = palette.crForeground.crBase;
		palette.crForeground.crActive = palette.crForeground.crBase;
		palette.crForeground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crSurface.crBase = ProjColor(nBtnLayer);
		palette.crSurface.crHovered = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crSurfaceForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);
		palette.crSurfaceForeground.crHovered = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crActive = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crBorder.crBase = ProjColor(PROJ_L_BORDER);
		palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorder.crDisable = ProjColor(dwCtlLayer);
		palette.crBorderInner = palette.crBorder;
		const int64_t qwPart = Widgets::Base::InputAction(info->id, toImvec2(info->position), toImvec2(info->size), Widgets::Base::MakePaletteWay(palette), sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2, lpText, nTextCap, NULL, lpBtn);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Slider
	// 描述: 单手柄滑动条(CTRL_TYPE_SLIDER): 一个值 + 一个值域, 值变更回传专属段 OPER_EDIT_CHANGED
	VOID Slider(CtrlInfo* info)
	{
		BufferInfo buf = info->bufferInfo;
		if (buf.buffer == NULL || buf.size < sizeof(CtrlSliderData))
		{
			return;
		}
		CtrlSliderData& slider = *(CtrlSliderData*)buf.buffer;
		const int64_t qwPart = Widgets::Base::SingleSlider(
			info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN_RAISED)),
			slider.vMin, slider.lMin, slider.lMax);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Progress
	// 描述: 进度状态显示控件(CTRL_TYPE_PROG): 可见期间每帧回传专属段 OPER_PROG_UPDATED
	VOID Progress(CtrlInfo* info)
	{
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlProgData))
		{
			return;
		}
		CtrlProgData& prog = *(CtrlProgData*)info->bufferInfo.buffer;
		if (prog.IsVisible)
		{
			if (info->pEventHandler)
			{
				info->pEventHandler(info, OperCode::MakeCodeOwn(OPER_PROG_UPDATED), 0, info->bufferInfo.buffer);
			}
			Widgets::Base::ProgressStatic(info->id, toImvec2(info->position), toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), prog.current, prog.expect, sizeinfo.padding);
		}
	}
	// 函数: RenderSurface
	// 描述: 渲染面控件(CTRL_TYPE_RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	VOID RenderSurface(CtrlInfo* info)
	{
		CtrlRenderData* pData = NULL;
		BOOL bDrawn = FALSE;
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlRenderData))
		{
			pData = (CtrlRenderData*)info->bufferInfo.buffer;
		}
		bDrawn = Widgets::Base::RenderSurface(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)),
			(pData != NULL) ? pData->pTexture : NULL,
			(pData != NULL) ? pData->texWidth : 0,
			(pData != NULL) ? pData->texHeight : 0);
		if (bDrawn == FALSE)
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
		}
	}
	// 函数: CheckBox
	// 描述: 复选框控件(CTRL_TYPE_CHECKBOX): 方框 / 勾 / 文字一律由 ImGui 原生复选框绘制
	VOID CheckBox(CtrlInfo* info)
	{
		CtrlCheckData* pData = NULL;
		LPCSTR lpLabel = "";
		BOOL IsChecked = FALSE;
		int64_t qwPart = 0;
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlCheckData))
		{
			pData = (CtrlCheckData*)info->bufferInfo.buffer;
			IsChecked = pData->IsChecked;
			lpLabel = pData->szLabel;
		}
		qwPart = Widgets::Base::CheckBox(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED_SUNKEN)), IsChecked, lpLabel);
		if (qwPart != 0)
		{
			if (pData != NULL)
			{
				pData->IsChecked = IsChecked;
			}
			if (info->pEventHandler != NULL)
			{
				info->pEventHandler(info, OperCode::MakeCodeOwn(OPER_EDIT_CHANGED), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: Combo
	// 描述: 下拉框控件(CTRL_TYPE_COMBO): 绘制经同名部件, 值/选项变更时回传专属段 OPER_EDIT_CHANGED
	VOID Combo(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlComboData))
		{
			return;
		}
		CtrlComboData& combo = *(CtrlComboData*)info->bufferInfo.buffer;
		const int64_t qwPart = Widgets::Base::Combo(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), combo.items, combo.count, combo.index);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}

	// 函数: TimeBar
	// 描述: 可编辑时间条控件(CTRL_TYPE_TIMEBAR): 取控件缓冲的提示文本与毫秒值, 绘制时码四段输入;
	//       提交时派发专属段编辑提交码(缓冲已由部件按上限钳制并写回)
	VOID TimeBar(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlTimeBarData))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		CtrlTimeBarData* pData = (CtrlTimeBarData*)info->bufferInfo.buffer;
		const int64_t qwPart = Widgets::Base::TimeBar(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			pData->szHint, pData->valueMs, pData->limitMs);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			CONST uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
			CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
			info->pEventHandler(info,
				OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0),
				0, info->bufferInfo.buffer);
		}
	}

	// 函数: FormatTimeSpanText
	// 描述: 把起止毫秒格式化为时码文本(时:分:秒.毫秒 - 时:分:秒.毫秒)
	static VOID FormatTimeSpanText(int64_t nStartMs, int64_t nEndMs, LPSTR lpText, size_t cbText)
	{
		if (lpText == NULL || cbText == 0)
		{
			return;
		}
		if (nStartMs < 0) { nStartMs = 0; }
		if (nEndMs < 0) { nEndMs = 0; }
		CONST int64_t h1 = nStartMs / 3600000;
		CONST int64_t m1 = (nStartMs / 60000) % 60;
		CONST int64_t s1 = (nStartMs / 1000) % 60;
		CONST int64_t ms1 = nStartMs % 1000;
		CONST int64_t h2 = nEndMs / 3600000;
		CONST int64_t m2 = (nEndMs / 60000) % 60;
		CONST int64_t s2 = (nEndMs / 1000) % 60;
		CONST int64_t ms2 = nEndMs % 1000;
		sprintf_s(lpText, cbText, u8"%02lld:%02lld:%02lld.%03lld - %02lld:%02lld:%02lld.%03lld",
			(long long)h1, (long long)m1, (long long)s1, (long long)ms1,
			(long long)h2, (long long)m2, (long long)s2, (long long)ms2);
	}

	// 函数: SliderRange
	// 描述: 双手柄范围滑块控件(SLIDER_RANGE): 左/右手柄分别派发专属段与编辑变更码
	VOID SliderRange(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlSliderRangeData))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		CtrlSliderRangeData* pData = (CtrlSliderRangeData*)info->bufferInfo.buffer;
		int64_t qwPart = Widgets::Base::RangeSlider(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)),
			pData->vMin, pData->vMax, pData->lMin, pData->lMax);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			CONST uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
			CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
			info->pEventHandler(info,
				OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0),
				0, info->bufferInfo.buffer);
		}
	}

	// 函数: TimeSpanRow
	// 描述: 时间片列表项部件: 行点击区 + 时码文本 + 行尾删除钮(返回列表项部件操作约定)
	static int64_t TimeSpanRow(int nRowId, vec2 pos, vec2 size, CONST Widgets::Base::PaintWay& way, VOID* lpItem)
	{
		using namespace Widgets::Base;

		int64_t qwOper = 0;
		TimeSpanItem* pItem = (TimeSpanItem*)lpItem;
		if (pItem)
		{
			// 准备文本
			CHAR szText[96]{}, szDelete[4] = { '-' };
			FormatTimeSpanText(pItem->startMs, pItem->endMs, szText, sizeof(szText));
			qwOper = TextAction(nRowId, toImvec2(pos), toImvec2(size), way, size.y, szText, szDelete);
		}

		return qwOper;
	}

	// 函数: ListTimeSpan
	// 描述: 时间片列表控件(LIST_TIMESPAN): 逐行显示起止时码, 行内删除钮与行选中经操作码回传
	VOID ListTimeSpan(CtrlInfo* info)
	{
		int64_t qwOper = 0;
		uint64_t qwParam = 0;
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(ListBuffer))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		ListBuffer* pList = (ListBuffer*)info->bufferInfo.buffer;
		DWORD dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		size_t cbItem = (pList->cbItem >= sizeof(TimeSpanItem)) ? (size_t)pList->cbItem : sizeof(TimeSpanItem);
		CONST FLOAT fInner = sizeinfo.padding;
		CONST FLOAT fRowH = sizeinfo.controlHeight;
		CONST FLOAT fStep = fRowH;
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fRounding);
		ImGui::SetCursorPos(toImvec2(info->position));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ProjColor(CtrlLayerOf(CtrlRel(info))));
		ImGui::BeginChild(info->label ? info->label : u8"##TIMESPANLIST", toImvec2(info->size), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleColor();
		DWORD dwVisible = (DWORD)((info->size.y - fInner) / fStep);
		if (dwVisible < 1) { dwVisible = 1; }
		if (dwCount > 0 && pList->dwTop >= dwCount)
		{
			pList->dwTop = dwCount - 1;
		}
		if (dwCount > dwVisible && pList->dwTop + dwVisible > dwCount)
		{
			pList->dwTop = dwCount - dwVisible;
		}
		if (pList->dwTop > dwCount)
		{
			pList->dwTop = 0;
		}
		if (ImGui::IsWindowHovered())
		{
			FLOAT fWheel = ImGui::GetIO().MouseWheel;
			if (fWheel < 0.0f && pList->dwTop + dwVisible < dwCount)
			{
				pList->dwTop++;
			}
			else if (fWheel > 0.0f && pList->dwTop > 0)
			{
				pList->dwTop--;
			}
		}
		CONST Widgets::Base::PaintWay way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));
		FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;

		if (fW < 1.0f) { fW = 1.0f; }
		for (DWORD i = 0; i < dwVisible && (pList->dwTop + i) < dwCount; i++)
		{
			DWORD dwRow = pList->dwTop + i;
			VOID* lpItem = (VOID*)((BYTE*)(pList + 1) + cbItem * (size_t)dwRow);
			CONST vec2 posRow(fInner, fInner + (fInner + fStep) * (FLOAT)i);
			if (dwRow == pList->dwSelected)
			{
				ImGui::SetCursorPos(toImvec2(posRow));
				CONST ImVec2 o = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddRectFilled(o, ImVec2(o.x + fW, o.y + fRowH),
					ImGui::GetColorU32(ImGuiCol_Header), fRounding * 0.5f);
			}
			int64_t qwRow = TimeSpanRow((int)dwRow, posRow, vec2(fW, fRowH), way, lpItem);
			if (qwRow != 0)
			{
				ListCollectRowOper(qwRow, dwRow, qwOper, qwParam);
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		if (qwOper != 0)
		{
			CONST uint32_t dwRowOper = (uint32_t)(OperCode::GetSegment((uint64_t)qwOper,
				OperCode::OPER_SEG_PAYLOAD) & 0xFFFF);
			if (dwRowOper == OPER_LIST_ROW_ACTIVATED)
			{
				pList->dwSelected = (DWORD)qwParam;
				qwOper = (int64_t)OperCode::MakeCode(
					OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_OPCODE),
					0, OPER_LIST_ROW_SELECTION_CHANGED);
				qwParam = 0;
			}
			if (info->pEventHandler != NULL)
			{
				info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
			}
		}
	}

	// 函数: ComboAction
	// 描述: 下拉动作控件(CTRL_TYPE_COMBO_ACTION): 按控件缓冲(CtrlComboData)取选项表与选中索引,
	VOID ComboAction(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		CtrlComboData* pCombo = NULL;
		LPCSTR* lpItems = NULL;
		INT nCount = 0;
		INT nIndex = 0;
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlComboData))
		{
			pCombo = (CtrlComboData*)info->bufferInfo.buffer;
			lpItems = pCombo->items;
			nCount = pCombo->count;
			nIndex = pCombo->index;
		}
		if (lpItems == NULL || nCount <= 0)
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		const DWORD crDisabled = ProjColor(PROJ_L_DISABLED);
		CONST INT32 nBtnLayer = CtrlLayer(info);
		Widgets::Base::Palette palette{};
		palette.crBackground = Widgets::Base::StateColors{};
		palette.crForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);
		palette.crForeground.crHovered = palette.crForeground.crBase;
		palette.crForeground.crActive = palette.crForeground.crBase;
		palette.crForeground.crDisable = crDisabled;
		palette.crSurface.crBase = ProjColor(nBtnLayer);
		palette.crSurface.crHovered = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = crDisabled;
		palette.crSurfaceForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);
		palette.crSurfaceForeground.crHovered = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crActive = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crDisable = crDisabled;
		palette.crBorder = Widgets::Base::StateColors{};
		palette.crBorderInner.crBase = ProjColor(PROJ_L_BORDER);
		palette.crBorderInner.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorderInner.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorderInner.crDisable = crDisabled;
		LPCSTR lpBtn = CTRL_DEFAULT_ACTION_BTN;
		const int64_t qwPart = Widgets::Base::ComboAction(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(palette),
			sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2,
			lpItems, nCount, nIndex, lpBtn);
		pCombo->index = nIndex;
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}

}

// 函数: DrawRectFace
// 描述: 画"矩形外观"(圆角底面 + 圆角描边)。供"通用矩形绘制"(CTRL_TYPE_RECT / 未知类型矩形占位)共用。
static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder)
{
	if (dl == NULL)
	{
		return;
	}
	ImVec2 e(o.x + size.x, o.y + size.y);
	dl->AddRectFilled(o, e, crBase, Controls::fRounding);
	dl->AddRect(o, e, crBorder, Controls::fRounding, 0, 1.0f);
}
// 函数: DrawUnknownCtrlRect
// 描述: 未知类型(文档类型串未命中 = CTRL_TYPE_UNKNOWN)的矩形占位绘制 —— 未知类型在被送入 Render 之前
static VOID DrawUnknownCtrlRect(CtrlInfo* info)
{
	if (info == NULL)
	{
		return;
	}
	const vec2& pos = info->position;
	const vec2& size = info->size;
	ImGui::SetCursorPos(toImvec2(pos));
	const ImVec2 o = ImGui::GetCursorScreenPos();
	DrawRectFace(ImGui::GetWindowDrawList(), o, toImvec2(size),
		Controls::CtrlLayer(info), Controls::ProjColor(Controls::PROJ_L_BORDER));
	ImGui::SetCursorPos(toImvec2(pos));
	ImGui::Dummy(toImvec2(size));
}
// 函数: CtrlClickCode
// 描述: 在图元绘制之后, 为"本身不回调的图元控件"(CTRL_TYPE_TEXT / CTRL_TYPE_RECT)补一枚覆盖
static uint16_t CtrlClickCode(CtrlInfo* info, LPCSTR lpFallback)
{
	uint16_t wCommon = OPER_COMMON_NONE;
	if (info == NULL || info->size.x <= 0.0f || info->size.y <= 0.0f)
	{
		return wCommon;
	}
	ImGui::SetCursorPos(toImvec2(info->position));
	ImGui::PushID(info->id);
	ImGui::InvisibleButton(info->label ? info->label : lpFallback, toImvec2(info->size));
	if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		wCommon |= OPER_COMMON_CLICKED;
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		wCommon |= OPER_COMMON_RIGHTCLICK;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		wCommon |= OPER_COMMON_DOUBLECLICK;
	}
	ImGui::PopID();
	return wCommon;
}
// 函数: ListBufferOf
// 描述: 取列表控件(LIST_INPUT/LIST_COMBO)的缓冲头
static ListBuffer* ListBufferOf(CtrlInfo* info)
{
	if (info == NULL || info->bufferInfo.buffer == NULL ||
		info->bufferInfo.size < sizeof(ListBuffer))
	{
		return NULL;
	}
	return (ListBuffer*)info->bufferInfo.buffer;
}

// 函数: ListCollectRowOper
// 描述: 把"本列表项部件返回约定"折算进控件的"oper + 回调 param"(ListInput/ListCombo/Command::List
static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam)
{
	if (qwRow == 0)
	{
		return;
	}
	CONST uint16_t wOwn = (uint16_t)((uint64_t)qwRow & 0xFFFF);
	CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwRow >> 16) & 0xFFFF);
	CONST uint32_t dwExtra1 = (uint32_t)(((uint64_t)qwRow >> 32) & 0xFFFF);
	CONST uint32_t dwExtra2 = (uint32_t)(((uint64_t)qwRow >> 48) & 0xFFFF);
	if (wOwn == OPER_LIST_ROW_MOVED)
	{
		qwOper = (int64_t)OperCode::MakeCode(OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_DATA),
			OPER_LIST_ROW_MOVED, dwExtra1);
		qwParam = (uint64_t)dwExtra2;
		return;
	}
	qwOper = (int64_t)OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_OPCODE), 0, (uint32_t)wOwn);
	qwParam = (uint64_t)dwRow;
}
