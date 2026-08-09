// ================================
// 文件: Application.h
// 说明: 应用程序主类头文件
// 　　  应用程序的主要逻辑和功能将在此类中实现
// 　　  main 函数只负责初始化 Application 对象以及负责渲染
// 　　  实际函数行为将由 Application 对象提供（以关联在控件组中的函数指针的方式进行实现）
// ================================

#pragma once
#include <minwindef.h>
#include "Layout.h"
#include "BasicDesign.h"
#include "LayoutSolver.h"

// ================================
// 辅助定义区
// ================================



// ================================
// 类名: Application
// 说明: 应用程序主类 (单例模式)
// ================================
class Application
{
private:
	// 彻底贯彻内存池设计，所有成员变量的内存都通过 SecureBuffer 来分配和管理
	// 特此创建一个 Struct 来保存所有成员变量，方便管理和访问
	// 但是 Struct 的创建和销毁仍然需要通过 SecureBuffer 来完成，以确保内存安全

	// 结构: AppData
	// 描述: 存储应用程序成员变量的结构体
	struct AppData
	{
		AutoSize refsize;		// 用于参考尺寸匹配的 AutoSize 对象	| 144 字节
		RECT rect;				// 默认窗体矩形						| 16 字节
		LayoutSolver* solver;	// 布局求解器对象指针				| 8 字节
		Layout* layout;			// 布局对象指针						| 8 字节
		HWND hWnd;				// 窗体句柄							| 8 字节
		Layout* pre;				// 准备界面							| 8 字节
		Layout* editor;			// 编辑器界面						| 8 字节
		LPCSTR lpOperfolder;	// 操作文件夹						| 8 字节
		BOOL IsMaximizedWnd;	// 是否为最大窗口					| 4 字节

	};

	// 成员变量
	static SecureBuffer* MemPool;	// 用于分配应用程序所需的安全缓冲区（本应用使用该buffer，但是不适用安全管理功能）
	// 该类虽然是为安全范式设计的，但是作为应用程序的主要内存分配器使用也是可以的
	static AppData* data;			// 存储应用程序成员变量的结构体指针
	// 服务于单例模式，所以构造函数和析构函数为私有

	// 函数: 构造函数
	// 描述: 私有化构造函数，禁止外部直接创建对象
	Application() = delete;
	// 函数: 析构函数
	// 描述: 私有化析构函数，禁止外部直接销毁对象
	~Application() = delete;

	// 由于引入了 AutoSize 类作为参考尺寸，因此支持了自动布局
	// 为了方便更改 size，我们新增一个方法用于应用参考尺寸（原本 Init 工厂也将调用此方法）

	// 函数: ApplyRefSize
	// 描述: 应用参考尺寸到布局对象
	static BOOL ApplyRefSize();
public:

	// 函数: Init
	// 描述: 初始化应用程序逻辑
	static BOOL Init();
	// 函数: Render
	// 描述: 渲染应用程序界面
	static BOOL Render();
	// 函数: GetLayout
	// 描述: 获取布局对象引用
	static Layout& GetLayout();
	// 函数: Release
	// 描述: 释放应用程序单例对象
	static VOID Release();
	// 函数: GetRefsize()
	// 描述: 获取参考尺寸对象
	static CONST AutoSize& GetRefsize();


	// 从此处开始，逐步变更函数为静态函数，对象使用 instance 指针

	// 函数: SetWindowHandle
	// 功能: 设置窗体句柄
	static VOID SetWindowHandle(HWND hSet);
	// 函数: GetWindowHandle
	// 功能: 设置窗体句柄
	static HWND GetWindowHandle();
	// 函数: GetCurrentFolder
	// 功能: 获取当前操作目录
	static LPCSTR GetCurrentFolder();
	// 函数: ResetStatic
	// 功能: 重设应用状态
	static VOID ResetStatic();
	// 函数: SetWindowSize
	// 功能: 设置窗体大小
	static VOID SetWindowSize(SIZE size);
	// 函数: SwitchMaximized
	// 功能: 切换窗体最大化状态
	static VOID SwitchMaximized();
	// 函数: IsMaximized
	// 功能: 当前是否为最大化窗体
	static BOOL IsMaximized();
	// 函数: FindControl
	// 功能: 根据控件所属的 Index 和 Label 获取该控件
	static CONST CtrlInfo* FindControl(UINT index, LPCSTR label);
	// 函数: FindControlIndex
	// 功能: 根据控件所属的 Index 和 Label 获取该控件的 ID（目的是为了不受 CONST 限制的操作）
	static UINT FindControlIndex(UINT index, LPCSTR label);
	// 函数: SwitchLayout
	// 功能: 切换布局
	static VOID SwitchLayout(LPCSTR name);

	// 函数: SetCurrentFolder
	// 功能: 设置当前操作目录
	static VOID SetCurrentFolder(LPCSTR path);
};

