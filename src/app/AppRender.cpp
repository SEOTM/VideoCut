#include "AppRender.h"
#include "framework/render/Render.h"

static CONST HostApi* sHost = NULL;

// 函数: SetHost
// 描述: 登记宿主能力表(见 .h)
VOID AppRender::SetHost(CONST HostApi* pHost)
{
	sHost = pHost;
}

// 函数: GetDevice
// 描述: 取渲染宿主使用的 D3D11 设备(见 .h)
ID3D11Device* AppRender::GetDevice()
{
	if (sHost == NULL || sHost->GetNativeDevice == NULL)
	{
		return NULL;
	}
	return (ID3D11Device*)sHost->GetNativeDevice();
}

// 函数: GetDeviceContext
// 描述: 取渲染宿主使用的 D3D11 设备上下文(见 .h)
ID3D11DeviceContext* AppRender::GetDeviceContext()
{
	if (sHost == NULL || sHost->GetNativeDeviceContext == NULL)
	{
		return NULL;
	}
	return (ID3D11DeviceContext*)sHost->GetNativeDeviceContext();
}
