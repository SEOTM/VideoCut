#pragma once
#include "framework/base/BasicDesign.h"
#include "framework/base/ControlSpec.h"
#include "framework/render/Render.h"

namespace Controls
{
	// 函数: UpdateContext
	// 描述: 更新绘制上下文(内容容器原点 / 窗口位置记录槽 / 参考尺寸 / 投影配色 / 圆角)
	BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj, CONST vec2* pWndPos, CONST vec2& orgContent);

	// 函数: HasActiveInput
	// 描述: 本帧是否存在"处于编辑中(ImGui 项 active)的输入控件"
	BOOL HasActiveInput();

	// 函数: DrawCtrl
	// 描述: 按控件类型分派绘制实现(逐控件绘制入口): 有效坐标换算内容容器本地坐标 + 定位矩形 + 查表分派;
	BOOL DrawCtrl(CtrlInfo* ctrl);

	// 函数: TitleBar
	// 描述: 标题栏控件(TITLE): 标题文本 + 右上关闭钮, 标题状态经操作码回传事件处理器
	VOID TitleBar(CtrlInfo* info);
	// 函数: Button
	// 描述: 按钮控件(BUTTON): 点击时调用控件自身事件处理器(操作码 = 公共码 OPER_COMMON_CLICKED)
	VOID Button(CtrlInfo* info);
	// 函数: Text
	// 描述: 静态文本控件(TEXT): 文本图元; 仅当绑定事件(pEventHandler != NULL)时在图元之后补一枚
	VOID Text(CtrlInfo* info);
	// 函数: Rect
	// 描述: RECT 控件(纯矩形绘制): 外观 = 控件声明矩形, 无文本; 仅当绑定事件(pEventHandler != NULL)时在
	VOID Rect(CtrlInfo* info);
	// 函数: InputSingle
	// 描述: 单行输入控件(INPUT_SINGLE): 失焦/回车提交时调用控件自身事件处理器
	VOID InputSingle(CtrlInfo* info);
	// 函数: InputMulti
	// 描述: 多行文本输入控件(INPUT_MULTI): 长文本(提示词 / 负面提示词 / 参数文件内容)
	VOID InputMulti(CtrlInfo* info);
	// 函数: InputReal
	// 描述: 实数输入控件(INPUT_REAL): 缓冲为 DOUBLE; 提交操作码 = 专属段 OPER_EDIT_SUBMIT
	VOID InputReal(CtrlInfo* info);
	// 函数: InputNumeric
	// 描述: 整数输入控件(INPUT_NUMERIC): 缓冲为 INT; 提交操作码 = 专属段 OPER_EDIT_SUBMIT
	VOID InputNumeric(CtrlInfo* info);
	// 函数: InputAction
	// 描述: 输入动作控件(INPUT_ACTION): 底部矩形 + 无边框单行输入 + 右侧按钮; 文本取控件缓冲
	VOID InputAction(CtrlInfo* info);
	// 函数: Slider
	// 描述: 单手柄滑动条控件(SLIDER): 一个值 + 一个值域; 值变更操作码 = 专属段 OPER_EDIT_CHANGED
	VOID Slider(CtrlInfo* info);
	// 函数: Progress
	// 描述: 进度状态显示控件(PROG); 操作码 = 专属段 OPER_PROG_UPDATED(电平型: 可见期间每帧报一次)
	VOID Progress(CtrlInfo* info);
	// 函数: RenderSurface
	// 描述: 渲染面控件(RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	VOID RenderSurface(CtrlInfo* info);
	// 函数: CheckBox
	// 描述: 复选框控件(CHECKBOX): 方框 / 勾 / 文字由 ImGui 原生复选框绘制(文字取控件缓冲)
	VOID CheckBox(CtrlInfo* info);
	// 函数: Combo
	// 描述: 下拉框控件(COMBO); 值/选项变更操作码 = 专属段 OPER_EDIT_CHANGED
	VOID Combo(CtrlInfo* info);
	// 函数: ComboAction
	// 描述: 下拉动作控件(COMBO_ACTION): 无边框下拉框 + 右侧按钮; 选项表与选中索引取控件缓冲
	VOID ComboAction(CtrlInfo* info);
	// 函数: TimeBar
	// 描述: 可编辑时间条控件(TIMEBAR): 提示文本 + 时:分:秒.毫秒四段数字输入; 数据取控件缓冲
	VOID TimeBar(CtrlInfo* info);
	// 函数: ListRowDropOper
	// 描述: 列表项拖拽落点 → 列表项部件返回约定(纯换算, 不触碰 ImGui; 绘制侧唯一真源)
	int64_t ListRowDropOper(DWORD dwSrc, DWORD dwRow, BOOL IsAfter);

	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口 —— 外部一次性送入(单参数), 控件自行拆解任务:
	VOID ListInput(CtrlInfo* info);
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口 —— 同上(外部一次性送入 + 控件自行拆解), 按该类型
	VOID ListCombo(CtrlInfo* info);

	// 函数: GetCtrlApi
	// 描述: 取控件库的绘制接口表(进程级静态表; 重复调用幂等)
	CONST CtrlApi* GetCtrlApi();
}
