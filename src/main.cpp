// 预定义宏
#define IMGUI_DEFINE_MATH_OPERATORS

// 编译指示
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <combaseapi.h>
#include <imm.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

#include <iostream>

// 包含自定义头文件
#include "BasicDesign.h"
#include "Application.h"
#include "D3D11Manager.h"
#include "Controls.h"
#include "resource.h"

#ifdef NDEBUG
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif // NDEBUG

// 全局变量声明
HWND hWnd;					// 窗口句柄
DWORD dwDisableIME = 0;		// 非 IME 输入需求标志
HIMC hIMC;					// IME 输入上下文
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);  // 窗口过程

// 启用UTF-8 API支持
#pragma comment(linker, "/manifestdependency:\"type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 初始化UTF-8环境（程序启动时调用一次）
VOID Init_UTF8_Env();
// 函数：检查当前系统是否为深色模式
// 返回值：true - 深色模式；false - 亮色模式或无法确定
bool IsSystemDarkMode();
// 函数: main
// 描述: 主函数
int main(int, char**)
{
	Init_UTF8_Env();
	// 定义返回值
	INT ret = 0;
	// 常驻变量声明区（统一声明变量，而非就近声明）
	ATOM atom;				// 窗体原子
	BOOL IsExit = FALSE;		// 退出标志
	LPCWSTR ERRTEXT = NULL;	// 错误文本
	HINSTANCE hInstance;		// 应用句柄
	// 初始化 COM 接口
	if (!SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
	{
		ERRTEXT = L"COM 接口初始化失败！";
		ret = 1;
		goto MAIN_END;
	}
	// 使进程感知DPI并获取主显示器缩放比例
	ImGui_ImplWin32_EnableDpiAwareness();

	// 测试FFmpeg是否正确链接
	av_log_set_level(AV_LOG_INFO);
	std::cout << "FFmpeg linked successfully. Version: " << av_version_info() << std::endl;
	// 获取应用句柄
	hInstance = GetModuleHandle(NULL);
	if (!hInstance)
	{
		ERRTEXT = L"获取应用句柄失败！";
		ret = 1;
		goto MAIN_UNINIT_COM;
	}
	// 获取应用实例
	{
		BOOL IsSucc = Application::Init();
		if (IsSucc == FALSE)
		{
			ERRTEXT = L"应用程序初始化失败！";
			ret = 1;
			goto MAIN_UNINIT_COM;
		}
	}
	// 注册并创建窗口
	{
		// 获取设计大小
		DWORD wSize = Application::GetRefsize().GetWindowWidth<DWORD>();
		DWORD hSize = Application::GetRefsize().GetWindowHeight<DWORD>();
		// 定义窗口样式
		//  - 不可调整大小
		//  - 没有最大化按钮
		// DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
		WS_OVERLAPPEDWINDOW;
		DWORD dwStyle = WS_POPUP | WS_EX_TOPMOST | WS_SYSMENU | WS_MINIMIZEBOX;

		// 定义窗体类
		WNDCLASSEX wc = { sizeof(WNDCLASSEX),CS_CLASSDC,WndProc,0L,0L, hInstance,NULL, NULL, NULL, NULL,L"TOOLUIBYKEENEDGE",NULL };
		// 加载并设置图标
		 HICON hICON = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
		 wc.hIcon = hICON;
		 wc.hIconSm = hICON;
		// 注册窗体
		atom = RegisterClassEx(&wc);
		if (!atom)
		{
			ERRTEXT = L"窗体注册失败！";
			ret = 1;
			goto MAIN_END;
		}
		// 创建窗口
		hWnd = CreateWindow(
			MAKEINTATOM(atom), L"视频拼接剪切辅助编辑器",		// 注册 / 窗口名称
			dwStyle,							// 窗口样式
			CW_USEDEFAULT, CW_USEDEFAULT,	// 位置
			wSize, hSize,	// 尺寸
			NULL, NULL, hInstance, NULL
		);
		if (!hWnd)
		{
			ERRTEXT = L"窗口创建失败！";
			ret = 1;
			goto MAIN_UNREG_CLS;
		}
		else
		{
			Application::SetWindowHandle(hWnd);
		}
		hIMC = ImmGetContext(hWnd);
		// 设置窗体圆角
		{
			DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
			HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
			if (SUCCEEDED(hr))
			{
				std::cout << u8"圆角实现: DWM" << std::endl;
			}
			else
			{
				DWORD space = Application::GetRefsize().GetSpace<DWORD>();
				HRGN hRgn = CreateRoundRectRgn(0, 0, wSize, hSize, space, space); // 最后两个参数控制圆角弧度
				SetWindowRgn(hWnd, hRgn, TRUE); // TRUE 表示系统接管区域对象的删除
				std::cout << u8"圆角实现: RGN:" << space << std::endl;
			}
		}
	}

	// 初始化 Direct3D 设备
	if (!D3D11Manager::GetInstance().Initialize(hWnd))
	{
		ERRTEXT = L"D3D 设备初始化失败！";
		ret = 1;
		goto MAIN_CLN_D3D;
	}
	// 显示窗口
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	if (FALSE)
	{
		int policy = DWMNCRP_ENABLED;
		DwmSetWindowAttribute(hWnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
		MARGINS margins = { 1,1,1,1 };
		DwmExtendFrameIntoClientArea(hWnd, &margins);
	}

	// 设置 IMGUI 上下文
	{
		IMGUI_CHECKVERSION();
		if (!ImGui::CreateContext())
		{
			ERRTEXT = L"IMGUI 上下文创建失败！";
			ret = 1;
			goto MAIN_DST_CTX;
		}
	}
	// 为 IMGUI 加载字体
	{
		// 初始化

		const char* sFontFiles[] = {				// 系统字体路径
			"C:\\Windows\\Fonts\\msyh.ttc",			// 微软雅黑
			"C:\\Windows\\Fonts\\simhei.ttf",		// 黑体
			"C:\\Windows\\Fonts\\simsun.ttc",		// 宋体
			"C:\\Windows\\Fonts\\simfang.ttf",		// 仿宋
			"C:\\Windows\\Fonts\\simkai.ttf",		// 楷体
			NULL
		};
		ImGuiIO& io = ImGui::GetIO();	// 获取 IO 对象引用
		io.IniFilename = NULL;
		io.Fonts->Clear();				// 清除默认字体
		ImFont* font = NULL;			// 定义字体指针
		FLOAT fFontSize = Application::GetRefsize().GetFontSize<FLOAT>();	// 字体大小

		// 依次加载系统字体路径中的字体
		for (int i = 0; sFontFiles[i]; i++)
		{
			font = io.Fonts->AddFontFromFileTTF(sFontFiles[i], fFontSize, nullptr, io.Fonts->GetGlyphRangesChineseFull());
			if (font)
			{
				break;
			}
		}
		// 如果所有中文字体都加载失败，回退到默认字体
		if (!font) {
			io.Fonts->AddFontDefault();
			MessageBoxA(hWnd, "未能加载中文字体，将使用默认字体。\n某些中文字符可能无法正确显示。", "字体警告", MB_OK | MB_ICONWARNING);
		}
		else {
			io.FontDefault = font; // 设置为默认字体
		}
	}
	// 设置 IMGUI 布局
	{
		// 设置内距
		ImGui::GetStyle().WindowPadding = { 0,0 };
		ImGui::GetStyle().WindowRounding = Application::GetRefsize().GetSpace<FLOAT>();
		ImGui::GetStyle().FramePadding = ImVec2(Application::GetRefsize().GetPadding<FLOAT>(), Application::GetRefsize().GetPadding<FLOAT>());
		ImGui::GetStyle().FrameRounding = Application::GetRefsize().GetPadding<FLOAT>();
		// 在 ImGui 初始化后设置透明
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f); // 半透明
		ImGui::GetStyle().Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);   // 无边框


		// 启用键盘控制
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		// 设置样式
		{
			// 获取系统样式
			BOOL IsDrak = IsSystemDarkMode();
			// 设置基底样式
			if (IsDrak)
			{
				ImGui::StyleColorsDark();
			}
			else
			{
				ImGui::StyleColorsLight();
			}
			// 获取 ImGui 样式
			ImGuiStyle& style = ImGui::GetStyle();
			// 创建颜色方案
			MatchCR cr(IsDrak ? MatchCR::DEFAULT_MODE::DEFAULT_MODE_DARK : MatchCR::DEFAULT_MODE::DEFAULT_MODE_LIGHT);

			// 窗口背景
			style.Colors[ImGuiCol_WindowBg] = MAKE_CR_IMVEC4(cr.GetBack());
			style.Colors[ImGuiCol_ChildBg] = MAKE_CR_IMVEC4(cr.GetSurface());
			style.Colors[ImGuiCol_PopupBg] = MAKE_CR_IMVEC4(cr.GetBack());
			style.Colors[ImGuiCol_MenuBarBg] = MAKE_CR_IMVEC4(cr.GetSurface()); // 菜单栏用表面色稍作区分

			// 边框
			style.Colors[ImGuiCol_Border] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_BorderShadow] = MAKE_CR_IMVEC4(cr.GetBorder()); // 或调暗
			style.Colors[ImGuiCol_Separator] = MAKE_CR_IMVEC4(cr.GetBorder());

			// 框架背景（输入框、下拉框等）
			style.Colors[ImGuiCol_FrameBg] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_FrameBgHovered] = MAKE_CR_IMVEC4(cr.GetHover());
			style.Colors[ImGuiCol_FrameBgActive] = MAKE_CR_IMVEC4(cr.GetPrimary()); // 激活时用主色

			// 标题栏
			style.Colors[ImGuiCol_TitleBg] = MAKE_CR_IMVEC4(cr.GetSecondary());
			style.Colors[ImGuiCol_TitleBgActive] = MAKE_CR_IMVEC4(cr.GetPrimary());
			style.Colors[ImGuiCol_TitleBgCollapsed] = MAKE_CR_IMVEC4(cr.GetSecondary());

			// 按钮
			style.Colors[ImGuiCol_Button] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_ButtonHovered] = MAKE_CR_IMVEC4(cr.GetHover());
			style.Colors[ImGuiCol_ButtonActive] = MAKE_CR_IMVEC4(cr.GetPrimary());

			// 标头（可折叠区域、树节点等）
			style.Colors[ImGuiCol_Header] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_HeaderHovered] = MAKE_CR_IMVEC4(cr.GetHover());
			style.Colors[ImGuiCol_HeaderActive] = MAKE_CR_IMVEC4(cr.GetPrimary());

			// 滚动条
			style.Colors[ImGuiCol_ScrollbarBg] = MAKE_CR_IMVEC4(cr.GetSecondary());
			style.Colors[ImGuiCol_ScrollbarGrab] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = MAKE_CR_IMVEC4(cr.GetHover());
			style.Colors[ImGuiCol_ScrollbarGrabActive] = MAKE_CR_IMVEC4(cr.GetPrimary());

			// 滑块和勾选标记
			style.Colors[ImGuiCol_SliderGrab] = MAKE_CR_IMVEC4(cr.GetPrimary());
			style.Colors[ImGuiCol_SliderGrabActive] = MAKE_CR_IMVEC4(cr.GetEmphasis());
			style.Colors[ImGuiCol_CheckMark] = MAKE_CR_IMVEC4(cr.GetPrimary());

			// 选项卡
			style.Colors[ImGuiCol_Tab] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_TabHovered] = MAKE_CR_IMVEC4(cr.GetHover());
			style.Colors[ImGuiCol_TabActive] = MAKE_CR_IMVEC4(cr.GetPrimary());
			style.Colors[ImGuiCol_TabUnfocused] = MAKE_CR_IMVEC4(cr.GetSecondary());
			style.Colors[ImGuiCol_TabUnfocusedActive] = MAKE_CR_IMVEC4(cr.GetBorder());

			// 表格
			style.Colors[ImGuiCol_TableHeaderBg] = MAKE_CR_IMVEC4(cr.GetSecondary());
			style.Colors[ImGuiCol_TableBorderStrong] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_TableBorderLight] = MAKE_CR_IMVEC4(cr.GetBorder());
			style.Colors[ImGuiCol_TableRowBg] = MAKE_CR_IMVEC4(cr.GetBack());
			style.Colors[ImGuiCol_TableRowBgAlt] = MAKE_CR_IMVEC4(cr.GetBorder()); // 交替行

			// 文本选择背景
			style.Colors[ImGuiCol_TextSelectedBg] = MAKE_CR_IMVEC4(cr.GetHover());

			// 拖拽目标
			style.Colors[ImGuiCol_DragDropTarget] = MAKE_CR_IMVEC4(cr.GetEmphasis());

			// 导航高亮
			style.Colors[ImGuiCol_NavHighlight] = MAKE_CR_IMVEC4(cr.GetEmphasis());

			// 叠加调暗背景（模态窗口、导航窗口）
			style.Colors[ImGuiCol_ModalWindowDimBg] = MAKE_CR_IMVEC4(cr.GetOverlay());
			style.Colors[ImGuiCol_NavWindowingDimBg] = MAKE_CR_IMVEC4(cr.GetOverlay());

			// 文本颜色（通常根据背景自动选择对比色，但这里直接设置）
			DWORD crText = ~cr.GetBack() | 0xFF000000;
			DWORD crText2 = cr.GetPrimary();

			style.Colors[ImGuiCol_Text] = MAKE_CR_IMVEC4(crText);
			style.Colors[ImGuiCol_TextDisabled] = MAKE_CR_IMVEC4(crText2);
		}

		// 设置缩放
		ImGuiStyle& style = ImGui::GetStyle();
	}
	// 设置平台/渲染器后端
	{
		if (!ImGui_ImplWin32_Init(hWnd))
		{
			ERRTEXT = L"WIN32 后端启用失败！";
			ret = 1;
			goto MAIN_SDN_WIN32;
		}
		// 设置 IMGUI DX11 后端
		ID3D11Device* device = D3D11Manager::GetInstance().GetDevice();
		ID3D11DeviceContext* context = D3D11Manager::GetInstance().GetDeviceContext();
		if (!ImGui_ImplDX11_Init(device, context))
		{
			ERRTEXT = L"DX11 后端启用失败！";
			ret = 1;
			goto MAIN_SDN_DX11;
		}
	}
	// 主循环
	while (!IsExit)
	{
		// 轮询并处理消息（输入、窗口调整大小等）
		// 请参阅下面的WndProc()函数，了解如何将事件分派到Win32后端。
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				IsExit = TRUE;
			}
		}
		if (IsExit)
		{
			break;
		}
		// 处理窗口被最小化或屏幕被锁定的情况
		D3D11Manager& d3dManager = D3D11Manager::GetInstance();
		if (d3dManager.IsSwapChainOccluded() &&
			d3dManager.Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			Sleep(10);
			continue;
		}
		d3dManager.SetSwapChainOccluded(FALSE);
		// 处理窗口调整大小
		d3dManager.ProcessResize();
		// 开始 IMGUI 帧
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();


		// 调用渲染程序
		{
			Layout& layout = Application::GetLayout();
			CONST AutoSize& refsize = Application::GetRefsize();

			vec2 size;
			BOOL IsResize = Controls::CheckWindowResize(layout, &size);
			if (IsResize)
			{
				DWORD w = static_cast<DWORD>(size.x);
				DWORD h = static_cast<DWORD>(size.y);
				SetWindowPos(hWnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
			}

			Controls::RenderMain(layout, refsize);

			BOOL IsDisableIME = Controls::IsIMEDisabled();
			if (IsDisableIME)
			{
				if (ImmGetOpenStatus(hIMC))
				{
					ImmSetOpenStatus(hIMC, FALSE);
				}
			}
			else
			{
				if (!ImmGetOpenStatus(hIMC))
				{
					ImmSetOpenStatus(hIMC, TRUE);
				}
			}
		}

		// Render();
		// 渲染
		ImGui::Render();
		// 设置渲染目标
		ID3D11DeviceContext* ctx = d3dManager.GetDeviceContext();
		ID3D11RenderTargetView* rtv = d3dManager.GetMainRenderTargetView();
		ctx->OMSetRenderTargets(1, &rtv, NULL);

		// g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		// 呈现
		HRESULT hr = d3dManager.Present(1, 0);			// 垂直同步呈现
		// HRESULT hr = g_pSwapChain->Present(1, 0);		// 垂直同步呈现
		//HRESULT hr = g_pSwapChain->Present(0, 0);		// 无垂直同步呈现
		// 每帧清空时使用透明背景
		float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 完全透明
		d3dManager.ClearRenderTargetView(clear_color);

		// g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
		// 每帧清空时使用透明背景
		// float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 完全透明
		// g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
	}

	// 下面进入到清理流程
	// 正常情况下按照从上到下的顺序依次清理
	// 发生错误时，根据已经创建资源的情况，跳转到对应的位置进行清理

MAIN_SDN_DX11:
	// 关闭 IMGUI DX11 后端
	ImGui_ImplDX11_Shutdown();
MAIN_SDN_WIN32:
	// 关闭 IMGUI Win32 后端
	ImGui_ImplWin32_Shutdown();
MAIN_DST_CTX:
	// 销毁 IMGUI 上下文
	ImGui::DestroyContext();
MAIN_CLN_D3D:
	// 清理 D3D 设备
	// CleanupDeviceD3D();
	D3D11Manager::GetInstance().Shutdown();
MAIN_DST_WND:
	// 释放 IMC
	ImmReleaseContext(hWnd, hIMC);
	// 销毁窗体
	DestroyWindow(hWnd);
MAIN_UNREG_CLS:
	// 注销窗体
	UnregisterClassW(MAKEINTATOM(atom), hInstance);
MAIN_UNINIT_COM:
	CoUninitialize();
MAIN_END:
	if (ret)
	{
		MessageBox(NULL, ERRTEXT, L"错误", MB_OK | MB_ICONERROR);
	}
	return ret;
}

// 从imgui_impl_win32.cpp前向声明消息处理程序
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32消息处理程序
// 您可以读取io.WantCaptureMouse、io.WantCaptureKeyboard标志来判断dear imgui是否想要使用您的输入。
// - 当io.WantCaptureMouse为true时，不要将鼠标输入数据分派给主应用程序，或清除/覆盖您的鼠标数据副本。
// - 当io.WantCaptureKeyboard为true时，不要将键盘输入数据分派给主应用程序，或清除/覆盖您的键盘数据副本。
// 通常，您可能总是将所有输入传递给dear imgui，并根据这两个标志对您的应用程序隐藏它们。
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED)
		{
			return 0;
		}
		D3D11Manager::GetInstance().SetResizeDimensions((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
		if (wParam == SIZE_RESTORED)
		{
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			if (style & WS_THICKFRAME)
			{
				Application::SetWindowSize({ (LONG)LOWORD(lParam),(LONG)HIWORD(lParam) });
			}
		}
		return 0;
	}
	case WM_SYSCOMMAND:
	{
		// 捕获最小化和还原命令
		if ((wParam & 0xFFF0) == SC_MINIMIZE)
		{
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			SetWindowLongPtr(hWnd, GWL_STYLE, style | WS_CAPTION);
			ShowWindow(hWnd, SW_SHOWMINIMIZED);
			return 0; // 阻止默认处理
		}
		else if ((wParam & 0xFFF0) == SC_RESTORE)
		{
			ShowWindow(hWnd, SW_RESTORE);
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			SetWindowLongPtr(hWnd, GWL_STYLE, style & ~WS_CAPTION);
			SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			SetForegroundWindow(hWnd);
			return 0;
		}
		// 其他系统命令交给默认处理
		return DefWindowProc(hWnd, msg, wParam, lParam);
		if ((wParam & 0xfff0) == SC_KEYMENU) // 禁用ALT应用程序菜单
			return 0;
		break;
	}
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	case WM_IME_STARTCOMPOSITION:
	case WM_IME_COMPOSITION:
	case WM_IME_ENDCOMPOSITION:
	case WM_IME_SETCONTEXT:
	{
		if (dwDisableIME > 0)
		{
			return 0;
		}
		else
		{
			DefWindowProcW(hWnd, msg, wParam, lParam);
		}
		break;
	}
	case WM_NCCALCSIZE:
	{
		return 0;
		break;
	}
	case WM_NCHITTEST:
	{
		if (!Application::IsMaximized())
		{
			// 首先调用默认处理，获取默认的命中结果
			LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);

			// 如果默认结果说它在客户区（HTCLIENT），我们就自己来判断
			if (hit == HTCLIENT)
			{
				// 获取鼠标的屏幕坐标（lParam 包含的是屏幕坐标）
				POINT ptMouse = { lParam & 0xFFFF, (lParam >> 16) & 0xFFFF };

				// 获取窗口矩形
				RECT rcWindow;
				GetWindowRect(hWnd, &rcWindow);

				// 定义边缘的“敏感区域”厚度，例如 5 像素
				const int nBorderWidth = 5;

				// 判断鼠标是否在边缘，并返回相应的代码
				// 左侧边缘
				if (ptMouse.x < rcWindow.left + nBorderWidth)
				{
					// 如果在左上角或左下角
					if (ptMouse.y < rcWindow.top + nBorderWidth)
						return HTTOPLEFT;
					else if (ptMouse.y > rcWindow.bottom - nBorderWidth)
						return HTBOTTOMLEFT;
					else
						return HTLEFT;
				}
				// 右侧边缘
				else if (ptMouse.x > rcWindow.right - nBorderWidth)
				{
					if (ptMouse.y < rcWindow.top + nBorderWidth)
						return HTTOPRIGHT;
					else if (ptMouse.y > rcWindow.bottom - nBorderWidth)
						return HTBOTTOMRIGHT;
					else
						return HTRIGHT;
				}
				// 上边缘
				else if (ptMouse.y < rcWindow.top + nBorderWidth)
				{
					return HTTOP;
				}
				// 下边缘
				else if (ptMouse.y > rcWindow.bottom - nBorderWidth)
				{
					return HTBOTTOM;
				}

				// 如果不在边缘，那么它就在客户区内
				return HTCLIENT;
			}

			// 如果默认结果不是客户区（比如在系统菜单按钮上），我们就返回默认结果
			return hit;
		}
	}
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

VOID Init_UTF8_Env()
{
	// 设置控制台为UTF-8（如果使用控制台）
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
}
// 函数：检查当前系统是否为深色模式
// 返回值：true - 深色模式；false - 亮色模式或无法确定
bool IsSystemDarkMode()
{
	HKEY hKey;
	const wchar_t* subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
	const wchar_t* valueName = L"AppsUseLightTheme";
	DWORD value = 0; // 用于接收读取到的值
	DWORD valueSize = sizeof(DWORD);
	DWORD dwType = 0;

	// 1. 打开注册表键
	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey);
	if (status != ERROR_SUCCESS) {
		// 注册表键打开失败，可能是系统版本过低或不支持
		return false; // 默认返回 false 或进行错误处理
	}

	// 2. 查询特定键的值
	status = RegQueryValueExW(hKey, valueName, nullptr, &dwType, (LPBYTE)&value, &valueSize);

	// 3. 关闭注册表键
	RegCloseKey(hKey);

	// 4. 检查查询是否成功，并确保数据类型正确
	if (status == ERROR_SUCCESS && dwType == REG_DWORD) {
		// 根据文档：0 = 深色模式, 1 = 亮色模式
		return (value == 0);
	}

	// 查询失败或类型不匹配，无法确定
	return false;
}