#include <windows.h>
#include <shobjidl.h>
#include <iostream>
#include <d3d11.h>
#include <thread>
#include <atomic>
#include <new>
#include "Application.h"
#include "BasicDesign.h"
#include "D3D11Manager.h"
#include "LayoutSolver.h"
#include "UIEventHandler.h"
#include "resource.h"

// ================================
// 其他常量
// ================================
constexpr int FOLDER_BUFFER = MAX_PATH * 3;
constexpr int LIST_ITEMS = 126;
constexpr int PSTRING_BUFFER = sizeof(LPCSTR);
constexpr int DOUBLE_BUFFER = sizeof(DOUBLE);
constexpr int TITLEBAR_BUFFER = sizeof(TitleBuffer);


// ================================
// 类名: Application
// 说明: 应用程序主类 (单例模式)
// ================================

// 静态成员变量初始化
SecureBuffer* Application::MemPool = NULL;
Application::AppData* Application::data = NULL;

// AutoSize Application::refsize = AutoSize::CreateByRatio(400, 250, 30, 20, 10, 10, AutoSize::AUTO_SIZE_BY_SCREEN_WIDTH);


// 函数: ApplyRefSize
// 描述: 应用参考尺寸到布局对象
BOOL Application::ApplyRefSize()
{
	// 返回值
	BOOL ret = FALSE;
	// 获取参考尺寸信息
	auto [wScr, hScr, wWin, hWin, hCtrl, fSize, space, padding] = data->refsize.GetSizeInfo<FLOAT>();
	// 定义临时变量
	CtrlInfo* wnd = NULL;
	CtrlInfo* info = NULL;
	// 暂时硬编码
	Layout* layouts[2] = { data->pre,data->editor };
	for (int i = 0; i < 2; i++)
	{
		// CONS
		printf(u8"布局编号:%02d\n", i);

		CONST LayoutSolver::LayoutInfo* layout = data->solver->GetLayout(i);
		// 获取控件数量
		DWORD dwCtrls = layout->dwControls;
		// 遍历控件并设置尺寸
		for (DWORD j = 0; j < dwCtrls; j++)
		{
			info = layouts[i]->GetCtrlInfo(j);
			LayoutSolver::ControlSpec ctrl = layout->ctrls[j];
			AutoSize::SizeInfo<FLOAT> ref = data->refsize.GetSizeInfo<FLOAT>();
			info->position = {
				LayoutSolver::CalcParams(ctrl.x,ref),
				LayoutSolver::CalcParams(ctrl.y,ref)
			};
			info->size = {
				LayoutSolver::CalcParams(ctrl.width,ref),
				LayoutSolver::CalcParams(ctrl.height,ref)
			};
			// CONS
			printf("  %-16s:(%4.0f,%4.0f) [%4.0f*%4.0f]\n", info->label, info->position.x, info->position.y, info->size.x, info->size.y);
		}
	}

	ret = TRUE;
END:
	return ret;
}

// 函数: Init
// 描述: 初始化应用程序逻辑
// 备注: 此函数为静态函数，用于初始化应用程序的主要逻辑
//	     包括创建布局对象，设置控件信息，关联事件处理函数等
//	     返回 TRUE 表示初始化成功，FALSE 表示初始化失败
// 注意: 单例对象在此函数中创建
BOOL Application::Init()
{

	// 使用 goto 语句，为避免初始化错误应提前声明所有变量
	BOOL ret = FALSE;	// 返回状态
	CtrlInfo info{};	// 控件信息临时变量
	// 使用新的初始化逻辑
	MemPool = new SecureBuffer(1024 * 1024 * 8); // 初始化为 8MB 的安全缓冲区池
	if (!MemPool)
	{
		goto ERR_FAIL_BUFFER_ALLOC;
	}
	// 为 Struct AppData 分配内存
	data = (AppData*)MemPool->Allocate(sizeof(AppData)).buffer;
	if (!data)
	{
		goto ERR_FAIL_BUFFER_NOFREE;
	}
	else
	{
		// 初始化 AutoSize 对象 (经验参数)
		AutoSize::RatioInfo ri;
		ri.dwRatioWidth = 622;
		ri.dwRatioHeight = 432;
		ri.dwRatioControlHeight = 25;
		ri.dwRatioFontSize = 17;
		ri.dwRatioSpace = 10;
		ri.dwRatioPadding = 4;
		AutoSize refsize = AutoSize::CreateByRatio(ri, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
		AutoSize::SizeInfo<DWORD> si = refsize.GetSizeInfo<DWORD>();
		si.windowWidth = si.screenWidth * 0.8;
		si.windowHeight = si.screenHeight * 0.8;
		refsize.SetSize(si);
		// 设置 Layout 类的 secure buffer 引用，使其能够通过该缓冲区池来分配内存
		Layout::SetSecBuffer(MemPool);

		// 将 AutoSize 对象保存到 AppData 中
		data->refsize = refsize;
		data->rect = { 0,0, (LONG)refsize.GetWindowWidth<DWORD>(), (LONG)refsize.GetWindowHeight<DWORD>() };
		data->solver = NULL;
		data->layout = NULL;
		data->hWnd = NULL;
		data->pre = NULL;
		data->editor = NULL;
		data->lpOperfolder = NULL;
		data->IsMaximizedWnd = FALSE;
	}

	// 创建布局对象前获取窗口尺寸信息(共同使用 BasicDesign 提供的常量)

	// 创建布局解析器并加载文件
	// 1. 从 MemPool 中分配一个可用大小的内存
	{
		BufferInfo buf = MemPool->Allocate(sizeof(LayoutSolver));
		if (buf.buffer == nullptr || buf.size < sizeof(LayoutSolver))
		{
			goto ERR_FAIL_BUFFER_NOFREE;
		}
		// 2. 使用定位 new 在分配的内存上创建 LayoutSolver 对象
		// 注意：这里使用的是定位 new，而不是普通的 new
		data->solver = new (buf.buffer) LayoutSolver(MemPool);
	}
	if (!data->solver->LoadLayouts(u8"LayoutConfig.json"))
	{
		// 从文件路径打开失败，则尝试从 RC 中打开内置的资源
		
		// 1. 获取当前模块的句柄（如果是DLL，替换为DLL的句柄）
		HINSTANCE hInstance = GetModuleHandle(NULL);

		// 2. 查找资源 (类型为 "JSON"，ID为 resourceId)
		HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(IDR_JSON1), L"JSON");
		if (!hRes) 
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}

		// 3. 加载资源到内存（加载到全局句柄）
		HGLOBAL hGlobal = LoadResource(hInstance, hRes);
		if (!hGlobal) 
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}

		// 4. 锁定资源，获取指向内存数据的指针
		LPVOID pData = LockResource(hGlobal);
		if (!pData) 
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}

		// 5. 获取资源的大小（字节数）
		DWORD dwSize = SizeofResource(hInstance, hRes);
		if (dwSize == 0) 
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}

		// 使用第二种加载方式
		if (!(data->solver->LoadLayouts(pData, dwSize)))
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}
	}
	
	// 创建布局对象
	{
		// 从解析器中获取布局数量和控件数量
		DWORD dwLayouts = data->solver->GetLayoutsCount();
		if (dwLayouts == 0)
		{
			goto ERR_FAIL_LOAD_LAYOUTS;
		}
		// 控件数量
		// DWORD dwControls = dwLayouts * 3;	// 每个布局至少包含 MAINWND / TITLEBAR / CONTAINER 三个控件
		// 获取布局信息
		// for (int i = 0; i < dwLayouts; i++)
		// {
		//		dwControls += data->solver->GetLayout(i)->dwControls;
		// }
		// 这里暂时使用硬编码，但是理论讲应该使用动态编码
		Layout** layouts[2] = { &data->pre,&data->editor };
		for (int i = 0; i < 2; i++)
		{
			CONST LayoutSolver::LayoutInfo* layout = data->solver->GetLayout(i);
			DWORD dwControls = layout->dwControls;
			*(layouts[i]) = Layout::CreateLayout(dwControls);
			if (!(*(layouts[i])))
			{
				goto ERR_FAIL_CREATE_LAYOUT;
			}
			// 设置控件信息
			for (int j = 0; j < dwControls; j++)
			{
				// 初始化需要设置 ID、TYPE、LABEL、BUFFERINFO、EVENTHANDLER 的控件信息
				CONST LayoutSolver::ControlSpec& ctrl = layout->ctrls[j];
				info.id = ctrl.id;
				info.type = CtrlInfo::GetTypeEnum(ctrl.type);
				info.label = ctrl.label;
				// Event 暂时空置
				info.pEventHandler = NULL;
				// 缓冲区两种设置方式
				// 第一种，根据事件设置动态大小缓冲区
				// 第二种，根据控件类型设置固定缓冲区

				// 绑定事件的同时根据控件所需缓冲大小设置动态缓冲区
				if (ctrl.event)
				{
					EventInfo event = UIEventHandler::BindEventHandler(ctrl.event);
					info.pEventHandler = event.pfnEvent;
					// 如果该事件需要提前申请一定的缓冲区，则申请
					if (event.bufferSize != 0)
					{
						info.bufferInfo = MemPool->Allocate(event.bufferSize);
					}
				}
				// 根据根据控件类型设置缓冲区信息
				switch (info.type)
				{
				case CTRL_TYPE_TITLE:
				{
					// 标题栏控件需要设置缓冲区信息
					info.bufferInfo = MemPool->Allocate(TITLEBAR_BUFFER);
					((TitleBuffer*)info.bufferInfo.buffer)->text = ctrl.text;
					break;
				}
				case CTRL_TYPE_BUTTON:
				case CTRL_TYPE_TEXT:
				{
					// 按钮和按钮和文本控件需要设置缓冲区信息
					info.bufferInfo = {
						(void*)ctrl.text,
						8
					};
					break;
				}
				case CTRL_TYPE_INPUT_REAL:
				{
					// 实数输入需要设置缓冲区
					info.bufferInfo = MemPool->Allocate(DOUBLE_BUFFER);
					break;
				}
				case CTRL_TYPE_TIMEBAR:
				{
					// 特殊控件
					// 该控件先由 EVENT 中知道了缓冲区的大小，在本函数中申请空间
					// 申请空间后该函数缓冲区中有一部分需要设置 text

					// 从 UIEventHandler 处获得 Buffer 布局
					// 结构: TimeBarInfo
					// 描述: 时段列表结构体
					struct TimeBarInfo
					{
						LPCSTR hint;
						struct RANGE
						{
							int64_t left;
							int64_t right;
						} span;
					};
					// 设置提示文本
					(*(TimeBarInfo*)info.bufferInfo.buffer).hint = ctrl.text;
					break;
				}
				default:
					break;
				}

				// 设置控件
				if (!(*(layouts[i]))->SetCtrlInfo(j, info))
				{
					goto ERR_FAIL_SET_CTRL_INFO;
				}
			}
		}
	}
	// 使用自动布局
	if (!ApplyRefSize())
	{
		// 成功应用
		ret = FALSE;
		goto ERR_FAIL_APPLY_REFSIZE;
	}
	data->layout = data->pre;
	// data->layout = data->editor;
	// 初始化完毕，返回 TRUE
	ret = TRUE;
	goto END;

	// 错误标签：释放资源并返回



	// 错误: 应用参考尺寸失败
ERR_FAIL_APPLY_REFSIZE:

	// 错误：设置控件信息失败
ERR_FAIL_SET_CTRL_INFO:

	// 错误：布局对象创建失败
ERR_FAIL_CREATE_LAYOUT:

	// 错误：获取布局信息失败
ERR_FAIL_GET_LAYOUT:

	// 错误：加载布局文件失败
ERR_FAIL_LOAD_LAYOUTS:

	// 错误：内存池无可分配内存
ERR_FAIL_BUFFER_NOFREE:
	delete MemPool;
	MemPool = NULL;

	// 错误：缓冲区分配失败
ERR_FAIL_BUFFER_ALLOC:

	ret = FALSE;

	// 结束标签：返回状态
END:
	return ret;
}

// 函数: Render
// 描述: 渲染应用程序界面（不调用 IMGUI）
// 备注: 此函数暂时空置，这里的渲染的含义是指 D3D 对象的操作
//	     实际的界面渲染将由框架调用 IMGUI 来完成
//	     但是当前应用程序没有 D3D 相关操作，所以此函数为空
BOOL Application::Render()
{
	return TRUE;
}


// 函数: GetLayout
// 描述: 获取布局对象引用
Layout& Application::GetLayout()
{
	return *(data->layout);
}

// 函数: Release
// 描述: 释放应用程序单例对象
VOID Application::Release()
{
	// 由于更新了 MemPool 的使用逻辑，单例对象的释放也需要更新
	// 统一通过 MemPool 来释放单例对象和相关资源

	if (MemPool)
	{
		// 在释放 MemPool 之前，先调用 SecureZeroAll 来清零缓冲区中的所有数据，以确保安全
		MemPool->SecureZeroAll();
		// 设置 Layout 类的 secure buffer 引用为 NULL，防止在 MemPool 释放后继续访问
		Layout::SetSecBuffer(NULL);
		// 释放安全缓冲区池，池内的所有资源也会被释放
		delete MemPool;
		MemPool = NULL;
	}
}
// 函数: GetRefsize()
// 描述: 获取参考尺寸对象
CONST AutoSize& Application::GetRefsize()
{
	return data->refsize;
}
// 函数: SetWindowHandle
// 功能: 设置窗体句柄
VOID Application::SetWindowHandle(HWND hSet)
{
	data->hWnd = hSet;
	return;
}
// 函数: GetWindowHandle
// 功能: 设置窗体句柄
HWND Application::GetWindowHandle()
{
	return data->hWnd;
}
// 函数: GetCurrentFolder
// 功能: 获取当前操作目录
LPCSTR Application::GetCurrentFolder()
{
	return data->lpOperfolder;
}
// 函数: ResetStatic
// 功能: 重设应用工作状态
VOID Application::ResetStatic()
{
	// 重设窗体样式
	LONG_PTR style = GetWindowLongPtr(data->hWnd, GWL_STYLE);
	SetWindowLongPtr(data->hWnd, GWL_STYLE, style & ~WS_THICKFRAME);
	SetWindowPos(data->hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}
// 函数: SetWindowSize
// 功能: 设置窗体大小
VOID Application::SetWindowSize(SIZE size)
{
	AutoSize::SizeInfo<DWORD> si = data->refsize.GetSizeInfo<DWORD>();
	si.windowWidth = size.cx;
	si.windowHeight = size.cy;
	data->refsize.SetSize(si);
	ApplyRefSize();
	// 更新窗体矩形
	data->IsMaximizedWnd = FALSE;
}
// 函数: SetMaxsize
// 功能: 设置窗体最大化
VOID Application::SwitchMaximized()
{
	if (data->layout == data->editor)
	{
		if (!data->IsMaximizedWnd)
		{
			GetWindowRect(data->hWnd, &data->rect);
			AutoSize::SizeInfo<DWORD> si = data->refsize.GetSizeInfo<DWORD>();
			SetWindowPos(data->hWnd, HWND_TOP, 0, 0, si.screenWidth, si.screenHeight, NULL);
			data->IsMaximizedWnd = TRUE;
		}
		else
		{
			auto [left, top, right, bottom] = data->rect;
			SetWindowPos(data->hWnd, HWND_TOP, left, top, right - left, bottom - top, NULL);
			data->IsMaximizedWnd = FALSE;
		}
	}
}
// 函数: IsMaximized
// 功能: 当前是否为最大化窗体
BOOL Application::IsMaximized()
{
	BOOL bRet = FALSE;
	if (data->layout == data->editor)
	{
		bRet = data->IsMaximizedWnd;
	}
	return bRet;
}
// 函数: FindControl
// 功能: 根据控件所属的 Index 和 Label 获取该控件
const CtrlInfo* Application::FindControl(UINT index, LPCSTR label)
{
	CtrlInfo* ret = NULL;
	// 暂时硬编码
	if (index >= 0 && index <= 1)
	{
		Layout* ui[2] = { data->pre, data->editor };
		CONST LayoutSolver::LayoutInfo* layout = data->solver->GetLayout(index);
		for (UINT i = 0; i < layout->dwControls; i++)
		{
			if (strcmp(layout->ctrls[i].label, label) == 0)
			{
				ret = ui[index]->GetCtrlInfo(i);
				break;
			}
		}
	}
	// 返回控件
	return ret;
}
// 函数: FindControlIndex
// 功能: 根据控件所属的 Index 和 Label 获取该控件的 ID（目的是为了不受 CONST 限制的操作）
UINT Application::FindControlIndex(UINT index, LPCSTR label)
{
	UINT ret = 0;
    // 暂时硬编码
	if (index >= 0 && index <= 1)
	{
		CONST LayoutSolver::LayoutInfo* layout = data->solver->GetLayout(index);
		for (UINT i = 0; i < layout->dwControls; i++)
		{
			if (strcmp(layout->ctrls[i].label, label) == 0)
			{
				ret = i;
				break;
			}
		}
	}
	// 返回控件
	return ret;
}
// 函数: SwitchLayout
// 功能: 切换布局
VOID Application::SwitchLayout(LPCSTR name)
{
	DWORD dwNum = data->solver->GetLayoutsCount();
	for (DWORD i = 0; i < dwNum; i++)
	{
		if (strcmp(data->solver->GetLayout(i)->name, name) == 0)
		{
			// 临时构建一个数组，未来会被替换
			Layout* ui[2] = { data->pre,data->editor };
			data->layout = ui[i];
			break;
		}
	}
}
// 函数: SetCurrentFolder
// 功能: 设置当前操作目录
VOID Application::SetCurrentFolder(LPCSTR path)
{
	data->lpOperfolder = path;
}
