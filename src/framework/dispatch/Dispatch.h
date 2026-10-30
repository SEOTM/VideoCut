#pragma once
#include <windows.h>
#include <stddef.h>

#include "framework/base/BasicDesign.h"
#include "framework/dispatch/UIEventHandler.h"
#include "framework/layout/Layout.h"
#include "framework/layout/LayoutModel.h"
#include "framework/render/Render.h"

class LayoutSolver;

struct DispatchConfig
{
	LPCSTR szDocName;
	LPCSTR szDocEnv;
	LPCSTR szAutoCloseEnv;
	LPCSTR szLogPrefix;
	DWORD dwWndCount;
	BOOL bDarkFollowSystem;
};

enum LayoutSourceKind
{
	LAYOUT_SOURCE_DEFAULT = 0,
	LAYOUT_SOURCE_MEMORY,
	LAYOUT_SOURCE_MODEL,
	LAYOUT_SOURCE_LAYOUTS,
	LAYOUT_SOURCE_DEFERRED
};

struct LayoutSource
{
	LayoutSourceKind kind;
	LPCVOID pMemData;
	DWORD cbMemSize;
	CONST FileMidState* pModel;
	Layout* CONST* ppLayouts;
	CONST AutoSize* pRefs;
	DWORD dwLayoutCount;

	// 函数: CreateDefault
	// 描述: 构造"缺省加载链"来源(不传任何内容时的形态)
	static LayoutSource CreateDefault()
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_DEFAULT;
		return src;
	}

	// 函数: CreateMemory
	// 描述: 构造"内存缓冲"来源(JSON 或 bin 字节; 解析失败回退缺省加载链)
	static LayoutSource CreateMemory(LPCVOID pData, DWORD cbSize)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_MEMORY;
		src.pMemData = pData;
		src.cbMemSize = cbSize;
		return src;
	}

	// 函数: CreateModel
	// 描述: 构造"布局模型"来源(整份中间态; 直接展开)
	static LayoutSource CreateModel(CONST FileMidState* pModel)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_MODEL;
		src.pModel = pModel;
		return src;
	}

	// 函数: CreateLayouts
	// 描述: 构造"布局对象数组"来源(布局数组 + 参考尺寸数组; 直接使用)
	static LayoutSource CreateLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_LAYOUTS;
		src.ppLayouts = ppLayouts;
		src.pRefs = pRefs;
		src.dwLayoutCount = dwCount;
		return src;
	}

	// 函数: CreateDeferred
	// 描述: 构造"延迟装配"来源(初始化不提交真实布局; 运行期由业务代码逐枚提交)
	static LayoutSource CreateDeferred()
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_DEFERRED;
		return src;
	}
};

class Dispatch
{
private:
	struct LayoutSlot
	{
		Layout* pLayout;
		AutoSize ref;
		BOOL bVisible;
		BOOL bNested;
		BOOL bOwned;
	};

	struct DispatchData
	{
		LayoutSlot* pSlots;
		LayoutSolver* pSolver;
		ProjCR* pProjCR;
		CONST HostApi* pHost;
		DWORD dwSlotCount;
		DWORD dwSlotCapacity;
		RefSize refSize;
		LONG nWndX;
		LONG nWndY;
		DWORD dwWndW;
		DWORD dwWndH;
		DWORD dwWndCount;
		BOOL bReady;
		BOOL bExit;
		BOOL bDarkFollowSystem;
		BOOL bDarkMode;
		DWORD dwFrameCount;
		DWORD dwAutoCloseMs;
		ULONGLONG qwStartTick;
		DWORD dwLastShowState;
		DWORD dwLastVisible;
		CHAR szDocName[MAX_PATH];
		CHAR szDocEnv[64];
		CHAR szAutoCloseEnv[64];
	};

	static DispatchData* data;

	Dispatch() = delete;
	~Dispatch() = delete;
	Dispatch(const Dispatch&) = delete;
	Dispatch& operator=(const Dispatch&) = delete;

	// 函数: LoadScene
	// 描述: 建立会话分配器与成员块, 并按布局来源装配场景(解析 / 展开 / 几何解算 / 位置定稿 / 自证)
	static BOOL LoadScene(CONST DispatchConfig& cfg, CONST LayoutSource& source);

	// 函数: UnloadScene
	// 描述: 释放场景资源(布局槽与布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
	static VOID UnloadScene();

	// 函数: ResetSlots
	// 描述: 清空布局槽(归调度层所有的布局对象先回收控件缓冲再析构; 槽数归零)
	static BOOL ResetSlots();

	// 函数: ReserveSlots
	// 描述: 保证槽数组至少容纳指定数量(不足时按步进扩容; 旧块随会话分配器整体释放)
	static BOOL ReserveSlots(DWORD dwCapacity);

	// 函数: AppendSlot
	// 描述: 在槽尾追加一枚布局(容量不足时先扩容)
	static BOOL AppendSlot(Layout* pLayout, CONST AutoSize& ref, BOOL bOwned, BOOL bVisible, BOOL bNested);

	// 函数: UseLayouts
	// 描述: 整体替换布局槽(先清空), 逐枚接管调用方给出的布局对象(不取得所有权)
	static BOOL UseLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount, BOOL bFirstOnly);

	// 函数: BuildFromModel
	// 描述: 从布局模型逐枚展开布局对象并解算几何(前 dwWant 枚; 产物归调度层所有)
	static BOOL BuildFromModel(CONST FileMidState& state, DWORD dwWant);

	// 函数: ApplyWndGeometry
	// 描述: 按布局参考参数推导首枚窗口几何与该布局的参考尺寸(AutoSize)
	static BOOL ApplyWndGeometry(CONST RefSize& refSize, AutoSize& refOut, DWORD* pdwWndW, DWORD* pdwWndH);

	// 函数: CountTotalCtrls
	// 描述: 统计全部布局槽的控件数(自证用)
	static DWORD CountTotalCtrls();

	// 函数: CountBoundCtrls
	// 描述: 统计全部布局槽内已绑定事件处理器的控件数(自证用)
	static DWORD CountBoundCtrls();

	// 函数: EnsureProjCR
	// 描述: 确保投影配色已建立并跟随当前亮暗口径(跟随系统或 SetDarkMode 登记值; 反复调用幂等)
	static VOID EnsureProjCR();

	// 函数: ApplyHostStyle
	// 描述: 按参考尺寸与投影配色填宿主样式描述(圆角与 17 个配色槽); 配色未就绪时返回 FALSE
	static BOOL ApplyHostStyle(StyleDesc& desc);

	// 函数: RefreshWndShowCache
	// 描述: 刷新最近一帧的窗口显示状态缓存(收尾自证在窗口销毁后仍可打印当时的可见性)
	static VOID RefreshWndShowCache();

	// 函数: PrintRenderProof
	// 描述: 渲染自证 —— 打印绘制量、窗口尺寸调整次数、控件数与窗口显示状态
	static VOID PrintRenderProof(LPCSTR lpszTag);

	// 函数: PrintWndGeometry
	// 描述: 打印首帧前的建窗几何(Start 默认几何的实测值; 首帧渲染前窗口尚未按文档几何结算)
	static VOID PrintWndGeometry();

	// 函数: Log
	// 描述: 日志输出 —— 统一前缀 [<配置前缀>] 并按行刷新
	static VOID Log(LPCSTR lpszFormat, ...);

public:
	using BindingsFunc = CONST UIEventHandler::EventBinding* (*)(size_t* pnCount);

	static constexpr DWORD DISPATCH_SLOT_MAX = LAYOUT_MAX;

	// 函数: Init
	// 描述: 初始化调度层(控制台 UTF-8 → DPI 感知 → 事件绑定表注入 → 场景装配 → 宿主注入与样式登记 → 无人值守参数)
	static BOOL Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings,
		CONST LayoutSource& source = LayoutSource::CreateDefault());

	// 函数: AttachLayouts
	// 描述: 整体替换布局(清空现有槽后逐枚接管; 含运行期替换; 布局对象所有权归调用方)
	static BOOL AttachLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount);

	// 函数: AddLayout
	// 描述: 追加一枚布局(逐枚提交; 布局对象所有权归调用方; 新槽默认可见且按基座渲染)
	static BOOL AddLayout(Layout* pLayout, CONST AutoSize& ref);

	// 函数: SetLayoutVisible
	// 描述: 设置指定槽的可见性(不可见的布局不参与本帧渲染)
	static BOOL SetLayoutVisible(DWORD index, BOOL bVisible);

	// 函数: SetLayoutNested
	// 描述: 设置指定槽的嵌套状态(TRUE = 按 Flex 渲染, 沿用前一枚基座布局的窗口段; FALSE = 基座渲染)
	static BOOL SetLayoutNested(DWORD index, BOOL bNested);

	// 函数: GetLayoutState
	// 描述: 取指定槽的可见性与嵌套状态(越界返回 FALSE)
	static BOOL GetLayoutState(DWORD index, BOOL* pbVisible, BOOL* pbNested);

	// 函数: GetLayoutCount
	// 描述: 取布局数量(延迟装配且未提交时返回 0)
	static DWORD GetLayoutCount();

	// 函数: GetLayoutAt
	// 描述: 取指定槽的布局对象(越界返回空对象)
	static Layout& GetLayoutAt(DWORD index);

	// 函数: GetRefAt
	// 描述: 取指定槽的参考尺寸(越界返回缺省参考量)
	static CONST AutoSize& GetRefAt(DWORD index);

	// 函数: SetDarkMode
	// 描述: 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)
	static VOID SetDarkMode(BOOL bDark);

	// 函数: Frame
	// 描述: 每帧处理流程(建窗几何自证 → 数据层更新 → 布局渲染清单 → 可见性缓存 → IME 覆盖 → 渲染自证 → 退出判据)
	static VOID Frame(VOID* pUser);

	// 函数: OnSegment
	// 描述: 段前回调范例挂载点(后端开帧之后、UI 库帧开始之前; 当前为空实现)
	static VOID OnSegment(DWORD index, VOID* pUser);

	// 函数: Release
	// 描述: 释放调度资源(投影配色 → 场景与成员块 → 会话分配器; 幂等)
	static VOID Release();

	// 函数: GetLayout
	// 描述: 取首枚布局对象(渲染源; 未就绪时返回空对象)
	static Layout& GetLayout();

	// 函数: GetRef
	// 描述: 取首枚参考尺寸(控件几何解算与绘制取值基准; 未就绪时返回缺省参考量)
	static CONST AutoSize& GetRef();

	// 函数: GetFontSize
	// 描述: 取建面字号(由首枚布局的参考字号推导; 无效时按缺省字号兜底)
	static FLOAT GetFontSize();

	// 函数: GetWndCount
	// 描述: 取本调度需要创建的标准窗口数量(未就绪时按单窗口兜底)
	static DWORD GetWndCount();

	// 函数: GetWndRect
	// 描述: 取主布局窗口矩形(桌面位置取自首枚布局的窗口件位置记录; 客户区尺寸取推导值)
	static BOOL GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);

	// 函数: GetExitAddress
	// 描述: 取退出判据地址(宿主主循环每帧只读; 未就绪时返回 NULL)
	static CONST BOOL* GetExitAddress();

	// 函数: IsReady
	// 描述: 调度层是否就绪(成员块已分配且至少一枚布局已装配)
	static BOOL IsReady();
};
