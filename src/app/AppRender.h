#pragma once
#include <d3d11.h>

struct HostApi;

// ================================
// 命名空间: AppRender
// 描述: 业务侧对渲染设施的访问入口
// 备注: 设备经框架宿主能力表取得（HostApi::GetNativeDevice），业务不直接依赖具体宿主实现
// ================================
namespace AppRender
{
	// 函数: SetHost
	// 描述: 登记宿主能力表（须在取设备之前调用；传 NULL 解除登记）
	VOID SetHost(CONST HostApi* pHost);
	// 函数: GetDevice
	// 描述: 取渲染宿主使用的 D3D11 设备
	// 返回:
	//		ID3D11Device*: 宿主未登记或渲染设施未就绪时返回 NULL
	ID3D11Device* GetDevice();
	// 函数: GetDeviceContext
	// 描述: 取渲染宿主使用的 D3D11 设备上下文
	// 返回:
	//		ID3D11DeviceContext*: 宿主未登记或渲染设施未就绪时返回 NULL
	ID3D11DeviceContext* GetDeviceContext();
}
