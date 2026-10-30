#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <windows.h>
#include "framework/render/imgui/control/Widgets.h"
#include "framework/render/imgui/control/Controls.h"
#include "framework/base/CtrlBuffer.h"
#include "framework/base/TextWidth.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <stdio.h>
#include <string.h>

namespace Controls
{
	namespace Widgets
	{

		static auto sizeinfo = AutoSize::SizeInfoFLOAT{};
		static FLOAT fRounding = 0.0f;
		static CONST ProjCR* pcrHander = NULL;
		static DWORD dwDisableIME = 0;

		constexpr INT32 PROJ_L_BORDER_HOVER = 11;
		constexpr INT32 PROJ_L_BORDER_ACTIVE = 13;

		constexpr FLOAT SLIDER_GRIP_RATIO = 0.28f;
		constexpr FLOAT SLIDER_TRACK_RATIO = 0.30f;

		// 函数: ProjColor
		// 描述: 列表拖动辅助取色的唯一入口(投影未注入时返回 0)
		static inline DWORD ProjColor(INT32 nLayer, ProjCR::CR_SLOT eSlot = ProjCR::PROJ_SLOT_SURFACE)
		{
			INT32 nLayerClamped = (nLayer < PROJ_LAYER_MIN) ? PROJ_LAYER_MIN :
				((nLayer > PROJ_LAYER_MAX) ? PROJ_LAYER_MAX : nLayer);
			return (pcrHander != NULL) ? pcrHander->Sample((DWORD)nLayerClamped, eSlot) : 0;
		}

		// 函数: UpdateContext
		// 描述: 更新部件层上下文(每帧一次; 复位编辑中计数, 并算当前控件圆角)
		BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj)
		{
			dwDisableIME = 0;
			sizeinfo = refsize.GetSizeInfo<FLOAT>();
			fRounding = sizeinfo.padding;
			pcrHander = pProj;
			return TRUE;
		}

		// 函数: HasActiveInput
		// 描述: 查询本帧是否存在编辑中的输入项(计数非 0 即 TRUE)
		BOOL HasActiveInput()
		{
			return (dwDisableIME != 0) ? TRUE : FALSE;
		}

		// 函数: toImvec2
		// 描述: 内置 vec2 到 ImGui 类型的转换模板(safe = false 走 reinterpret_cast)
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
		static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder);
		static BOOL ComboItems(LPCSTR* lpItems, INT nCount, INT& nIndex);
		constexpr LPCSTR GENUI_LIST_DRAG_TYPE = "GENUI_LIST_ROW";
		struct ListRowDragPayload
		{
			DWORD dwRow;
		};
		static FLOAT ListRowDragHandle(int id, vec2 pos, vec2 size, DWORD dwRow);
		static int64_t ListRowDropTarget(int id, vec2 pos, vec2 size, DWORD dwRow);
		static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam);

	}
	void CollectImmedState(ImmedState::State& state)
	{
		using namespace ImmedState;
		if (ImGui::IsItemHovered())
		{
			state.Set(IMMED_STATE_HOVERED);
		}
		if (ImGui::IsItemActive())
		{
			state.Set(IMMED_STATE_ACTIVE);
		}
		if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
		{
			state.Set(IMMED_STATE_DISABLED);
		}
		if (ImGui::IsItemDeactivated())
		{
			state.Set(IMMED_STATE_DEACTIVATED);
		}
		if (ImGui::IsItemFocused())
		{
			state.Set(IMMED_STATE_FOCUSED);
		}
		if (ImGui::IsItemEdited())
		{
			state.Set(IMMED_STATE_EDITED);
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			state.Set(IMMED_STATE_SUBMITTED);
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			state.Set(IMMED_STATE_DRAGGING);
		}
	}
}

namespace Controls
{
	namespace Widgets
	{

		namespace Base
		{
			// 函数: ExitSign
			// 描述: 关闭按钮部件: 两段交叉线符号 + 悬停底(标题栏用)
			BOOL ExitSign(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR label, FLOAT fPadding, FLOAT thickness)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				// 能力获取
				CONST Palette& palette = way.palette;
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				// 尺寸计算
				float fSize = (size.x < size.y ? size.x : size.y) - fPadding * 4;
				ImVec2 sign = { pos.x + CenterOffset(size.x, fSize), pos.y + CenterOffset(size.y, fSize) };
				ImVec2 end = { sign.x + fSize, sign.y + fSize };
				ImVec2 btn_end = { pos.x + size.x, pos.y + size.y };
				// 绘制与交互
				ImGui::PushID(id);
				ImGui::SetCursorScreenPos(pos);
				ImGui::InvisibleButton(label ? label : "##EXITBUTTON", size);
				BOOL IsHovered = ImGui::IsItemHovered();
				BOOL IsClicked = ImGui::IsItemClicked();
				ImU32 crFore = StateColorU32(palette.crForeground, WIDGET_STATE_BASE);
				if (IsHovered)
				{
					drawlist->AddRectFilled(pos + ImVec2(0, 1), btn_end - ImVec2(1, 0), palette.crBackground.crHovered, sizeinfo.space + 1, ImDrawFlags_RoundCornersTopRight);
				}
				drawlist->AddLine(ImVec2(sign.x, sign.y), ImVec2(end.x, end.y), crFore, thickness);
				drawlist->AddLine(ImVec2(sign.x, end.y), ImVec2(end.x, sign.y), crFore, thickness);
				ImGui::PopID();
				return IsClicked;
			}
			// 函数: InputText
			// 描述: 单行文本件: 底与边框经 ImGui 原生绘制(内边距按声明高对齐), 本件不再自绘外框
			int64_t InputText(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap, LPCSTR lpHint)
			{	
				// 初始化
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 参数计算
				const BOOL bEditable = nTextCap > 0;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				ImGui::PushID(id);
				ImGuiStyle& style = ImGui::GetStyle();
				FLOAT padT = CenterOffset(size.y, sizeinfo.fontSize);
				if (padT < 1.0f)
				{
					padT = 1.0f;
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::SetNextItemWidth(size.x);
				ImGui::InputTextWithHint("##FRAMEDITEM_TEXT", lpHint, lpText, nCap,
					bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_EDIT_SUBMIT;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: InputTextMulti
			// 描述: 多行文本件: 底与边框经 ImGui 原生绘制(项尺寸 = 声明矩形), 本件不再自绘外框
			int64_t InputTextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint)
			{
				UNREFERENCED_PARAMETER(lpHint);
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				const BOOL bEditable = (nTextCap > 0) ? TRUE : FALSE;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				ImGui::PushID(id);
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::InputTextMultiline("##FRAMEDITEM_TEXT_MULTI", lpText, nCap, size,
					bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_EDIT_SUBMIT;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: InputScalar
			// 描述: 数值件: 底与边框经 ImGui 原生绘制(内边距按声明高对齐), 本件不再自绘外框
			int64_t InputScalar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, VOID* pData, ImGuiDataType eType,
				LPCSTR lpFormat, LPCSTR lpHint)
			{
				UNREFERENCED_PARAMETER(lpHint);
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (pData == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				ImGuiStyle& style = ImGui::GetStyle();
				FLOAT padT = CenterOffset(size.y, sizeinfo.fontSize);
				if (padT < 1.0f)
				{
					padT = 1.0f;
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::SetNextItemWidth(size.x);
				const BOOL bChanged = ImGui::InputScalar("##FRAMEDITEM_SCALAR", eType, pData,
					NULL, NULL, lpFormat, ImGuiInputTextFlags_CharsDecimal);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_EDIT_SUBMIT;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: Combo
			// 描述: 下拉件: 原生底 + 通道补画 1px 边框, 展开列表项表与 ComboAction 同源
			int64_t Combo(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR* lpItems, INT nCount,
				INT& nIndex)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (lpItems == NULL || nCount <= 0 || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				if (nIndex < 0 || nIndex >= nCount)
				{
					nIndex = 0;
				}
				ImGui::PushID(id);
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				const FLOAT th = sizeinfo.fontSize;
				FLOAT padT = CenterOffset(size.y, th);
				if (padT < 1.0f)
				{
					padT = 1.0f;
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				drawlist->ChannelsSplit(2);
				drawlist->ChannelsSetCurrent(0);
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetNextItemWidth(size.x);
				BOOL bChanged = FALSE;
				const char* preview = lpItems[nIndex];
				if (ImGui::BeginCombo("##FRAMEDITEM_COMBO", preview))
				{
					bChanged = ComboItems(lpItems, nCount, nIndex);
					ImGui::EndCombo();
				}
				WidgetState state = WIDGET_STATE_BASE;
				state |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state |= ItemDisabled() ? WIDGET_STATE_DISABLED : WIDGET_STATE_BASE;
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_EDIT_CHANGED;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				drawlist->ChannelsSetCurrent(1);
				drawlist->AddRect(boot, end, StateColorU32(palette.crBorder, state),
					fRounding, ImDrawFlags_None, 1.0f);
				drawlist->ChannelsMerge();
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: TimeSpan
			// 描述: 时段显示件: 自绘圆角底 + 时码文本(无交互项, 恒返回 0)
			int64_t TimeSpan(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t nStartMs, int64_t nEndMs,
				int64_t nLimitMs)
			{
				const int64_t qwOper = 0;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				if (nStartMs < 0)
				{
					nStartMs = 0;
				}
				if (nEndMs < 0)
				{
					nEndMs = 0;
				}
				if (nLimitMs > 0)
				{
					if (nStartMs > nLimitMs)
					{
						nStartMs = nLimitMs;
					}
					if (nEndMs > nLimitMs)
					{
						nEndMs = nLimitMs;
					}
				}
				CHAR szSpan[64] = {};
				sprintf_s(szSpan, sizeof(szSpan), "%02d:%02d:%02d.%03d - %02d:%02d:%02d.%03d",
					(int)(nStartMs / 3600000 % 100), (int)(nStartMs / 60000 % 60),
					(int)(nStartMs / 1000 % 60), (int)(nStartMs % 1000),
					(int)(nEndMs / 3600000 % 100), (int)(nEndMs / 60000 % 60),
					(int)(nEndMs / 1000 % 60), (int)(nEndMs % 1000));
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::Dummy(size);
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				drawlist->AddRectFilled(boot, end, StateColorU32(palette.crBackground, WIDGET_STATE_BASE), fRounding);
				FLOAT fTextY = boot.y + CenterOffset(size.y, sizeinfo.fontSize);
				drawlist->AddText(ImVec2(boot.x + style.FramePadding.x, fTextY),
					StateColorU32(palette.crForeground, WIDGET_STATE_BASE), szSpan);
				ImGui::PopID();
				return qwOper;
			}
			// 函数: TimeBarSegment
			// 描述: 时间条的单个数字段: 透明底 + 定宽数字输入(变更或滚轮微调时写回段值并返回 TRUE)
			static BOOL TimeBarSegment(int id, ImVec2 pos, FLOAT fW, FLOAT fH, CONST Palette& palette,
				INT32* pValue, INT32 nMax, LPCSTR lpFormat)
			{
				BOOL bChanged = FALSE;
				if (pValue == NULL || fW <= 0.0f || fH <= 0.0f)
				{
					return bChanged;
				}
				FLOAT padT = CenterOffset(fH, sizeinfo.fontSize);
				if (padT < 1.0f)
				{
					padT = 1.0f;
				}
				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				ImGui::PushID(id);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(sizeinfo.padding, padT));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_FrameBg, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::SetNextItemWidth(fW);
				if (ImGui::InputScalar("##TIMEBARSEG", ImGuiDataType_S32, pValue, NULL, NULL, lpFormat,
					ImGuiInputTextFlags_CharsDecimal))
				{
					bChanged = TRUE;
				}
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;
				}
				// 滚轮微调: 上滚加一, 下滚减一; 越界时回绕到另一端(与旧项目时码输入一致)
				if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
				{
					*pValue += (ImGui::GetIO().MouseWheel > 0.0f) ? 1 : -1;
					*pValue = (*pValue < 0) ? nMax : ((*pValue > nMax) ? 0 : *pValue);
					bChanged = TRUE;
				}
				// 逐段上限: 时 99 / 分 59 / 秒 59 / 毫秒 999
				if (*pValue < 0)
				{
					*pValue = 0;
					bChanged = TRUE;
				}
				else if (*pValue > nMax)
				{
					*pValue = nMax;
					bChanged = TRUE;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar(2);
				ImGui::PopID();
				return bChanged;
			}
			// 函数: TimeBar
			// 描述: 可编辑时间条: 提示文本 + 圆角底 + 时/分/秒/毫秒四段数字输入; 任一段变更即按新值
			//       合成毫秒写回引用(按上限钳制), 并返回专属段编辑提交码(无变更返回 0)
			int64_t TimeBar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpHint,
				int64_t& rValueMs, int64_t nLimitMs)
			{
				int64_t qwOper = 0;
				BOOL bChanged = FALSE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 1. 当前值拆成时 / 分 / 秒 / 毫秒四段(上限 99:59:59.999)
				int64_t nValue = (rValueMs > 0) ? rValueMs : 0;
				if (nLimitMs > 0 && nValue > nLimitMs)
				{
					nValue = nLimitMs;
				}
				INT32 Parts[4] = { 0, 0, 0, 0 };
				{
					int64_t nTotalSeconds = nValue / 1000;
					Parts[0] = (INT32)(nTotalSeconds / 3600);
					Parts[1] = (INT32)((nTotalSeconds / 60) % 60);
					Parts[2] = (INT32)(nTotalSeconds % 60);
					Parts[3] = (INT32)(nValue % 1000);
					if (Parts[0] > 99)
					{
						Parts[0] = 99;
						Parts[1] = 59;
						Parts[2] = 59;
						Parts[3] = 999;
					}
				}
				// 2. 排版: 提示文本占左段, 圆角底条占右段(提示为空时底条占满控件矩形)
				CONST FLOAT fPad = sizeinfo.padding;
				CONST FLOAT fHalfFont = sizeinfo.fontSize * 0.5f;
				CONST FLOAT fBarNeed = fHalfFont * 9.0f + fPad * 11.0f;
				FLOAT fHintW = 0.0f;
				if (lpHint != NULL && lpHint[0] != '\0')
				{
					fHintW = ImGui::CalcTextSize(lpHint).x;
				}
				CONST FLOAT fHintGap = (fHintW > 0.0f) ? fPad : 0.0f;
				FLOAT fBarW = size.x - fHintW - fHintGap;
				if (fBarW > fBarNeed)
				{
					fBarW = fBarNeed;
				}
				if (fBarW < 1.0f)
				{
					fBarW = 1.0f;
				}
				CONST FLOAT fUnit = (fBarW < fBarNeed) ? (fBarW / fBarNeed) : 1.0f;
				CONST FLOAT fSegWidths[4] = {
					fHalfFont * 2.0f * fUnit + fPad * 2.0f,
					fHalfFont * 2.0f * fUnit + fPad * 2.0f,
					fHalfFont * 2.0f * fUnit + fPad * 2.0f,
					fHalfFont * 3.0f * fUnit + fPad * 2.0f
				};
				CONST FLOAT fGap = fPad * fUnit;
				static CONST LPCSTR Formats[4] = { "%02d", "%02d", "%02d", "%03d" };
				static CONST INT32 Limits[4] = { 99, 59, 59, 999 };
				static CONST LPCSTR Separators[3] = { ":", ":", "." };
				// 3. 提示文本 + 圆角底条
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				CONST ImVec2 boot = ImGui::GetCursorScreenPos();
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				if (fHintW > 0.0f)
				{
					drawlist->AddText(ImVec2(boot.x, boot.y + CenterOffset(size.y, sizeinfo.fontSize)),
						StateColorU32(palette.crForeground, WIDGET_STATE_BASE), lpHint);
				}
				CONST ImVec2 barBoot = ImVec2(boot.x + fHintW + fHintGap, boot.y);
				CONST ImVec2 barEnd = ImVec2(barBoot.x + fBarW, boot.y + size.y);
				CONST FLOAT fBarRounding = (fRounding < size.y * 0.5f) ? fRounding : size.y * 0.5f;
				drawlist->AddRectFilled(barBoot, barEnd, StateColorU32(palette.crBackground, WIDGET_STATE_BASE), fBarRounding);
				drawlist->AddRect(barBoot, barEnd, StateColorU32(palette.crBorder, WIDGET_STATE_BASE), fBarRounding, 0, 1.0f);
				// 4. 四段数字输入与段间分隔符
				FLOAT fX = barBoot.x;
				for (int i = 0; i < 4; i++)
				{
					if (TimeBarSegment(id * 8 + i, ImVec2(fX, pos.y), fSegWidths[i], size.y, palette,
						&Parts[i], Limits[i], Formats[i]))
					{
						bChanged = TRUE;
					}
					fX += fSegWidths[i];
					if (i < 3)
					{
						CONST ImVec2 sepSize = ImGui::CalcTextSize(Separators[i]);
						drawlist->AddText(ImVec2(fX + CenterOffset(fGap, sepSize.x),
							boot.y + CenterOffset(size.y, sizeinfo.fontSize)),
							StateColorU32(palette.crForeground, WIDGET_STATE_DISABLED), Separators[i]);
						fX += fGap;
					}
				}
				// 5. 合成毫秒: 任一段变更才写回并回传提交码(按上限钳制)
				if (bChanged)
				{
					int64_t nNew = ((int64_t)Parts[0] * 3600 + (int64_t)Parts[1] * 60 + (int64_t)Parts[2]) * 1000 +
						(int64_t)Parts[3];
					if (nNew < 0)
					{
						nNew = 0;
					}
					if (nLimitMs > 0 && nNew > nLimitMs)
					{
						nNew = nLimitMs;
					}
					rValueMs = nNew;
					qwOper = (int64_t)(uint64_t)OPER_EDIT_SUBMIT;
				}
				ImGui::PopID();
				return qwOper;
			}
			// 函数: InputAction
			// 描述: 输入动作部件: 左段单行输入 + 右段按钮(两段底与边框在项提交后补画)
			int64_t InputAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW, CHAR* lpText, size_t nTextCap, LPCSTR lpHint, LPCSTR lpBtn)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				const BOOL bEditable = (nTextCap > 0) ? TRUE : FALSE;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				const FLOAT fBtnMin = size.y;
				FLOAT fBtn = (fBtnW > fBtnMin) ? fBtnW : fBtnMin;
				if (fBtn > size.x - sizeinfo.fontSize)
				{
					fBtn = fBtnMin;
				}
				if (fBtn > size.x - 1.0f)
				{
					fBtn = size.x - 1.0f;
				}
				if (fBtn < 1.0f)
				{
					fBtn = 1.0f;
				}
				FLOAT fInW = size.x - fBtn;
				if (fInW < 1.0f)
				{
					fInW = 1.0f;
				}
				ImGui::PushID(id);
				ImDrawList* drawlist = ImGui::GetWindowDrawList();

				ImGuiStyle& style = ImGui::GetStyle();
				ImGui::SetCursorPos(pos);
				ImVec2 boot = ImGui::GetCursorScreenPos();
				ImVec2 mid = ImVec2{ boot.x + fInW, boot.y + size.y };
				ImVec2 end = ImVec2{ boot.x + size.x, boot.y + size.y };
				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				WidgetState state = WIDGET_STATE_BASE;

				drawlist->ChannelsSplit(3);
				drawlist->ChannelsSetCurrent(1);

				ImGui::PushStyleColor(ImGuiCol_FrameBg, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(ImVec2(pos.x, pos.y + CenterOffset(size.y, sizeinfo.fontSize) - style.FramePadding.y));
				ImGui::SetNextItemWidth(fInW);
				LPCSTR lpHintShown = (bEditable && lpHint != NULL) ? lpHint : "";
				ImGui::InputTextWithHint("##INPUTACTION_TEXT", lpHintShown, lpText, nCap, bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				state |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state |= ItemDisabled() ? WIDGET_STATE_DISABLED : WIDGET_STATE_BASE;
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_CUSTOM_IA_INPUT_SUBMIT;
				}
				ImGui::PopStyleColor(5);

				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, palette.crSurfaceForeground.crBase);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::SetCursorPos(ImVec2(pos.x + size.x - fBtn, pos.y));
				const BOOL bClicked = ImGui::Button(lpBtn ? lpBtn : "##INPUTACTION_BTN", ImVec2(fBtn, size.y));
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(5);
				if (bClicked)
				{
					wOwn = OPER_CUSTOM_IA_BTN_CLICKED;
				}

				drawlist->ChannelsSetCurrent(0);
				const ImU32 crFaceBg = StateColorU32(palette.crBackground, state);
				drawlist->AddRectFilled(boot, end, crFaceBg, fRounding);

				drawlist->ChannelsSetCurrent(2);
				const ImU32 crBorder = StateColorU32(palette.crBorder, state);
				const ImU32 crBorderInner = StateColorU32(palette.crBorderInner, state);
				drawlist->AddRect(boot, end, crBorder, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->AddRect(ImVec2{ mid.x,boot.y }, end, crBorderInner, fRounding, ImDrawFlags_None, 1.0f);

				drawlist->ChannelsMerge();

				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: ComboAction
			// 描述: 下拉动作部件: 左段下拉框 + 右段按钮(补画口径与 InputAction 一致)
			int64_t ComboAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW,
				LPCSTR* lpItems, INT nCount, INT& nIndex, LPCSTR lpBtn)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (lpItems == NULL || nCount <= 0 || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				if (nIndex < 0 || nIndex >= nCount)
				{
					nIndex = 0;
				}
				const FLOAT fBtnMin = size.y;
				FLOAT fBtn = (fBtnW > fBtnMin) ? fBtnW : fBtnMin;
				if (fBtn > size.x - sizeinfo.fontSize)
				{
					fBtn = fBtnMin;
				}
				if (fBtn > size.x - 1.0f)
				{
					fBtn = size.x - 1.0f;
				}
				if (fBtn < 1.0f)
				{
					fBtn = 1.0f;
				}
				FLOAT fComboW = size.x - fBtn - sizeinfo.padding;
				if (fComboW < 1.0f)
				{
					fComboW = 1.0f;
				}
				ImGui::PushID(id);
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				ImGui::SetCursorPos(pos);
				struct
				{
					struct
					{
						ImVec2 boot;
						ImVec2 end;
					}combo, button;
				} position;
				position.combo.boot = ImGui::GetCursorScreenPos();
				position.combo.end = ImVec2{ position.combo.boot.x + fComboW, position.combo.boot.y + size.y };
				position.button.boot = ImVec2{ position.combo.end.x + sizeinfo.padding, position.combo.boot.y };
				position.button.end = ImVec2{ position.button.boot.x + fBtn, position.button.boot.y + size.y };

				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				struct
				{
					WidgetState combo = WIDGET_STATE_BASE;
					WidgetState button = WIDGET_STATE_BASE;
				} state;

				drawlist->ChannelsSplit(2);
				drawlist->ChannelsSetCurrent(0);

				ImGui::PushStyleColor(ImGuiCol_FrameBg, palette.crBackground.crBase);
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, palette.crBackground.crHovered);
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, palette.crBackground.crActive);
				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(ImVec2(pos.x, pos.y + CenterOffset(size.y, sizeinfo.fontSize) - style.FramePadding.y));
				ImGui::SetNextItemWidth(fComboW);
				BOOL bChanged = FALSE;
				{
					const char* preview = lpItems[nIndex];
					if (ImGui::BeginCombo("##COMBOACTION_COMBO", preview))
					{
						bChanged = ComboItems(lpItems, nCount, nIndex);
						ImGui::EndCombo();
					}
				}
				state.combo |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state.combo |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state.combo |= ItemDisabled() ? WIDGET_STATE_DISABLED : WIDGET_STATE_BASE;
				ImGui::PopStyleColor(8);
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_CUSTOM_CA_COMBO_CHANGED;
				}

				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::SetCursorPos(ImVec2(pos.x + size.x - fBtn, pos.y));
				const BOOL bClicked = ImGui::Button(lpBtn ? lpBtn : "##COMBOACTION_BTN", ImVec2(fBtn, size.y));
				state.button |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state.button |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state.button |= ItemDisabled() ? WIDGET_STATE_DISABLED : WIDGET_STATE_BASE;
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(5);
				if (bClicked)
				{
					wOwn = OPER_CUSTOM_CA_BTN_CLICKED;
				}
				drawlist->ChannelsSetCurrent(1);
				const ImU32 crCombo = StateColorU32(palette.crBorder, state.combo);
				const ImU32 crButton = StateColorU32(palette.crBorder, state.button);
				drawlist->AddRect(position.combo.boot, position.combo.end, crCombo, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->AddRect(position.button.boot, position.button.end, crButton, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->ChannelsMerge();

				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: TextAction
			// 描述: 文本动作控件: 命中区为覆盖控件矩形的无绘制项, 文本与按钮自绘
			int64_t TextAction(int id, ImVec2 pos, ImVec2 size, const PaintWay& way, FLOAT fBtnW, LPCSTR lpText, LPCSTR lpBtn)
			{
				using namespace ImmedState;
				int64_t qwOper = 0;
				ImGui::PushID(id);
				// 计算位置尺寸
				struct Geo
				{
					union
					{
						ImVec4 imRECT;
						struct
						{
							ImVec2 boot;
							ImVec2 end;
						}rect;
					}base, text, btn;
					Geo() :base{}, text{}, btn{} {}
				}geo;
				ImGui::SetCursorPos(pos);
				geo.base.rect.boot = ImGui::GetCursorScreenPos();
				geo.base.rect.end = geo.base.rect.boot + size;
				geo.text.rect.boot = geo.base.rect.boot;
				geo.text.rect.end = geo.base.rect.end - ImVec2(fBtnW, 0.0f);
				geo.btn.rect.boot = geo.text.rect.end - ImVec2(0, size.y);
				geo.btn.rect.end = geo.base.rect.end;
				// 计算文本垂直偏移量
				const float TEXTOFF = CenterOffset(size.y, sizeinfo.fontSize);
				// 获取配色方案
				Palette palette = way.palette;
				DWORD crAlpha = 0x000000;
				// 初始化预设状态
				struct
				{
					State base;
					State btn;
				}state;

				// 行为记录
				ImGui::SetCursorPos(pos);
				ImGui::InvisibleButton("##SPANTEXT", size - ImVec2(fBtnW, 0.0f));
				CollectImmedState(state.base);

				ImGui::SetCursorPos(ImVec2(pos.x + size.x - fBtnW, pos.y));
				ImGui::InvisibleButton("##DELETE", ImVec2(fBtnW, size.y));
				CollectImmedState(state.btn);

				// 获取绘制列表
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				// 绘制底层
				const ImU32 crFaceBg = palette.crBackground.StateColor(state.base);
				const ImU32 crFaceDel = palette.crBackground.StateColor(state.btn);
				drawlist->AddRectFilled(geo.base.rect.boot, geo.base.rect.end, crFaceBg, fRounding);
				drawlist->AddRectFilled(geo.btn.rect.boot, geo.btn.rect.end, crFaceDel, fRounding);
				// 绘制内容
				drawlist->AddText(geo.text.rect.boot + ImVec2(sizeinfo.padding, TEXTOFF), palette.crForeground.crBase, lpText);
				drawlist->AddText(geo.btn.rect.boot + ImVec2(CenterOffset(size.y, sizeinfo.fontSize / 2), TEXTOFF), palette.crForeground.crBase, lpBtn);
				// 绘制顶层
				const ImU32 crBorder = palette.crBorder.StateColor(state.base | state.btn);
				const ImU32 crBorderInner = palette.crBorderInner.StateColor(state.btn);
				drawlist->AddRect(geo.base.rect.boot, geo.base.rect.end, crBorder, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->AddRect(geo.btn.rect.boot, geo.btn.rect.end, crBorderInner, fRounding, ImDrawFlags_None, 1.0f);

				// 处理记录操作
				if (state.btn.Is(IMMED_STATE_CLICK_COMPLETED))
				{
					qwOper = (int64_t)((uint64_t)OPER_LIST_ROW_BTN_CLICKED | ((uint64_t)OPER_COMMON_CLICKED << 16));
				}
				else if (state.base.Is(IMMED_STATE_CLICK_COMPLETED))
				{
					qwOper = (int64_t)((uint64_t)OPER_LIST_ROW_ACTIVATED | ((uint64_t)OPER_COMMON_CLICKED << 16));
				}
				ImGui::PopID();
				return qwOper;
			}
			// 函数: SingleSlider
			// 描述: 单手柄滑动条: 命中区为覆盖控件矩形的无绘制项, 轨道与手柄自绘
			int64_t SingleSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t& value, int64_t lMin, int64_t lMax)
			{
				int64_t qwOper = 0;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f || lMax <= lMin)
				{
					return qwOper;
				}
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				UNREFERENCED_PARAMETER(style);
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::InvisibleButton("##SLIDER_ITEM", size);
				const BOOL bHovered = ImGui::IsItemHovered();
				const BOOL bActive = ImGui::IsItemActive();
				const FLOAT r = size.y * SLIDER_GRIP_RATIO;
				const FLOAT trackHalf = r * SLIDER_TRACK_RATIO;
				const FLOAT cy = boot.y + CenterOffset(size.y, r * 2.0f) + r;
				const FLOAT usable = size.x - r * 2.0f;
				if (bActive == TRUE)
				{
					const FLOAT mx = ImGui::GetIO().MousePos.x;
					FLOAT ratio = (usable > 0.0f) ? ((mx - boot.x - r) / usable) : 0.0f;
					if (ratio < 0.0f) { ratio = 0.0f; }
					if (ratio > 1.0f) { ratio = 1.0f; }
					const int64_t nValue = lMin + (int64_t)(ratio * (FLOAT)(lMax - lMin) + 0.5f);
					if (nValue != value)
					{
						value = nValue;
						qwOper = (int64_t)OPER_EDIT_CHANGED;
					}
				}
				{
					FLOAT ratio = (FLOAT)(value - lMin) / (FLOAT)(lMax - lMin);
					if (ratio < 0.0f) { ratio = 0.0f; }
					if (ratio > 1.0f) { ratio = 1.0f; }
					const FLOAT px = boot.x + r + usable * ratio;
					const ImU32 crTrack = StateColorU32(palette.crBackground, WIDGET_STATE_BASE);
					const ImU32 crFill = StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE);
					const ImU32 crGrip = StateColorU32(palette.crBorder,
						(bHovered == TRUE || bActive == TRUE) ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE);
					const ImU32 crGripEdge = StateColorU32(palette.crBorder, WIDGET_STATE_BASE);
					drawlist->AddRectFilled(ImVec2(boot.x, cy - trackHalf), ImVec2(end.x, cy + trackHalf), crTrack, trackHalf);
					drawlist->AddRectFilled(ImVec2(boot.x, cy - trackHalf), ImVec2(px, cy + trackHalf), crFill, trackHalf);
					drawlist->AddCircleFilled(ImVec2(px, cy), r, crGrip);
					drawlist->AddCircle(ImVec2(px, cy), r, crGripEdge, 0, 1.0f);
				}
				ImGui::PopID();
				return qwOper;
			}
			// 函数: RangeSlider
			// 描述: 双手柄范围滑动条(见 .h)
			int64_t RangeSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way,
				int64_t& vMin, int64_t& vMax, int64_t lMin, int64_t lMax)
			{
				int64_t qwOper = 0;
				static int sThumb = 0;
				static int sThumbId = -1;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f || lMax <= lMin)
				{
					return qwOper;
				}
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				CONST ImVec2 boot = ImGui::GetCursorScreenPos();
				CONST ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::InvisibleButton("##RANGE_ITEM", size);
				CONST BOOL bHovered = ImGui::IsItemHovered();
				CONST BOOL bActive = ImGui::IsItemActive();
				CONST FLOAT r = size.y * SLIDER_GRIP_RATIO;
				CONST FLOAT trackHalf = r * SLIDER_TRACK_RATIO;
				CONST FLOAT cy = boot.y + CenterOffset(size.y, r * 2.0f) + r;
				CONST FLOAT usable = size.x - r * 2.0f;
				// 1. 进入拖动时按鼠标位置选择较近的手柄
				if (ImGui::IsItemActivated())
				{
					CONST FLOAT mx = ImGui::GetIO().MousePos.x;
					FLOAT ratioMin = (FLOAT)(vMin - lMin) / (FLOAT)(lMax - lMin);
					FLOAT ratioMax = (FLOAT)(vMax - lMin) / (FLOAT)(lMax - lMin);
					if (ratioMin < 0.0f) { ratioMin = 0.0f; }
					if (ratioMin > 1.0f) { ratioMin = 1.0f; }
					if (ratioMax < 0.0f) { ratioMax = 0.0f; }
					if (ratioMax > 1.0f) { ratioMax = 1.0f; }
					CONST FLOAT pxMin = boot.x + r + usable * ratioMin;
					CONST FLOAT pxMax = boot.x + r + usable * ratioMax;
					sThumb = ((mx - pxMin) <= (pxMax - mx)) ? 0 : 1;
					sThumbId = id;
				}
				// 2. 拖动中写回当前手柄值(两手柄互相钳制)
				if (bActive == TRUE)
				{
					if (sThumbId != id)
					{
						sThumb = 0;
						sThumbId = id;
					}
					CONST FLOAT mx = ImGui::GetIO().MousePos.x;
					FLOAT ratio = (usable > 0.0f) ? ((mx - boot.x - r) / usable) : 0.0f;
					if (ratio < 0.0f) { ratio = 0.0f; }
					if (ratio > 1.0f) { ratio = 1.0f; }
					CONST int64_t nValue = lMin + (int64_t)(ratio * (FLOAT)(lMax - lMin) + 0.5f);
					if (sThumb == 0)
					{
						CONST int64_t nNew = (nValue < vMax) ? nValue : vMax;
						if (nNew != vMin)
						{
							vMin = nNew;
							qwOper = (int64_t)((uint64_t)OPER_SLIDER_THUMB_LEFT |
								((uint64_t)OPER_EDIT_CHANGED << 16));
						}
					}
					else
					{
						CONST int64_t nNew = (nValue > vMin) ? nValue : vMin;
						if (nNew != vMax)
						{
							vMax = nNew;
							qwOper = (int64_t)((uint64_t)OPER_SLIDER_THUMB_RIGHT |
								((uint64_t)OPER_EDIT_CHANGED << 16));
						}
					}
				}
				// 3. 绘制轨道、选中区间与两个手柄
				{
					FLOAT ratioMin = (FLOAT)(vMin - lMin) / (FLOAT)(lMax - lMin);
					FLOAT ratioMax = (FLOAT)(vMax - lMin) / (FLOAT)(lMax - lMin);
					if (ratioMin < 0.0f) { ratioMin = 0.0f; }
					if (ratioMin > 1.0f) { ratioMin = 1.0f; }
					if (ratioMax < 0.0f) { ratioMax = 0.0f; }
					if (ratioMax > 1.0f) { ratioMax = 1.0f; }
					CONST FLOAT pxMin = boot.x + r + usable * ratioMin;
					CONST FLOAT pxMax = boot.x + r + usable * ratioMax;
					CONST ImU32 crTrack = StateColorU32(palette.crBackground, WIDGET_STATE_BASE);
					CONST ImU32 crFill = StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE);
					CONST ImU32 crGrip = StateColorU32(palette.crBorder,
						(bHovered == TRUE || bActive == TRUE) ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE);
					CONST ImU32 crGripEdge = StateColorU32(palette.crBorder, WIDGET_STATE_BASE);
					drawlist->AddRectFilled(ImVec2(boot.x, cy - trackHalf), ImVec2(end.x, cy + trackHalf), crTrack, trackHalf);
					drawlist->AddRectFilled(ImVec2(pxMin, cy - trackHalf), ImVec2(pxMax, cy + trackHalf), crFill, trackHalf);
					drawlist->AddCircleFilled(ImVec2(pxMin, cy), r, crGrip);
					drawlist->AddCircle(ImVec2(pxMin, cy), r, crGripEdge, 0, 1.0f);
					drawlist->AddCircleFilled(ImVec2(pxMax, cy), r, crGrip);
					drawlist->AddCircle(ImVec2(pxMax, cy), r, crGripEdge, 0, 1.0f);
				}
				ImGui::PopID();
				return qwOper;
			}
			// 函数: ProgressStatic
			// 描述: 进度状态显示件: 自绘圆角底 + 进度文本(恒返回 FALSE)
			BOOL ProgressStatic(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t current, int64_t expect, FLOAT fPadding)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				CONST Palette& palette = way.palette;
				const FLOAT padding = fPadding;
				double prog = expect ? (current * 100.0 / expect) : 0.0;
				ImGui::SetCursorPos(pos);
				ImVec2 cur = ImGui::GetCursorScreenPos();
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				drawlist->AddRectFilled(cur, cur + size, StateColorU32(palette.crBackground, WIDGET_STATE_BASE), fRounding);
				ImGui::SetCursorPos(ImVec2(pos.x + padding, pos.y + CenterOffset(size.y, sizeinfo.fontSize)));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::Text(u8"进度: %6.2f%%", prog);
				ImGui::PopStyleColor();
				ImGui::SetCursorPos(pos);
				ImGui::Dummy(size);
				return FALSE;
			}
			// 函数: InputAction(列表项重载)
			// 描述: 列表项部件: 解包行数据后转对应基础部件, 行首拖拽把手占位后行内容右移
			int64_t InputAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlInputActionData* pData = (CtrlInputActionData*)lpItem;
				CHAR* lpText = CtrlInputActionText(pData);
				CHAR* lpBtnText = CtrlInputActionBtnText(pData);
				if (lpText == NULL)
				{
					return qwOper;
				}
				LPCSTR lpBtn = (lpBtnText != NULL && lpBtnText[0] != '\0') ? lpBtnText : CTRL_DEFAULT_ACTION_BTN;
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2;
				const size_t nTextCap = CTRL_TEXT_BUF;
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				qwOper = InputAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					lpText, nTextCap, NULL, lpBtn);
				{
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: InputActionRO(列表项重载, 只读)
			// 描述: 列表项部件只读形态: 文本容量按 0 传入, 转对应的基础部件走只读分支
			int64_t InputActionRO(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlInputActionData* pData = (CtrlInputActionData*)lpItem;
				CHAR* lpText = CtrlInputActionText(pData);
				CHAR* lpBtnText = CtrlInputActionBtnText(pData);
				if (lpText == NULL)
				{
					return qwOper;
				}
				LPCSTR lpBtn = (lpBtnText != NULL && lpBtnText[0] != '\0') ? lpBtnText : u8"-";
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2;
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				qwOper = InputAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					lpText, 0, NULL, lpBtn);
				{
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: ComboAction(列表项重载)
			// 描述: 列表项部件: 解包选项表后转对应基础部件, 触发结论打包返回
			int64_t ComboAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlComboData* pData = (CtrlComboData*)lpItem;
				if (pData->items == NULL || pData->count <= 0)
				{
					return qwOper;
				}
				LPCSTR lpRowBtn = (pData->lpBtn != NULL) ? pData->lpBtn : CTRL_DEFAULT_ACTION_BTN;
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpRowBtn) + 2) / 2;
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				qwOper = ComboAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					pData->items, pData->count, pData->index, lpRowBtn);
				{
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: Button
			// 描述: 按钮件: 三态面取自配色方案 + 面内文字(自带可交互项)
			int64_t Button(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				int64_t qwOper = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				ImGui::PushStyleColor(ImGuiCol_Button, StateColorU32(palette.crSurface, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, StateColorU32(palette.crSurface, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, StateColorU32(palette.crSurface, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				if (ImGui::Button(lpText ? lpText : "", size))
				{
					wCommon |= OPER_COMMON_CLICKED;
				}
				ImGui::PopStyleColor(4);
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wCommon << 16);
				return qwOper;
			}
			// 函数: Text
			// 描述: 文本图元件: 部件矩形内垂直居中绘制一行文本, 裁剪到部件矩形
			int64_t Text(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				const int64_t qwOper = 0;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				CONST FLOAT fTextY = pos.y + CenterOffset(size.y, sizeinfo.fontSize);
				ImGui::SetCursorPos(ImVec2(pos.x, fTextY));
				ImGui::PushClipRect(boot, end, TRUE);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::Text("%s", lpText ? lpText : "");
				ImGui::PopStyleColor();
				ImGui::PopClipRect();
				ImGui::PopID();
				return qwOper;
			}
			// 函数: TextMulti
			// 描述: 块级多行文本件: 折行交给 ImGui 文本层(PushTextWrapPos + TextUnformatted), 本件不再自绘折行
			int64_t TextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				const int64_t qwOper = 0;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				FLOAT fPad = sizeinfo.padding;
				if (fPad < 0.0f)
				{
					fPad = 0.0f;
				}
				const FLOAT fMaxPad = ((size.x < size.y) ? size.x : size.y) * 0.5f;
				if (fPad > fMaxPad)
				{
					fPad = fMaxPad;
				}
				const FLOAT fWrapW = size.x - fPad * 2.0f;
				const FLOAT fAreaH = size.y - fPad * 2.0f;
				if (fWrapW < 1.0f || fAreaH < 1.0f)
				{
					return qwOper;
				}
				LPCSTR lpBody = (lpText != NULL) ? lpText : "";
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(ImVec2(pos.x + fPad, pos.y + fPad));
				ImGui::PushTextWrapPos(pos.x + fPad + fWrapW);
				ImGui::PushClipRect(boot, end, TRUE);
				ImGui::TextUnformatted(lpBody);
				ImGui::PopClipRect();
				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
				ImGui::PopID();
				return qwOper;
			}
			// 函数: CheckBox
			// 描述: 自定义复选框件
			int64_t CheckBox(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, BOOL& IsChecked, LPCSTR lpLabel)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}

				LPCSTR lpText = (lpLabel != NULL && lpLabel[0] != '\0') ? lpLabel : "##CHECKBOX";
				bool bChecked = (IsChecked == TRUE);

				ImGui::PushID(id);

				ImGuiWindow* pWindow = ImGui::GetCurrentWindow();
				if (pWindow->SkipItems)
				{
					ImGui::PopID();
					return qwOper;
				}
				const ImGuiID itemId = pWindow->GetID(lpText);
				float squareSize = size.y - (float)sizeinfo.padding;
				struct
				{
					struct
					{
						ImVec2 pos;
						ImVec2 size;
					} rect, box, text;
				}geo;

				ImGui::SetCursorPos(pos);
				geo.rect.pos = ImGui::GetCursorScreenPos();
				geo.rect.size = size;

				geo.box.size = ImVec2(squareSize, squareSize);
				geo.box.pos = ImVec2(geo.rect.pos.x, geo.rect.pos.y + CenterOffset(size.y, squareSize));

				geo.text.size = ImGui::CalcTextSize(lpText, NULL, true);
				geo.text.pos = ImVec2(geo.box.pos.x + geo.box.size.x + sizeinfo.padding, geo.rect.pos.y + CenterOffset(size.y, geo.text.size.y));

				const ImRect totalRect(geo.rect.pos, geo.rect.pos + geo.rect.size);

				ImGui::ItemSize(size, 0.0f);
				if (!ImGui::ItemAdd(totalRect, itemId))
				{
					ImGui::PopID();
					return qwOper;
				}

				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));

				bool hovered = false;
				bool held = false;
				bool pressed = ImGui::ButtonBehavior(totalRect, itemId, &hovered, &held);
				if (pressed)
				{
					bChecked = !bChecked;
					IsChecked = bChecked ? TRUE : FALSE;
					wOwn = OPER_EDIT_CHANGED;
					ImGui::MarkItemEdited(itemId);
				}

				ImDrawList* pDrawList = pWindow->DrawList;

				WidgetState state = WIDGET_STATE_BASE;
				if (hovered) {
					state |= WIDGET_STATE_HOVERED;
				}
				if (held && hovered)
				{
					state |= WIDGET_STATE_ACTIVE;
				}

				const ImU32 crBack = palette.crBackground.StateColor(state);
				const ImU32 crCheck = palette.crForeground.crBase;

				ImGui::RenderNavHighlight(totalRect, itemId);

				ImGui::RenderFrame(geo.box.pos, geo.box.pos + geo.box.size, crBack, true, fRounding);

				if (bChecked)
				{
					ImGui::RenderCheckMark(pDrawList, geo.box.pos + ImVec2(sizeinfo.padding, sizeinfo.padding), crCheck, geo.box.size.x - sizeinfo.padding * 2.0f);
				}

				if (geo.text.size.x > 0.0f)
				{
					ImGui::RenderText(geo.text.pos, lpText);
				}

				ImGui::PopStyleColor(2);
				ImGui::PopID();

				qwOper = (int64_t)wOwn;
				return qwOper;
			}
			// 函数: RenderSurface
			// 描述: 渲染面件: 有纹理时等比居中绘制, 无纹理时画占位面
			BOOL RenderSurface(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CONST VOID* pTexture, INT32 texWidth, INT32 texHeight)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				CONST Palette& palette = way.palette;
				const BOOL bHaveImage = (pTexture != NULL && texWidth > 0 && texHeight > 0) ? TRUE : FALSE;
				if (bHaveImage == TRUE && size.x > 0.0f && size.y > 0.0f)
				{
					FLOAT scale = size.x / (FLOAT)texWidth;
					if ((size.y / (FLOAT)texHeight) < scale)
					{
						scale = size.y / (FLOAT)texHeight;
					}
					const FLOAT wDraw = (FLOAT)texWidth * scale;
					const FLOAT hDraw = (FLOAT)texHeight * scale;
					ImGui::PushID(id);
					ImGui::SetCursorPos(ImVec2(pos.x + CenterOffset(size.x, wDraw), pos.y + CenterOffset(size.y, hDraw)));
					ImGui::Image(ImTextureRef((void*)pTexture), ImVec2(wDraw, hDraw));
					ImGui::PopID();
					return TRUE;
				}
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				CONST ImVec2 o = ImGui::GetCursorScreenPos();
				DrawRectFace(dl, o, size, StateColorU32(palette.crBackground, WIDGET_STATE_BASE),
					StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				{
					LPCSTR lpTip = u8"（等待结果图）";
					CONST ImVec2 ts = ImGui::CalcTextSize(lpTip);
					dl->AddText(ImVec2(o.x + CenterOffset(size.x, ts.x), o.y + CenterOffset(size.y, sizeinfo.fontSize)),
						StateColorU32(palette.crForeground, WIDGET_STATE_BASE), lpTip);
				}
				ImGui::PopID();
				return FALSE;
			}
		}
		namespace Command
		{
			// 函数: List
			// 描述: 命令部件: 按列表描述符一次画完(六步), 逐行收集操作码并回调
			int64_t List(CONST ListSpec& spec)
			{
				int64_t qwOper = 0;
				uint64_t qwParam = 0;
				const FLOAT fInner = sizeinfo.padding;
				const FLOAT fGap = sizeinfo.space;
				vec2 szItem{};
				vec2 posCur{};
				FLOAT fStep = 0.0f;
				const BOOL bSpecial = (spec.pfnSpecial != NULL) ? TRUE : FALSE;
				const FLOAT fSpecialH = (FLOAT)((spec.dwSpecialFlag >> 16) & 0xFFFF);
				const BOOL bSpecialFirst = (bSpecial && ((spec.dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST)) ? TRUE : FALSE;
				DWORD dwSpecialId = 0;
				DWORD dwCount = spec.dwCount;
				if (spec.pfnItem == NULL || spec.size.x <= 0.0f || spec.size.y <= 0.0f)
				{
					return qwOper;
				}
				if (spec.lpItemBase == NULL)
				{
					dwCount = 0;
				}
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, spec.fRounding);
				ImGui::SetCursorPos(toImvec2(spec.pos));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, spec.crShell);
				ImGui::BeginChild(spec.label ? spec.label : u8"##LISTPANEL", toImvec2(spec.size), true, 0);
				ImGui::PopStyleColor();
				{
					FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;
					if (spec.fRowH * dwCount > spec.size.y)
					{
						fW = fW - sizeinfo.padding * 2.0f;
					}
					if (fW < 1.0f)
					{
						fW = 1.0f;
					}
					szItem = vec2(fW, spec.fRowH);
					posCur = vec2(fInner, fInner);
					fStep = spec.fRowH + fGap;
				}
				CONST PaintWay way = Base::MakePaletteWay(spec.palette);
				if (bSpecial && bSpecialFirst)
				{
					int64_t qwSpecial = spec.pfnSpecial((int)dwSpecialId, posCur,
						vec2(szItem.x, fSpecialH), way, spec.lpSpecial);
					if (qwSpecial != 0)
					{
						ListCollectRowOper(qwSpecial, dwSpecialId, qwOper, qwParam);
					}
					posCur.y += fSpecialH + fGap;
				}
				for (DWORD i = 0; i < dwCount; i++)
				{
					VOID* lpItem = (VOID*)((BYTE*)spec.lpItemBase + spec.cbItem * i);
					int64_t qwRow = spec.pfnItem((int)i, posCur, szItem, way, lpItem);
					if (qwRow != 0)
					{
						ListCollectRowOper(qwRow, i, qwOper, qwParam);
					}
					posCur.y += fStep;
				}
				if (bSpecial && bSpecialFirst == FALSE)
				{
					dwSpecialId = dwCount;
					int64_t qwSpecial = spec.pfnSpecial((int)dwSpecialId, posCur,
						vec2(szItem.x, fSpecialH), way, spec.lpSpecial);
					if (qwSpecial != 0)
					{
						ListCollectRowOper(qwSpecial, dwSpecialId, qwOper, qwParam);
					}
					posCur.y += fSpecialH + fGap;
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				if (qwOper != 0 && spec.pfnEvent != NULL)
				{
					spec.pfnEvent(spec.pEventObj, (uint64_t)qwOper, qwParam, spec.pEventData);
				}
				return qwOper;
			}
		}

		namespace Pipe
		{
			// 函数: ListRow
			// 描述: 管道部件: 进入同一列表子窗逐行绘制(五步), 行游标与几何由调用方算好
			int64_t ListRow(CONST ListPipe& pipe, int nRowId, vec2 posRow, FLOAT fRowH,
				VOID* lpItem, pfnListWidget pfnItem, CONST PaintWay& way)
			{
				int64_t qwOper = 0;
				const FLOAT fInner = sizeinfo.space;
				if (pipe.size.x <= 0.0f || pipe.size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, pipe.fRounding);
				ImGui::SetCursorPos(toImvec2(pipe.pos));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, pipe.crShell);
				ImGui::BeginChild(pipe.label ? pipe.label : u8"##LISTPANEL", toImvec2(pipe.size), true, 0);
				ImGui::PopStyleColor();
				if (pfnItem != NULL)
				{
					FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;
					if (fRowH * pipe.dwCount > pipe.size.y)
					{
						fW = fW - sizeinfo.padding * 2.0f;
					}
					if (fW < 1.0f)
					{
						fW = 1.0f;
					}
					qwOper = pfnItem(nRowId, posRow, vec2(fW, fRowH), way, lpItem);
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				return qwOper;
			}
		}
	}
}

namespace Controls
{
	namespace Widgets
	{

		// 函数: DrawRectFace
		// 描述: 画矩形外观(圆角底 + 圆角描边; 色值由调用方给出, 本函数只画不取色)
		static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder)
		{
			if (dl == NULL)
			{
				return;
			}
			ImVec2 e(o.x + size.x, o.y + size.y);
			dl->AddRectFilled(o, e, crBase, fRounding);
			dl->AddRect(o, e, crBorder, fRounding, 0, 1.0f);
		}
		// 函数: ComboItems
		// 描述: 在已打开的下拉列表内提交项表(Combo 与 ComboAction 共用, 展开列表观感由此单一处定义)
		static BOOL ComboItems(LPCSTR* lpItems, INT nCount, INT& nIndex)
		{
			const INT nOld = nIndex;
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			for (INT i = 0; i < nCount; i++)
			{
				const BOOL IsSelected = (nIndex == i) ? TRUE : FALSE;
				const ImVec2 item_size(0.0f, sizeinfo.controlHeight);
				if (ImGui::Selectable(lpItems[i], IsSelected != FALSE, 0, item_size))
				{
					nIndex = i;
				}
				if (IsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::PopStyleVar(2);
			return (nOld != nIndex) ? TRUE : FALSE;
		}
		// 函数: ListRowDragHandle
		// 描述: 行首拖拽把手(拖拽源): 只登记拖拽并自绘图标, 返回占用宽度供调用方让位
		static FLOAT ListRowDragHandle(int id, vec2 pos, vec2 size, DWORD dwRow)
		{
			FLOAT fRet = 0.0f;
			if (size.x <= 0.0f || size.y <= 0.0f)
			{
				return fRet;
			}
			fRet = sizeinfo.controlHeight;
			if (fRet > size.x * 0.5f)
			{
				fRet = size.x * 0.5f;
			}
			if (fRet < 1.0f)
			{
				fRet = 1.0f;
			}
			ImGui::PushID(id);
			ImGui::SetCursorPos(toImvec2(pos));
			const ImVec2 handleMin = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton(u8"##DRAGHANDLE", ImVec2(fRet, size.y));
			const BOOL bHovered = ((ImGui::IsItemHovered() != false) || (ImGui::IsItemActive() != false)) ? TRUE : FALSE;
			if (ImGui::BeginDragDropSource())
			{
				ListRowDragPayload payload{};
				payload.dwRow = dwRow;
				ImGui::SetDragDropPayload(GENUI_LIST_DRAG_TYPE, &payload, sizeof(payload));
				ImGui::Text(u8"第 %u 行", (unsigned)dwRow);
				ImGui::EndDragDropSource();
			}
			{
				ImDrawList* dl = ImGui::GetWindowDrawList();
				Base::StateColors crHandle{};
				crHandle.crBase = ProjColor(PROJ_L_BORDER_HOVER);
				crHandle.crHovered = ProjColor(PROJ_L_BORDER_ACTIVE);
				const ImU32 crLine = Base::StateColorU32(crHandle,
					(bHovered != FALSE) ? Base::WIDGET_STATE_HOVERED : Base::WIDGET_STATE_BASE);
				const FLOAT fLineW = fRet * 0.5f;
				const FLOAT fLineH = 2.0f;
				const FLOAT fLineGap = sizeinfo.padding;
				const FLOAT fIconH = fLineH * 3.0f + fLineGap * 2.0f;
				const FLOAT cx = handleMin.x + Base::CenterOffset(fRet, fLineW);
				const FLOAT cy = handleMin.y + Base::CenterOffset(size.y, fIconH) + fIconH * 0.5f;
				for (INT k = -1; k <= 1; k++)
				{
					const FLOAT ly = cy + (FLOAT)k * (fLineH + fLineGap) - fLineH * 0.5f;
					dl->AddRectFilled(ImVec2(cx, ly), ImVec2(cx + fLineW, ly + fLineH), crLine);
				}
			}
			ImGui::PopID();
			return fRet;
		}
		// 函数: ListRowDropTarget
		// 描述: 整行登记为放置目标并在该侧自绘插入线(落点换算走公开件 ListRowDropOper; 返回 0 = 无投放)
		static int64_t ListRowDropTarget(int id, vec2 pos, vec2 size, DWORD dwRow)
		{
			int64_t qwRet = 0;
			if (size.x <= 0.0f || size.y <= 0.0f)
			{
				return qwRet;
			}
			ImGui::PushID(id);
			ImGui::SetCursorPos(toImvec2(pos));
			const ImVec2 cur = ImGui::GetCursorScreenPos();
			const ImRect drop = { cur , cur + toImvec2(size) };
			if (ImGui::BeginDragDropTargetCustom(drop, ImGui::GetID(u8"##LISTROWDROP")))
			{
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GENUI_LIST_DRAG_TYPE, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
				if (payload != NULL && payload->DataSize == (int)sizeof(ListRowDragPayload))
				{
					const DWORD dwSrc = ((const ListRowDragPayload*)payload->Data)->dwRow;
					const FLOAT fMid = (drop.Min.y + drop.Max.y) * 0.5f;
					const BOOL IsAfter = (ImGui::GetIO().MousePos.y > fMid) ? TRUE : FALSE;
					const float padding = sizeinfo.padding / 2;
					const float rounding = fRounding;
					const float thickness = 2.0f;
					const DWORD crEmphasis = ProjColor(PROJ_L_BORDER_ACTIVE);
					float vert = IsAfter ? cur.y + size.y - thickness : cur.y;
					ImDrawList* drawlist = ImGui::GetWindowDrawList();
					ImRect border = { cur - ImVec2(padding,padding), cur + toImvec2(size) + ImVec2(padding,padding) };
					ImRect line = { { cur.x, vert },{ cur.x + size.x, vert + thickness } };
					drawlist->AddRectFilled(line.Min, line.Max, crEmphasis, rounding, ImDrawFlags_RoundCornersAll);
					drawlist->AddRect(border.Min, border.Max, crEmphasis, rounding, ImDrawFlags_None, 2.0f);
					if (payload->IsDelivery())
					{
						qwRet = Controls::ListRowDropOper(dwSrc, dwRow, IsAfter);
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
			return qwRet;
		}
		// 函数: ListCollectRowOper
		// 描述: 把列表项部件返回约定折算进控件的 oper 与回调 param(收集口径 = 取最后一个非 0 值)
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

	}
}
