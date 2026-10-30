#include <windows.h>

#include "Identity.h"
#include "app/AppEvents.h"
#include "app/AppRender.h"
#include "app/AppState.h"
#include "app/resource.h"
#include "framework/dispatch/Dispatch.h"
#include "framework/render/imgui/ImGuiHost.h"

// 函数: main
// 描述: VideoCut 程序入口(取宿主表 → 调度初始化 → 业务初始化 → 宿主启动 → 主循环 → 释放)
int main(int, char**)
{
	CONST HostApi* pHost = ImGuiHost::Register();
	AppRender::SetHost(pHost);
	DispatchConfig cfg{};
	cfg.szDocName = IDENTITY_DOC_NAME;
	cfg.szDocEnv = IDENTITY_DOC_ENV;
	cfg.szAutoCloseEnv = IDENTITY_ENV_AUTOCLOSE;
	cfg.szLogPrefix = IDENTITY_LOG_PREFIX;
	cfg.dwWndCount = 2;			// 布局文档两枚(编辑基座 + 准备内嵌(flex)); 装配上界与窗口数同源
	cfg.bDarkFollowSystem = TRUE;
	if (pHost == NULL || !Dispatch::Init(pHost, cfg, AppEvents::GetBindings))
	{
		MessageBoxW(NULL, L"应用初始化失败(布局文档载入或事件绑定注入失败)!",
			IDENTITY_ERR_TITLE, MB_OK | MB_ICONERROR);
		return 1;
	}
	if (!AppState::Init())
	{
		MessageBoxW(NULL, L"业务状态初始化失败!",
			IDENTITY_ERR_TITLE, MB_OK | MB_ICONERROR);
		Dispatch::Release();
		return 1;
	}
	ImGuiHost::SetFontSize(Dispatch::GetFontSize());
	{
		HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_ICON1),
			IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
		if (hIcon != NULL)
		{
			pHost->SetAppIcon(hIcon);
		}
	}
	if (!pHost->Start(Dispatch::GetWndCount(), IDENTITY_APP_NAME, Dispatch::OnSegment, NULL))
	{
		MessageBoxW(NULL, L"渲染宿主启动失败(窗口类注册、窗口创建或渲染设施初始化失败)!",
			IDENTITY_ERR_TITLE, MB_OK | MB_ICONERROR);
		AppState::Release();
		Dispatch::Release();
		return 1;
	}
	// 准备界面是编辑界面的内嵌(flex)成员, 不占用第二个标准窗口; 运行期只显示首个窗口
	ImGuiHost::SetActiveWndCount(1);
	pHost->Run(AppState::Frame, NULL, Dispatch::GetExitAddress());
	pHost->Release();
	pHost->DestroyWnds();
	AppState::Release();
	Dispatch::Release();
	return 0;
}
