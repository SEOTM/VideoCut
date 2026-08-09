#pragma once
#include "SecureBuffer.h"
#include "FuncPtr.h"
#include <stdint.h>
// ================================
// 备注：额外说明
// ================================
// 在此应用中，不使用常见的 new 和 delete 来管理内存，而是使用自定义的 SecureBuffer 类来分配和释放内存。
// 包括创建 Layout 对象时的内存分配，控件信息数组的内存分配等，均通过 SecureBuffer 来完成。
// 如：
// secbuf 是一个 SecureBuffer 对象
// Layout* layout = new (secbuf.Allocate(sizeof(Layout))) Layout(num); // 分配内存并调用构造函数
// ......(中间的处理)
// secbuf.SetReadOnly(); // 设置为只读，防止后续修改
// ......(使用完毕后)
// layout->~Layout(); // 调用析构函数
// 另外，Layout 对象内部的控件信息数组也是通过 SecureBuffer 分配的内存来存储的。
// 这样可以确保所有敏感数据都存储在受保护的缓冲区中，提升安全性。

// ================================
// 描述: 预定义常量结构和枚举
// ================================

// 枚举：控件ID
// 描述：定义各个控件的唯一标识符
enum CtrlID
{
	// 无效ID
	ID_INVALID = 0,
	// 基础控件组
	ID_BASE_MAIN = 1,		// 主容器
	ID_BASE_TITLEBAR = 2,	// 标题栏
	ID_BASE_CONTAINER = 3,	// 容器
	ID_END_BASE,			// 基础控件结束 ID
	// 准备界面控件组
	ID_UI_PRE,				// 准备界面 ID 开始
	ID_TEXT_SRC,			// 源路径文本标签
	ID_INPUT_SRC,			// 源路径输入框
	ID_BUTTON_BROWSE,		// 浏览文件夹按钮
	ID_BUTTON_OPEN,			// 打开文件夹按钮
	// 选择界面控件组

	// 操作界面控件组
	ID_UI_OPER,				// 操作界面 ID 开始
	ID_TEXT_CLIP,			// 剪辑片段标签
	ID_TEXT_PREVIEW,		// 预览视频标签
	ID_BUTTON_ADDCLIP,		// 添加片段按钮
	ID_BUTTON_OPER,			// 执行操作按钮
	ID_SLIDER_TIME,			// 时间控制滑块
	ID_TIMEBAR_BEGIN,		// 开始时间组件
	ID_TIMEBAR_END,			// 终止时间组件
	ID_LIST_CLIP,			// 剪辑片段列表
	ID_RENDER_VIDEO,		// 视频呈现器
	ID_BUTTON_PLAY,			// 播放按钮
	ID_COMBO_SELECT,			// 视频选择组合框
	ID_PROG_REMIX,			// 重混进度
	ID_TEXT_SPEED,			// 变速因子标签
	ID_INPUT_REAL_SPEED,	// 变速因子输入框 
	ID_BUTTON_RESET,			// 重设目录按钮
};
// 枚举：控件类型
// 描述：定义各个控件的类型
enum CtrlType
{
	CTRL_TYPE_INVALID = 0,  // 无效类型
	CTRL_TYPE_WND = 1,		// 窗体类型
	CTRL_TYPE_TITLE,		// 标题栏类型
	CTRL_TYPE_CNTR,			// 容器类型
	CTRL_TYPE_CHILD,		// 子容器
	CTRL_TYPE_TEXT,			// 文本标签
	CTRL_TYPE_INPUT_SINGLE, // 单行输入框
	CTRL_TYPE_INPUT_MULTI,  // 多行输入框
	CTRL_TYPE_BUTTON,       // 按钮
	CTRL_TYPE_LIST,			// 列表
	CTRL_TYPE_TIMEBAR,		// 时间控件
	CTRL_TYPE_RENDER,		// 呈现器
	CTRL_TYPE_SLIDER,		// 滑动条
	CTRL_TYPE_COMBO,		// 组合框
	CTRL_TYPE_PROG,			// 进度显示
	CTRL_TYPE_INPUT_REAL,	// 实数输入框
};
// 结构: vec2
// 描述: 与 ImVec2 等价的二维坐标结构体
// 备注: 避免直接包含 ImGui 头文件，减少依赖性，并在实际使用时进行转换
// 增注: 原 ImVec2 定义如下：
/*
	ImVec2: 2D vector used to store positions, sizes etc. [Compile-time configurable type]
	 - This is a frequently used type in the API. Consider using IM_VEC2_CLASS_EXTRA to create implicit cast from/to our preferred type.
	 - Add '#define IMGUI_DEFINE_MATH_OPERATORS' before including this file (or in imconfig.h) to access courtesy maths operators for ImVec2 and ImVec4.
	IM_MSVC_RUNTIME_CHECKS_OFF
	struct ImVec2
	{
		float                                   x, y;
		constexpr ImVec2()                      : x(0.0f), y(0.0f) { }
		constexpr ImVec2(float _x, float _y)    : x(_x), y(_y) { }
		float& operator[] (size_t idx)          { IM_ASSERT(idx == 0 || idx == 1); return ((float*)(void*)(char*)this)[idx]; } // We very rarely use this [] operator, so the assert overhead is fine.
		float  operator[] (size_t idx) const    { IM_ASSERT(idx == 0 || idx == 1); return ((const float*)(const void*)(const char*)this)[idx]; }
	#ifdef IM_VEC2_CLASS_EXTRA
		IM_VEC2_CLASS_EXTRA     Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math types and ImVec2.
	#endif
	};
*/
struct vec2
{
	float x;
	float y;
	vec2() : x(0.0f), y(0.0f) {}
	vec2(float _x, float _y) : x(_x), y(_y) {}
	vec2 operator+() const { return *this; }
	vec2 operator-() const { return vec2(-x, -y); }
	vec2 operator+(const vec2& other) const { return vec2(x + other.x, y + other.y); }
	vec2 operator-(const vec2& other) const { return vec2(x - other.x, y - other.y); }
	vec2 operator*(const vec2& other) const { return vec2(x * other.x, y * other.y); }
	vec2 operator/(const vec2& other) const { return vec2(x / other.x, y / other.y); }
	BOOL operator!=(const vec2& other) const { return (x != other.x || y != other.y); }
	BOOL operator==(const vec2& other) const { return (x == other.x && y == other.y); }
};

// 结构: CtrlInfo
// 描述: 控件信息结构体
struct CtrlInfo
{
	// 属性包含下列成员
	// 基础部分:
	//     控件ID
	//     控件类型
	//	   控件标签
	//     控件位置
	//     控件大小
	// 可选部分:
	//     控件的缓冲区信息（仅对输入框有效）,指针为 nullptr 表示不需要缓冲区
	//     控件的事件处理函数指针（仅对按钮有效）
	//	   其他属性可以根据需要添加


	// 为优化内存对齐布局，成员变量中按照从大到小顺序排列
	BufferInfo bufferInfo;		// 控件的缓冲区信息（仅对输入框有效）
	pfnCtrlEvent pEventHandler;	// 控件的事件处理函数指针（仅对按钮有效）
	vec2 size;					// 控件大小
	vec2 position;				// 控件位置
	LPCSTR label;				// 控件标签
	CtrlType type;				// 控件类型
	int id;					// 控件ID

	// 字节数: 56

	// 函数: 构造函数
	// 描述: 初始化成员变量
	CtrlInfo()
		: id(ID_INVALID), type(CTRL_TYPE_INVALID), label(nullptr),
		position(0.0f, 0.0f), size(0.0f, 0.0f),
		bufferInfo{ nullptr, 0 },
		pEventHandler(nullptr)
	{}

	// 函数: GetTypeEnum
	// 描述: 获取控件类型枚举
	static CtrlType GetTypeEnum(LPCSTR type);
};

// ================================
// 类名: Layout
// 描述: 布局类
// ================================
class Layout
{
private:
	// 成员变量
	CtrlInfo* pCtrlInfos;		// 指向控件信息数组的指针
	size_t qwNumCtrl;			// 控件数量
	static SecureBuffer* secbuf; // 用于分配控件信息数组的安全缓冲区
	static Layout* NULLOBJ;		// 空对象
	static CtrlInfo NULLINFO;	// 空INFO
	// 函数: 构造函数
	// 描述: 初始化布局对象
	Layout(size_t num);
	// 函数: 析构函数
	// 描述: 释放布局对象
	~Layout();
	// 函数: NULL 对象构造函数
	// 描述: NULL
	Layout();
public:
	// 函数: CreateLayout
	// 描述: 静态工厂函数，创建布局对象
	static Layout* CreateLayout(size_t num);
	// 函数: GetNullObject
	// 描述: 获取空引用
	static Layout& GetNullObject();
	// 函数: DestroyLayout
	// 功能: 静态销毁函数，用于释放对象
	static VOID DestroyLayout(Layout* layout);
	// 函数: SetSecBuffer
	// 描述: 设置安全缓冲区对象
	static VOID SetSecBuffer(SecureBuffer* buffer);
	// 函数: SetCtrlInfo
	// 描述: 设置指定索引的控件信息
	BOOL SetCtrlInfo(size_t index, const CtrlInfo& info);
	// 函数: GetCtrlInfo
	// 描述: 获取指定索引的控件信息
	CtrlInfo* GetCtrlInfo(size_t index);
	// 函数: GetCtrlInfoRef
	// 描述: 获取指定索引的控件信息引用
	CtrlInfo& GetCtrlInfoRef(size_t index);
	// 函数: GetWindow
	// 描述: 获取窗体控件
	CtrlInfo* GetWindow();
	// 函数: GetTitle
	// 描述: 获取窗体控件
	CtrlInfo* GetTitle();
	// 函数: GetContainer
	// 函数: 获取客户区容器
	CtrlInfo* GetContainer();
	// 函数: GetShellNum
	// 描述: 获取非客户区控件数量
	size_t GetShellNum();
	// 函数: GetTotalNum
	// 描述: 获取控件数量
	size_t GetTotalNum() const;
	// 函数: IsInvalid
	// 描述: 是否为无效引用
	BOOL IsInvalid();
};

// ================================
// 区域: 特定类型控件结构体及常量定义区
// ================================

// 
constexpr uint64_t TITLEBAR_FLAG_NONE = 0;
constexpr uint64_t TITLEBAR_FLAG_HOVERED = 1 << 0;
constexpr uint64_t TITLEBAR_FLAG_ACTIVE = 1 << 1;
constexpr uint64_t TITLEBAR_FLAG_MOUSEDOWN = 1 << 2;
constexpr uint64_t TITLEBAR_FLAG_DOUBLECLICK = 1 << 3;
constexpr uint64_t TITLEBAR_FLAG_EXITCLICKED = 1 << 4;

// 结构: TitleBuffer
// 描述: 标题栏缓冲区使用约定
struct TitleBuffer
{
	LPCSTR text;			// 标题文本
	uint64_t flag;		// 状态标志
};