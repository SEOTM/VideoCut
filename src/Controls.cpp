// ================================================
// 库名: Controls
// 描述: 封装 Dear ImGui 控件的自定义控件库，并包含一些辅助函数和 Render 绘制函数
// ================================================

// ================================
// 区域: 编译常量定义区
// ================================
#define IMGUI_DEFINE_MATH_OPERATORS

// ================================
// 区域: 文件包含区
// ================================
#include "Controls.h"
#include "imgui.h"
#include <cmath>
#include <stdio.h>
#include <d3d11.h>
#include <algorithm>

// ================================
// 区域: 常量定义区
// ================================
static constexpr ImGuiWindowFlags dwMainWindowFlags =	// 窗口状态
ImGuiWindowFlags_NoTitleBar |				// 无标题栏
ImGuiWindowFlags_NoResize |					// 禁止调整大小
ImGuiWindowFlags_NoMove |					// 禁止移动
ImGuiWindowFlags_NoCollapse |				// 禁止折叠
ImGuiWindowFlags_NoBringToFrontOnFocus |		// 焦点不置顶
ImGuiWindowFlags_NoScrollbar |				// 无滚动条
ImGuiWindowFlags_NoScrollWithMouse;			// 鼠标滚轮无效
static constexpr ImGuiWindowFlags dwChildWindowFlags =	// 窗口状态
ImGuiWindowFlags_NoTitleBar |				// 无标题栏
ImGuiWindowFlags_NoResize |					// 禁止调整大小
ImGuiWindowFlags_NoCollapse |				// 禁止折叠
ImGuiWindowFlags_NoBringToFrontOnFocus |		// 焦点不置顶
ImGuiWindowFlags_NoScrollbar |				// 无滚动条
ImGuiWindowFlags_NoScrollWithMouse;			// 鼠标滚轮无效

static CHAR ERRBUFFER[16] = u8"无效缓冲";   // 无效缓冲区

// ================================
// 区域: 上下文变量定义区
// 描述: 由于 DearImgui 是即时渲染的 UI，我们并不需要专门保存控件的状态信息，所有控件的状态信息都由 DearImgui 内部维护
// 　　  但是渲染时需要使用一些变量来定义控件的行为和样式，这些变量将在此区域定义，这些就是所谓的上下文变量
// ================================
static AutoSize::SizeInfoFLOAT sizeinfo;   // 全局尺寸信息对象，用于在渲染过程中获取当前窗口的尺寸信息
static DWORD dwDisableIME = 0;             // 非 IME 输入需求标志

// ================================
// 区域: 帧间变量定义区
// 描述: 偶尔需要在两帧之间传递的变量，这些变量将在此区域定义，这些就是所谓的帧间变量
// 　　  例如判断上下帧是否是同一布局，或者窗口是否发送变化等
// ================================
static vec2 lastWnd = {};          // 上一帧的尺寸大小

// ================================
// 区域: 内部部件前向声明区
// ================================
namespace Widgets
{
	// 函数: InputNumeric
	// 功能: 输入数字
	BOOL InputNumeric(LPCSTR label, int& val, int max);
	// 函数: ExitSign
	// 描述: 绘制关闭按钮
	BOOL ExitSign(ImVec2 pos, ImVec2 size, FLOAT fPadding, FLOAT thickness);
	// 函数: TimeSpan
	// 功能: 时段显示(HH:MM:SS.MS - HH:MM:SS.MS)
	BOOL TimeSpan(int id, int start, int end, ImVec2 pos, ImVec2 size);
	// 函数: RangeSlider
	// 功能: 范围滑动条
	int64_t RangeSlider(LPCSTR label, int id, ImVec2 pos, ImVec2 size, int64_t& vMin, int64_t& vMax, int64_t lMin, int64_t lMax);
	// 函数: ProgressStatic
	// 功能: 进度状态显示
	BOOL ProgressStatic(int id, ImVec2 pos, ImVec2 size, int64_t current, int64_t expect);

}

// ================================
// 区域: 自定义控件前向声明区
// ================================

namespace Controls
{
	// 函数: TitleBar
	// 功能: 标题控件
	VOID TitleBar(CtrlInfo* info);
	// 函数: InputReal
	// 功能: 实数输入框
	VOID InputReal(CtrlInfo* info);
	// 函数: TimeSpanList
	// 功能: 时段列表
	BOOL TimeSpanList(CtrlInfo* info);
	// 函数: ImageRender
	// 功能: 图像渲染器
	BOOL ImageRender(CtrlInfo* info);
	// 函数: TimeBar
	// 功能: 时间条
	BOOL TimeBar(CtrlInfo* info);
	// 函数: RangeSlider
	// 功能: 范围滑动条
	BOOL RangeSlider(CtrlInfo* info);
	// 函数: ProgressStatic
	// 功能: 进度状态显示
	BOOL ProgressStatic(CtrlInfo* info);
	// 函数: ComboBox
	// 功能: 下拉框
	BOOL ComboBox(CtrlInfo* info);
}

// ================================
// 区域: 辅助函数定义区
// ================================

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
// 函数: IsMouseHoveringCircle
// 功能: 检测鼠标是否悬停在以坐标和半径限定的圆形位置
BOOL IsMouseHoveringCircle(CONST ImVec2& center, FLOAT radius)
{
	ImVec2 mouse = ImGui::GetIO().MousePos;
	FLOAT dx = mouse.x - center.x;
	FLOAT dy = mouse.y - center.y;
	return (dx * dx + dy * dy) <= (radius * radius);
}

// ================================
// 区域: 接口函数实现区
// ================================

// 函数: Render
// 描述: 渲染过程
// 参数:
//		IN	layout:			布局对象指针
//		IN	refsize:			参考尺寸对象引用
// 返回:
//		BOOL: 返回 TRUE 表示渲染成功，FALSE 表示渲染失败
BOOL Controls::RenderMain(Layout& layout, CONST AutoSize& refsize)
{
	// 变量声明
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	// 特殊情况
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	// 初始化
	num = layout.GetTotalNum();
	lastWnd = layout.GetWindow()->size;
	sizeinfo = refsize.GetSizeInfo<FLOAT>();
	dwDisableIME = 0;
	// 设置窗口
	ctrl = layout.GetWindow();
	ImGui::SetNextWindowPos(toImvec2(ctrl->position));
	ImGui::SetNextWindowSize(toImvec2(ctrl->size));
	ImGui::Begin(ctrl->label, NULL, dwMainWindowFlags);
	// 设置标题
	{
		ctrl = layout.GetTitle();
		if (ctrl)
		{
			TitleBar(ctrl);
		}
	}
	// 内容容器区域
	ctrl = layout.GetContainer();
	ImGui::SetCursorPosY(ctrl->position.y);
	ImGui::BeginChild(ctrl->label, toImvec2(ctrl->size), true, 0);
	// 遍历并渲染每个控件
	for (size_t i = layout.GetShellNum(); i < num; i++)
	{
		ctrl = layout.GetCtrlInfo(i);
		if (ctrl)
		{
			// 获取引用
			vec2& pos = ctrl->position;
			vec2& size = ctrl->size;
			// 压入 ID
			ImGui::PushID(ctrl->id);
			// 根据控件类型渲染
			switch (ctrl->type)
			{
			case CTRL_TYPE_BUTTON:
			{
				// 设置按钮
				ImGui::SetCursorPos(toImvec2(ctrl->position));
				LPCSTR text = (LPCSTR)ctrl->bufferInfo.buffer;
				if (ImGui::Button(text, toImvec2(ctrl->size)))
				{
					// 按钮点击事件处理
					if (ctrl->pEventHandler)
					{
						ctrl->pEventHandler();
					}
				}
				break;
			}
			case CTRL_TYPE_TEXT:
			{
				// 设置文本
				ImGui::SetCursorPos(toImvec2(ctrl->position));
				LPCSTR text = (LPCSTR)ctrl->bufferInfo.buffer;
				ImGui::Text(text);
				break;
			}
			case CTRL_TYPE_INPUT_SINGLE:
			{
				// 设置单行输入框
										// 设置位置
				ImGui::SetCursorPos(toImvec2(pos));
				ImGui::SetNextItemWidth(ctrl->size.x);
				// 获取缓冲区信息
				BufferInfo& bufInfo = ctrl->bufferInfo;
				// 检查缓冲区有效性
				if (bufInfo.buffer && bufInfo.size > 0)
				{
					ImGui::InputText(ctrl->label,
						(char*)bufInfo.buffer, bufInfo.size,
						ImGuiInputFlags_None);
				}
				else
				{
					ImGui::InputText(ctrl->label,
						ERRBUFFER, 16,
						ImGuiInputTextFlags_ReadOnly);
				}
				break;
			}
			case CTRL_TYPE_INPUT_REAL:
			{
				// 设置实数输入框
				InputReal(ctrl);
				break;
			}
			case CTRL_TYPE_LIST:
			{
				// 设置列表（此处使用 TimeSpanList）
				TimeSpanList(ctrl);
				break;
			}
			case CTRL_TYPE_RENDER:
			{
				// 设置渲染器
				ImageRender(ctrl);
				break;
			}
			case CTRL_TYPE_TIMEBAR:
			{
				// 设置时间条
				TimeBar(ctrl);
				break;
			}
			case CTRL_TYPE_SLIDER:
			{
				// 设置范围滑动条
				RangeSlider(ctrl);
				break;
			}
			case CTRL_TYPE_PROG:
			{
				// 设置进度状态显示
				ProgressStatic(ctrl);
				break;
			}
			case CTRL_TYPE_COMBO:
			{
				// 设置下拉框
				ComboBox(ctrl);
				break;
			}
			}
			// 弹出 ID
			ImGui::PopID();
		}
	}

	ImGui::EndChild(); // 结束内容区域子窗口
	ImGui::End(); // 结束主容器窗口
	bRet = TRUE;
RENDER_END:
	return bRet;
}
// 函数: RenferChild
// 描述: 渲染子窗口布局
BOOL Controls::RenderChild(Layout& layout)
{
	// 变量声明
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	// 特殊情况
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	// 初始化
	num = layout.GetTotalNum();
	// 设置窗口
	ctrl = layout.GetWindow();
	ImGui::SetNextWindowPos(toImvec2(ctrl->position));
	ImGui::SetNextWindowSize(toImvec2(ctrl->size));
	ImGui::Begin(ctrl->label, NULL, dwMainWindowFlags);
	// 设置标题
	{
		ctrl = layout.GetTitle();
		if (ctrl)
		{
			// TitleBar(ctrl);
			// 注: 后期测试决定新增 ChildTitle 控件还是移除无标题栏 Flag
		}
	}
	// 内容容器区域
	ctrl = layout.GetContainer();
	ImGui::SetCursorPosY(ctrl->position.y);
	ImGui::BeginChild(ctrl->label, toImvec2(ctrl->size), true, 0);
	// 遍历并渲染每个控件
	for (size_t i = layout.GetShellNum(); i < num; i++)
	{
		ctrl = layout.GetCtrlInfo(i);
		if (ctrl)
		{
			// 获取引用
			vec2& pos = ctrl->position;

			// 根据控件类型渲染
			switch (ctrl->type)
			{

			}
		}
	}
	ImGui::EndChild(); // 结束内容区域子窗口
	ImGui::End(); // 结束主容器窗口
RENDER_END:
	return bRet;
}
// 函数: IsIMEDisabled
// 描述: 检查当前是否需要禁用 IME 输入法
BOOL Controls::IsIMEDisabled()
{
	return dwDisableIME != 0;
}
// 函数: CheckWindowResize
// 描述: 检测布局变化是否需要调整窗口尺寸，若需要则返回建议尺寸
// 参数:
//      IN  layout: 布局对象指针
//      OUT size:   若返回 TRUE，则此处填充建议的窗口尺寸
// 返回:
//      TRUE:  当前布局需要调整窗口大小
//      FALSE: 当前布局与窗口尺寸匹配，无需调整
BOOL Controls::CheckWindowResize(Layout& layout, vec2* size)
{
	vec2 wnd = {};
	BOOL bRet = FALSE;
	if (!layout.IsInvalid())
	{
		wnd = layout.GetWindow()->size;
	}
	if (lastWnd != wnd)
	{
		if (size)
		{
			*size = wnd;
		}
		bRet = TRUE;
	}
	return bRet;
}

// ================================
// 区域: 内部部件实现区
// ================================

// 函数: InputNumeric
// 功能: 输入数字
BOOL Widgets::InputNumeric(LPCSTR label, int& val, int max)
{
	// 返回值
	BOOL bRet = FALSE;
	if (max > 0)
	{
		size_t size = log10(max) + 1;
		char buf[16]{};
		char fmt[] = "%02d";
		fmt[2] = '0' + size;
		snprintf(buf, sizeof(buf), fmt, val);
		if (ImGui::InputText(label, buf, size + 1, ImGuiInputTextFlags_CharsDecimal))
		{
			int in = atoi(buf);
			if (in > max)
			{
				in = max;
			}
			if (in < 0)
			{
				in = 0;
			}
			val = in;
			bRet = TRUE;
		}
		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0)
		{
			val += (ImGui::GetIO().MouseWheel > 0) ? 1 : -1;
			val = (val < 0) ? max : (val > max) ? 0 : val;
			bRet = TRUE;
		}
		if (ImGui::IsItemActive())
		{
			dwDisableIME += 1;
		}
	}
	return bRet;
}

// 函数: ExitSign
// 描述: 绘制关闭按钮
BOOL Widgets::ExitSign(ImVec2 pos, ImVec2 size, FLOAT fPadding, FLOAT thickness)
{
	// 获取当前窗口的绘制列表
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// 计算绘制大小
	FLOAT fSize = min(size.x, size.y) - fPadding * 4;								// 符号大小
	ImVec2 sign = { pos.x + (size.x - fSize) / 2,pos.y + (size.y - fSize) / 2 };		// 符号位置
	ImVec2 end = { sign.x + fSize,sign.y + fSize };									// 终止位置
	ImVec2 btn_end = { pos.x + size.x,pos.y + size.y };
	// 创建按钮区域
	ImGui::SetCursorScreenPos(pos);
	ImGui::InvisibleButton("##EXITBUTTON", size);

	// 检测按钮交互状态
	BOOL IsHovered = ImGui::IsItemHovered();
	BOOL IsClicked = ImGui::IsItemClicked();
	// 表面颜色
	ImU32 crFore = ImGui::GetColorU32(ImGuiCol_Text);
	// 绘制悬停背景
	if (IsHovered)
	{
		draw_list->AddRectFilled(pos, btn_end, ImGui::GetColorU32(ImGuiCol_ButtonHovered), 0);
	}
	// 绘制符号
	draw_list->AddLine(ImVec2(sign.x, sign.y), ImVec2(end.x, end.y), crFore, thickness);
	draw_list->AddLine(ImVec2(sign.x, end.y), ImVec2(end.x, sign.y), crFore, thickness);
	// 返回是否被点击
	return IsClicked;
}
// 函数: TimeSpan
// 功能: 时段显示(HH:MM:SS.MS - HH:MM:SS.MS)
BOOL Widgets::TimeSpan(int id, int start, int end, ImVec2 pos, ImVec2 size)
{
	FLOAT& padding = sizeinfo.padding;
	FLOAT& space = sizeinfo.space;
	FLOAT& ctrl = sizeinfo.controlHeight;
	BOOL bRet = FALSE;

	int ms1, ms2;
	int s1, s2;
	int m1, m2;
	int h1, h2;
	{
		ms1 = start % 1000;
		start /= 1000;
		ms2 = end % 1000;
		end /= 1000;

		s1 = start % 60;
		start /= 60;
		s2 = end % 60;
		end /= 60;

		m1 = start % 60;
		start /= 60;
		m2 = end % 60;
		end /= 60;

		h1 = start % 100;
		h2 = end % 100;
	}
	// 压入 ID
	ImGui::PushID("##TIMESPAN");
	ImGui::PushID(id);
	// 绘制背景
	ImGui::SetCursorPos(pos);
	ImVec2 cur = ImGui::GetCursorScreenPos();
	ImDrawList* drawlist = ImGui::GetWindowDrawList();
	drawlist->AddRectFilled(cur, cur + size, ImGui::GetColorU32(ImGuiCol_FrameBgHovered), padding);
	// 绘制文本
	ImGui::SetCursorPos(pos + ImVec2{ space,padding });
	ImGui::Text("%02d:%02d:%02d.%03d - %02d:%02d:%02d.%03d", h1, m1, s1, ms1, h2, m2, s2, ms2);
	// 绘制按钮
	ImGui::SetCursorPos(pos + ImVec2{ size.x - ctrl,0 });
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ChildBg));
	bRet = ImGui::Button(u8"ー", { ctrl,ctrl });
	ImGui::PopStyleColor(1);
	// 弹出 ID
	ImGui::PopID();
	ImGui::PopID();
	// 返回结果
	return bRet;
}
// 函数: RangeSlider
// 功能: 范围滑动条
int64_t Widgets::RangeSlider(LPCSTR label, int id, ImVec2 pos, ImVec2 size, int64_t& vMin, int64_t& vMax, int64_t lMin, int64_t lMax)
{
	// 定义变量
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawlist = ImGui::GetWindowDrawList();
	ImGuiStyle& style = ImGui::GetStyle();
	CONST FLOAT r = size.y;
	INT ret = 0;
	static int actived = 0; // 激活标识( 0:无, 1:左柄, 2:右柄, 3:中间区域)
	FLOAT TrackWidth = size.x - 2 * size.y;  // 扣除两端手柄半径后的可用宽度
	// 转换坐标和尺寸
	int64_t nlimit = lMax - lMin;
	ImGui::SetCursorPos(pos);
	ImVec2 boot = ImGui::GetCursorScreenPos();
	ImVec2 end = boot + ImVec2{ size.x,size.y };
	// 计算圆形手柄（滑块）的圆心
	ImVec2 pMin = boot + ImVec2{ TrackWidth * (vMin - lMin) / nlimit + size.y,size.y / 2 };
	ImVec2 pMax = boot + ImVec2{ TrackWidth * (vMax - lMin) / nlimit + size.y,size.y / 2 };
	ImVec2 SelMin = { pMin.x, boot.y };
	ImVec2 SelMax = { pMax.x, end.y };
	// 修正左右圆心

	// 为控件生成唯一ID，并推入ID栈
	ImGui::PushID(label);
	ImGui::PushID(id);
	// 交互处理

	// 检测鼠标悬停
	BOOL IsHoveredSel = ImGui::IsMouseHoveringRect(SelMin, SelMax);
	bool IsHoveredMin = IsMouseHoveringCircle(pMin, r);
	bool IsHoveredMax = IsMouseHoveringCircle(pMax, r);
	// 鼠标按下：确定激活的手柄
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (IsHoveredMin) actived = 1;
		else if (IsHoveredMax) actived = 2;
		else if (IsHoveredSel) actived = 3; // 点击中间区域可能用于移动整个范围（可选实现）
	}

	// 鼠标释放：清除激活状态
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		actived = 0;
	}

	// 拖拽逻辑
	if (actived > 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		float deltaPixels = io.MouseDelta.x;
		float deltaValue = deltaPixels / TrackWidth * nlimit;

		if (actived == 1) { // 拖动左柄
			vMin += (int64_t)deltaValue;
			vMin = std::clamp(vMin, lMin, vMax); // 确保不超越右柄和边界
			ret = 0B10;
		}
		else if (actived == 2) { // 拖动右柄
			vMax += (int64_t)deltaValue;
			vMax = std::clamp(vMax, vMin, lMax); // 确保不超越左柄和边界
			ret = 0B11;
		}
		// 可在此添加activeGrip==3时拖动整个范围的逻辑
	}

	// 可视化渲染
	ImU32 trackColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
	ImU32 trackFilledColor = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
	ImU32 gripColor = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	ImU32 gripHoveredColor = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);

	// 1. 绘制背景轨道
	drawlist->AddRectFilled(boot, end, trackColor, style.FrameRounding);

	// 2. 绘制已选范围（轨道填充部分）
	drawlist->AddRectFilled(SelMin, SelMax, trackFilledColor, style.FrameRounding);

	// 绘制圆形手柄
	drawlist->AddCircleFilled(pMin, r, (IsHoveredMin || actived == 1) ? gripHoveredColor : gripColor);
	drawlist->AddCircleFilled(pMax, r, (IsHoveredMax || actived == 2) ? gripHoveredColor : gripColor);

	// 弹出ID栈
	ImGui::PopID();
	ImGui::PopID();
	return ret;
}
// 函数: ProgressStatic
// 功能: 进度状态显示
BOOL Widgets::ProgressStatic(int id, ImVec2 pos, ImVec2 size, int64_t current, int64_t expect)
{
	// 返回值
	BOOL bRet = FALSE;
	// 获取应用
	FLOAT& padding = sizeinfo.padding;
	FLOAT& space = sizeinfo.space;
	FLOAT& ctrl = sizeinfo.controlHeight;
	// 计算进度
	DOUBLE prog = current * 100.0 / (expect ? expect : 1);
	// 绘制背景
	ImGui::SetCursorPos(pos);
	ImVec2 cur = ImGui::GetCursorScreenPos();
	ImDrawList* drawlist = ImGui::GetWindowDrawList();
	drawlist->AddRectFilled(cur, cur + size, 0x2FFFFFFF, padding);
	// 绘制文本
	ImGui::SetCursorPos(pos + ImVec2{ padding,padding });
	ImGui::Text(u8"进度: %6.2f%%", prog);
	// 返回结果
	return bRet;
}

// ================================
// 区域: 自定义控件实现区
// ================================

// 函数: TitleBar
// 功能: 标题控件
VOID Controls::TitleBar(CtrlInfo* info)
{
	// 变量声明
	BOOL bRet = FALSE;
	TitleBuffer* buf = (TitleBuffer*)info->bufferInfo.buffer;
	LPCSTR label = buf->text;
	ImVec2 size = toImvec2(info->size);
	ImVec2 pos = toImvec2(info->position);
	FLOAT exitwidth = 1.5f;
	FLOAT padding = sizeinfo.padding;
	uint64_t& flag = buf->flag;
	//static POINT shift{};
	//static BOOL IsDragWnd = FALSE;
	// 使用 InvisibleButton 创建可交互区域
	ImGui::InvisibleButton(label, size - ImVec2{ size.y * exitwidth,0 });
	// 标识采集
	if (ImGui::IsItemHovered())   flag |= TITLEBAR_FLAG_HOVERED;
	if (ImGui::IsItemActive())    flag |= TITLEBAR_FLAG_ACTIVE;
	if (ImGui::IsMouseDown(0))    flag |= TITLEBAR_FLAG_MOUSEDOWN;
	if (ImGui::IsMouseDoubleClicked(0)) flag |= TITLEBAR_FLAG_DOUBLECLICK;
	// 绘制文本
	ImGui::SetCursorPos(pos);
	ImGui::Text(label);
	if (Widgets::ExitSign({ size.x - size.y * exitwidth,0 }, { size.y * exitwidth,size.y }, padding, 1.0f))
	{
		flag |= TITLEBAR_FLAG_EXITCLICKED;
	}
	if (info->pEventHandler && flag != TITLEBAR_FLAG_NONE)
	{
		info->pEventHandler();
	}
}
// 函数: InputReal
// 功能: 实数输入框
VOID Controls::InputReal(CtrlInfo* info)
{
	// 获取引用
	vec2& pos = info->position;
	// 设置位置
	ImGui::SetCursorPos(toImvec2(pos));
	ImGui::SetNextItemWidth(info->size.x);
	// 获取缓冲区信息
	BufferInfo& bufInfo = info->bufferInfo;
	// 数值控件
	bool IsFixed = ImGui::InputDouble(info->label, (double*)bufInfo.buffer, 0, 0, "%05.2f", ImGuiInputTextFlags_CharsDecimal);
	if (ImGui::IsItemActive())
	{
		dwDisableIME++;
	}
	if (IsFixed && info->pEventHandler)
	{
		info->pEventHandler();
	}
}
// 函数: TimeSpanList
// 功能: 时段列表
BOOL Controls::TimeSpanList(CtrlInfo* info)
{
	// 获取引用
	vec2& pos = info->position;
	vec2& size = info->size;
	LPCSTR label = info->label;
	FLOAT& padding = sizeinfo.padding;
	FLOAT& space = sizeinfo.space;
	FLOAT& ctrl = sizeinfo.controlHeight;
	// 准备数据
	int* data = (int*)info->bufferInfo.buffer;

	// 处理
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, padding);
	ImGui::SetCursorPos(toImvec2(pos));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_FrameBg));
	ImGui::BeginChild(label, toImvec2(size), true, ImGuiChildFlags_Borders);
	ImGui::PopStyleColor();
	// 计算最多行数
	FLOAT combo = ctrl + space;
	int rows = (size.y - space) / combo;
	ImVec2 sub = { size.x - space * 2,ctrl };
	// 是否存在操作
	BOOL IsPreDEL = FALSE;

	int used = data[0];
	int caps = data[2];
	int& top = data[1];
	int& oper = data[3];
	int max = used; rows < used ? rows : used;
	long long* spans = (long long*)(data + 4 + top);
	// 创建
	for (int i = 0; i < max; i++)
	{
		if (Widgets::TimeSpan(i, spans[i * 2], spans[i * 2 + 1], { space, combo * i + space }, sub))
		{
			oper = (i << 1) | 1;
			IsPreDEL = TRUE;
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();

	// 处理事件
	if (info->pEventHandler && (oper != 0 || IsPreDEL))
	{
		info->pEventHandler();
	}

	return TRUE;
}
// 函数: ImageRender
// 功能: 图像渲染器
BOOL Controls::ImageRender(CtrlInfo* info)
{
	// 按钮
	struct VIDEO
	{
		SIZE size;
		ID3D11ShaderResourceView* srv;
		VOID* vs;
		int64_t us;
		int64_t ms;
	};
	VIDEO& video = *(VIDEO*)info->bufferInfo.buffer;
	FLOAT& padding = sizeinfo.padding;
	ImVec2 pos = toImvec2(info->position);
	ImVec2 size = toImvec2(info->size);
	ImVec2 srcsize = { (float)video.size.cx,(float)video.size.cy };
	{

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, padding);
		ImGui::SetCursorPos(pos);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_FrameBg));
		ImGui::BeginChild(info->label, size, true, ImGuiChildFlags_Borders);
		ImGui::PopStyleColor();
		//
		ImTextureRef texture((ImTextureID)video.srv);

		// 获取控件的可用区域
		ImVec2 availableSize = ImGui::GetContentRegionAvail();

		// 计算保持宽高比的缩放尺寸
		float widthRatio = availableSize.x / srcsize.x;
		float heightRatio = availableSize.y / srcsize.y;
		float scale = (widthRatio < heightRatio) ? widthRatio : heightRatio;
		ImVec2 displaySize = ImVec2(
			srcsize.x * scale,
			srcsize.y * scale
		);
		// 计算居中位置
		ImVec2 posInChild = ImVec2(
			(availableSize.x - displaySize.x) * 0.5f,
			(availableSize.y - displaySize.y) * 0.5f
		);
		// 设置位置并显示
		ImGui::SetCursorPos(posInChild);
		ImGui::Image(texture, displaySize);
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}
	// 处理事件
	if (info->pEventHandler)
	{
		info->pEventHandler();
	}
	return 0;
}
// 函数: TimeBar
// 功能: 时间条
BOOL Controls::TimeBar(CtrlInfo* info)
{
	// 结构: Range
	// 描述: 一个范围左值和右值
	struct RANGE
	{
		int64_t left;
		int64_t right;
	};
	// 时间组合结构体
	struct TimeComponents
	{
		int hours;
		int minutes;
		int seconds;
		int milliseconds;
		// 构造函数
		// 将毫秒构造为时间组合
		TimeComponents(LONG ms)
		{
			milliseconds = ms % 1000;
			int total_seconds = ms / 1000;
			seconds = total_seconds % 60;
			int total_minutes = total_seconds / 60;
			minutes = total_minutes % 60;
			hours = total_minutes / 60;
			// 限制最大值为 99:59:59.999
			if (hours > 99)
			{
				hours = 99;
				minutes = 59;
				seconds = 59;
				milliseconds = 999;
			}
		}
		// 方法: ToMilliseconds
		// 功能: 将组合结构转化为毫秒
		LONG ToMilliseconds() const
		{
			return ((hours * 60 + minutes) * 60 + seconds) * 1000 + milliseconds;
		}
	};

	// 读取数据
	BufferInfo buf = info->bufferInfo;
	struct TimeBarInfo
	{
		LPCSTR hint;
		RANGE span;
	};
	TimeBarInfo& tbi = *(TimeBarInfo*)buf.buffer;
	// 获取引用
	int64_t& ms = tbi.span.left;
	int64_t& limit = tbi.span.right;
	FLOAT& ch = sizeinfo.controlHeight;
	FLOAT& space = sizeinfo.space;
	FLOAT& padding = sizeinfo.padding;
	FLOAT& fh = sizeinfo.fontSize;
	// 其他参数
	ImVec2 pos = toImvec2(info->position);
	ImVec2 size = toImvec2(info->size);
	LPCSTR label = tbi.hint;
	FLOAT fw = fh / 2;
	LONG old = ms;
	// 获取颜色
	ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);      // 背景色
	ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);       // 边框色
	ImVec4 labelColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
	labelColor.w = 0.9f;                                             // 保持原有透明度
	ImVec4 separatorColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

	// 将毫秒转换为时分秒毫秒
	TimeComponents time(ms);
	// 设置起始位置
	ImGui::SetCursorPos(pos);
	// 设置文本位置（因为控件内部内距偏移，所以文本纵向位置需要根据内距计算）
	ImVec2 text = pos;
	text.y += padding;
	// 输入框样式
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padding, padding));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, 0x0);
	// 绘制文本
	ImGui::SetCursorPosY(pos.y + padding);
	ImGui::TextColored(labelColor, "%s", label);
	// 绘制背景
	ImGui::SameLine(0, padding);
	pos.x = ImGui::GetCursorPosX();
	ImGui::SetCursorPos(pos);
	ImVec2 min = ImGui::GetCursorScreenPos();
	ImVec2 max = { min.x + fw * 9 + padding * 11,min.y + ch };
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(min, max, bgColor, padding);
	draw_list->AddRect(min, max, borderColor, padding);
	// 小时输入框
	ImGui::PushID("H");
	ImGui::PushItemWidth(fw * 2 + padding * 2);
	Widgets::InputNumeric("##HOUR", time.hours, 99);
	ImGui::PopID();
	// 分隔符 ":"
	pos.x += fw * 2 + padding * 2;
	text.x = pos.x;
	ImGui::SetCursorPos(text);
	ImGui::TextColored(separatorColor, ":");
	// 分钟输入框
	pos.x += padding;
	ImGui::SetCursorPos(pos);
	ImGui::PushID("M");
	ImGui::PushItemWidth(fw * 2 + padding * 2);
	Widgets::InputNumeric("##MINUTE", time.minutes, 59);
	ImGui::PopID();
	// 第二个分隔符 ":"
	pos.x += fw * 2 + padding * 2;
	text.x = pos.x;
	ImGui::SetCursorPos(text);
	ImGui::TextColored(separatorColor, ":");
	// 秒输入框
	pos.x += padding;
	ImGui::SetCursorPos(pos);
	ImGui::PushID("S");
	ImGui::PushItemWidth(fw * 2 + padding * 2);
	Widgets::InputNumeric("##SECONDS", time.seconds, 59);
	ImGui::PopID();
	// 点分隔符 "."
	pos.x += fw * 2 + padding * 2;
	text.x = pos.x;
	ImGui::SetCursorPos(text);
	ImGui::TextColored(separatorColor, ".");
	// 毫秒输入框
	pos.x += padding;
	ImGui::SetCursorPos(pos);
	ImGui::PushID("MS");
	ImGui::PushItemWidth(fw * 3 + padding * 2);
	Widgets::InputNumeric("##MILLISECONDS", time.milliseconds, 999);
	ImGui::PopID();
	// 恢复样式
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
	// 返回
	LONG input = time.ToMilliseconds();
	ms = limit && input > limit ? ms : input;
	old != ms;
	if (old != ms)
	{
		if (info->pEventHandler)
		{
			info->pEventHandler();
		}
	}
	return 0;
}
// 函数: RangeSlider
// 功能: 范围滑动条
BOOL Controls::RangeSlider(CtrlInfo* info)
{
	BufferInfo buf = info->bufferInfo;
	int64_t& min = *((int64_t*)buf.buffer);
	int64_t& max = *((int64_t*)buf.buffer + 1);
	int64_t& lMin = *((int64_t*)buf.buffer + 2);
	int64_t& lMax = *((int64_t*)buf.buffer + 3);
	int64_t& oper = *((int64_t*)buf.buffer + 4);
	int64_t ret = Widgets::RangeSlider(
		info->label, info->id,
		toImvec2(info->position), toImvec2(info->size),
		min, max,
		lMin, lMax
	);
	if (ret && info->pEventHandler)
	{
		oper = ret;
		info->pEventHandler();
	}
	return 0;
}
// 函数: ProgressStatic
// 功能: 进度状态显示
BOOL Controls::ProgressStatic(CtrlInfo* info)
{
	// 获取信息
	struct
	{
		int64_t current = 0;			// 当前处理
		int64_t expect = 0;			// 预期处理
		BOOL IsVisible = FALSE;		// 可视标记
	} &prog = *(decltype(&prog))info->bufferInfo.buffer;

	if (prog.IsVisible)
	{
		if (info->pEventHandler)
		{
			info->pEventHandler();
		}
		Widgets::ProgressStatic(info->id, toImvec2(info->position), toImvec2(info->size), prog.current, prog.expect);
	}
	return TRUE;
}
// 函数: ComboBox
// 功能: 下拉框
BOOL Controls::ComboBox(CtrlInfo* info)
{
	// 获取信息
	struct
	{
		INT count;
		INT index;
		LPCSTR* items;
	} &combo = *(decltype(&combo))info->bufferInfo.buffer;
	// 绘制 Combo
	ImGui::SetCursorPos(toImvec2(info->position));
	ImGui::SetNextItemWidth(info->size.x);
	// 直接使用 ImGui::Combo 绘制下拉框
	if (ImGui::Combo(info->label, &combo.index, combo.items, combo.count))
	{
		// 选项改变事件处理
		if (info->pEventHandler)
		{
			info->pEventHandler();
		}
	}
	return TRUE;
}
