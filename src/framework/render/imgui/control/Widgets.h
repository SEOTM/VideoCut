#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <windows.h>
#include "framework/base/BasicDesign.h"
#include "framework/base/ControlSpec.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace Controls
{
	static inline void CollectImmedState(ImmedState::State& state);

	namespace Widgets
	{
		namespace Base
		{
			enum WidgetState : DWORD
			{
				WIDGET_STATE_BASE = 0,
				WIDGET_STATE_HOVERED = 1,
				WIDGET_STATE_ACTIVE = 2,
				WIDGET_STATE_DISABLED = 4,
				WIDGET_STATE_DEACTIVATED = 8,
				WIDGET_STATE_FOCUSED = 16,
				WIDGET_STATE_EDITED = 32,
				WIDGET_STATE_SUBMITTED = 64,
				WIDGET_STATE_DRAGGING = 128,

			};
			// 函数: WidgetState::operator |
			// 描述: 位或: 合并两枚部件状态位
			static inline WidgetState operator|(WidgetState lhs, WidgetState rhs)
			{
				return static_cast<WidgetState>(static_cast<DWORD>(lhs) | static_cast<DWORD>(rhs));
			}
			// 函数: WidgetState::operator &
			// 描述: 位与: 判定部件状态位是否置位
			static inline WidgetState operator&(WidgetState lhs, WidgetState rhs)
			{
				return static_cast<WidgetState>(static_cast<DWORD>(lhs) & static_cast<DWORD>(rhs));
			}
			// 函数: WidgetState::operator |=
			// 描述: 复合位或: 就地置位部件状态
			static inline WidgetState& operator|=(WidgetState& lhs, WidgetState rhs)
			{
				return lhs = lhs | rhs;
			}
			struct StateColors
			{
				DWORD crBase;
				DWORD crHovered;
				DWORD crActive;
				DWORD crDisable;
				DWORD StateColor(WidgetState state) const
				{
					if (state & WIDGET_STATE_DISABLED) return crDisable;
					if (state & WIDGET_STATE_ACTIVE)  return crActive;
					if (state & WIDGET_STATE_HOVERED) return crHovered;
					return crBase;
				}
				// 增加一组重载
				DWORD StateColor(ImmedState::State state) const
				{
					if (state.IsDisabled()) return crDisable;
					if (state.IsActive()) return crActive;
					if (state.IsHovered()) return crHovered;
					return crBase;
				}
			};
			struct Palette
			{
				StateColors crBackground;
				StateColors crForeground;
				StateColors crSurface;
				StateColors crSurfaceForeground;
				StateColors crBorder;
				StateColors crBorderInner;
			};

			enum desc
			{
				PALETTE = 0,
				IMAGE = 1
			};

			struct PaintWay
			{
				desc eDesc;
				union
				{
					Palette palette;
					void* pImage;
				};
			};

			// 函数: MakePaletteWay
			// 描述: 构造 PALETTE 绘制方式(值返回, 供调用点直接作实参)
			static inline PaintWay MakePaletteWay(CONST Palette& palette)
			{
				PaintWay way{};
				way.eDesc = PALETTE;
				way.palette = palette;
				return way;
			}

			// 函数: ExitSign
			// 描述: 关闭按钮部件: 两段交叉线符号 + 悬停底(标题栏用)
			BOOL ExitSign(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR label, FLOAT fPadding, FLOAT thickness);
			// 函数: StateColorU32
			// 描述: 把状态色换算为 ImDrawList 可用色(再按当前 Style.Alpha 乘算)
			static inline ImU32 StateColorU32(CONST StateColors& crs, WidgetState state)
			{
				return ImGui::GetColorU32((ImU32)crs.StateColor(state));
			}
			// 函数: ItemDisabled
			// 描述: 读最后一个项的禁用标志(TRUE = 该项本帧被禁用)
			static inline BOOL ItemDisabled()
			{
				return (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) ? TRUE : FALSE;
			}
			// 函数: CenterOffset
			// 描述: 计算容器与内容的居中偏移量(下限 0, 内容不小于容器时返回 0)
			static inline FLOAT CenterOffset(FLOAT parent, FLOAT child)
			{
				FLOAT fRet = (parent - child) * 0.5f;
				if (fRet < 0.0f)
				{
					fRet = 0.0f;
				}
				return fRet;
			}
			// 函数: InputText
			// 描述: 单行文本件(底与边框由 ImGui 原生绘制, 控件高度对齐声明高)
			int64_t InputText(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint);
			// 函数: InputTextMulti
			// 描述: 多行文本件(项尺寸取声明矩形, 不做单行那样的垂直居中)
			int64_t InputTextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint);
			// 函数: InputScalar
			// 描述: 数值件(底与边框由 ImGui 原生绘制, 文字经原生内边距居中)
			int64_t InputScalar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, VOID* pData, ImGuiDataType eType,
				LPCSTR lpFormat, LPCSTR lpHint);
			// 函数: Combo
			// 描述: 下拉件(展开列表的项表与 ComboAction 同源)
			int64_t Combo(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR* lpItems, INT nCount,
				INT& nIndex);
			// 函数: TimeSpan
			// 描述: 时段显示件: 自绘圆角底 + 起点到终点的时码文本(无交互项)
			int64_t TimeSpan(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t nStartMs, int64_t nEndMs,
				int64_t nLimitMs);
			// 函数: TimeBar
			// 描述: 可编辑时间条: 提示文本 + 圆角底 + 时/分/秒/毫秒四段数字输入(仿旧 VideoCut 的时码条)
			int64_t TimeBar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpHint,
				int64_t& rValueMs, int64_t nLimitMs);
			// 函数: InputAction
			// 描述: 输入动作部件: 左段单行输入 + 右段按钮(两段底与边框在项提交后补画)
			int64_t InputAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW, CHAR* lpText,
				size_t nTextCap, LPCSTR lpHint, LPCSTR lpBtn);
			// 函数: ComboAction
			// 描述: 下拉动作部件: 左段下拉框 + 右段按钮(补画口径与 InputAction 一致)
			int64_t ComboAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW,
				LPCSTR* lpItems, INT nCount, INT& nIndex, LPCSTR lpBtn);

			// 函数: TextAction
			// 描述: 文本动作部件
			int64_t TextAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW, LPCSTR lpText, LPCSTR lpBtn);

			// 函数: SingleSlider
			// 描述: 单手柄滑动条件(命中区为覆盖整个控件矩形的无绘制项)
			int64_t SingleSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t& value, int64_t lMin, int64_t lMax);
			// 函数: RangeSlider
			// 描述: 双手柄范围滑动条(左/右手柄分别返回专属段, 命中区覆盖整个控件矩形)
			int64_t RangeSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way,
				int64_t& vMin, int64_t& vMax, int64_t lMin, int64_t lMax);
			// 函数: ProgressStatic
			// 描述: 进度状态显示件(底与文本取自配色方案)
			BOOL ProgressStatic(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t current, int64_t expect, FLOAT fPadding);

			// 函数: Button
			// 描述: 按钮件: 三态面 + 面内文字(自带可交互项)
			int64_t Button(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: Text
			// 描述: 文本图元件: 部件矩形内垂直居中绘制一行文本, 裁剪到部件矩形
			int64_t Text(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: TextMulti
			// 描述: 块级多行文本件: 文本区四边内缩一枚设计内边距, 按文本区宽折行
			int64_t TextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: CheckBox
			// 描述: 复选框件: 方框/勾/文字由 ImGui 自带控件绘制, 勾选态经引用写回
			int64_t CheckBox(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, BOOL& IsChecked, LPCSTR lpLabel);

			// 函数: RenderSurface
			// 描述: 渲染面件: 有纹理时等比居中绘制, 无纹理时画占位面
			BOOL RenderSurface(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CONST VOID* pTexture, INT32 texWidth, INT32 texHeight);

			using pfnListWidget = int64_t(*)(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: InputAction(列表项重载)
			// 描述: 列表项部件: 解包行数据后转对应基础部件(行首拖拽把手占位)
			int64_t InputAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: InputActionRO(列表项重载, 只读)
			// 描述: 列表项部件只读形态: 文本容量按 0 传入, 转对应的基础部件走只读分支
			int64_t InputActionRO(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: ComboAction(列表项重载)
			// 描述: 列表项部件: 解包选项表后转对应基础部件, 触发结论打包返回
			int64_t ComboAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);
		}

		namespace Command
		{
			using Base::pfnListWidget;
			using Base::Palette;
			using Base::PaintWay;

			struct ListSpec
			{
				int id;
				vec2 pos;
				vec2 size;
				FLOAT fRowH;
				DWORD dwSelected;
				DWORD dwCount;
				Palette palette;
				DWORD crShell;
				pfnListWidget pfnItem;
				VOID* lpItemBase;
				size_t cbItem;
				pfnListWidget pfnSpecial;
				VOID* lpSpecial;
				size_t cbSpecial;
				DWORD dwSpecialFlag;
				pfnCtrlEvent pfnEvent;
				VOID* pEventObj;
				void* pEventData;
				LPCSTR label;
				FLOAT fRounding;
			};

			// 函数: List
			// 描述: 命令部件: 按列表描述符一次画完并逐行收集操作码(六步见 Widgets.md 的 List 节)
			int64_t List(CONST ListSpec& spec);
		}

		namespace Pipe
		{
			using Base::pfnListWidget;
			using Base::Palette;
			using Base::PaintWay;

			struct ListPipe
			{
				int id;
				vec2 pos;
				vec2 size;
				FLOAT fRowH;
				DWORD dwCount;
				DWORD crShell;
				LPCSTR label;
				FLOAT fRounding;
			};

			// 函数: ListRow
			// 描述: 管道部件: 逐行送入并绘制一行(五步见 Widgets.md 的 ListRow 节)
			int64_t ListRow(CONST ListPipe& pipe, int nRowId, vec2 posRow, FLOAT fRowH,
				VOID* lpItem, pfnListWidget pfnItem, CONST PaintWay& way);
		}

		// 函数: UpdateContext
		// 描述: 更新部件层上下文(每帧渲染布局之前调用一次, 并复位编辑中计数)
		BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj);

		// 函数: HasActiveInput
		// 描述: 查询本帧是否存在处于编辑中的输入项(供控件层判定 IME 开关)
		BOOL HasActiveInput();
	}
}
