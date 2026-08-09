#pragma once
#include "Layout.h"
#include "BasicDesign.h"

// ================================
// 库名: LayoutSolver
// 描述: 布局求解器，用于根据布局信息计算控件的实际位置和尺寸，并提供布局调整和渲染的功能
// ================================
class LayoutSolver
{
public:
	// 结构: SizeFactor
	// 描述: 存储控件尺寸系数和固定偏移
	struct SizeFactor
	{
		AutoSize::SizeInfoFLOAT coeff;	// 控件使用各参数的系数 (相对于参考尺寸的比例)
		int offset;						// 固定偏移量 (用于微调控件位置)
	};

	// 结构: ControlSpec
	// 描述: 存储控件规格信息的结构体
	struct ControlSpec
	{
		int id;				// 控件ID
		LPCSTR type;		// 控件类型(注：虽然在 Layout.h 中被定义为枚举，但是 JSON 文件并不支持枚举类型，因此这里使用字符串表示)
		LPCSTR label;		// 控件标签
		LPCSTR text;			// 控件文本内容
		LPCSTR event;		// 控件事件
		SizeFactor x;		// 控件X轴位置系数信息
		SizeFactor y;		// 控件Y轴位置系数信息
		SizeFactor width;	// 控件宽度系数信息
		SizeFactor height;	// 控件高度系数信息
	};

	// 结构: LayoutInfo
	// 描述: 存储布局信息的结构体
	struct LayoutInfo
	{
		LPCSTR name;			// 布局名称
		LPCSTR label;			// 布局标签
		BOOL IsMaxiable;		// 是否最大化
		BOOL IsResizeable;		// 是否可调整大小
		SizeFactor width;		// 布局宽度系数信息
		SizeFactor height;		// 布局高度系数信息
		DWORD dwControls;		// 控件数量
		ControlSpec ctrls[];		// 控件规格信息数组
	};
private:
	// 成员变量
	DWORD dwLayouts;						// 布局数量
    LayoutInfo** layouts;				// 布局信息数组
	SecureBuffer* secbuf;				// 用于分配控件规格信息数组的安全缓冲区，以及存储文本内容及标签的缓冲区
public:
	// 函数: LayoutSolver
	// 描述: 构造函数，初始化成员变量
	LayoutSolver(SecureBuffer* secbuf);
	// 函数: ~LayoutSolver
	// 描述: 析构函数，释放控件规格信息数组的内存
	~LayoutSolver();
	// 函数: LoadLayouts
	// 描述: 从文件中加载布局信息
    BOOL LoadLayouts(LPCSTR filename);
	// 函数: LoadLayouts
	// 描述: 从指定内存区域加载布局信息
    BOOL LoadLayouts(LPVOID data, DWORD size);
	// 函数: GetLayoutsCount
	// 描述: 获取布局数量
	DWORD GetLayoutsCount();
	// 函数: GetLayout
	// 描述: 获取指定索引的布局信息
	CONST LayoutInfo* GetLayout(DWORD index);

	// 函数: CalcParams
	// 描述: 静态函数，用以根据布局信息和参考尺寸以及固定偏移计算值
	static FLOAT CalcParams(SizeFactor& factor, AutoSize::SizeInfo<FLOAT>& ref);
};


