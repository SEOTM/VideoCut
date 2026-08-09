#pragma once
#include <d3d11.h>
#include <dxgi.h>

class D3D11Manager
{
private:
    // 私有构造函数和析构函数（单例模式）
    D3D11Manager() = default;
    ~D3D11Manager() = default;

    // 禁止拷贝和赋值
    D3D11Manager(const D3D11Manager&) = delete;
    D3D11Manager& operator=(const D3D11Manager&) = delete;

    // D3D11 相关资源
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;

    // 窗口状态
    HWND m_hWnd = nullptr;
    BOOL m_swapChainOccluded = FALSE;
    UINT m_resizeWidth = 0;
    UINT m_resizeHeight = 0;

    // 私有初始化方法
    bool CreateDeviceD3DInternal(HWND hWnd);
    void CreateRenderTargetInternal();

public:
    // 获取单例实例
    static D3D11Manager& GetInstance();

    // 初始化与清理
    bool Initialize(HWND hWnd);
    void Shutdown();

    // 渲染目标管理
    void CreateRenderTarget();
    void CleanupRenderTarget();

    // 窗口大小变化处理
    void SetResizeDimensions(UINT width, UINT height);
    void ProcessResize();

    // 获取资源接口
    ID3D11Device* GetDevice() const { return m_pd3dDevice; }
    ID3D11DeviceContext* GetDeviceContext() const { return m_pd3dDeviceContext; }
    ID3D11RenderTargetView* GetMainRenderTargetView() const { return m_mainRenderTargetView; }
    IDXGISwapChain* GetSwapChain() const { return m_pSwapChain; }

    // 状态查询
    BOOL IsSwapChainOccluded() const { return m_swapChainOccluded; }
    void SetSwapChainOccluded(BOOL occluded) { m_swapChainOccluded = occluded; }

    // 呈现
    HRESULT Present(UINT syncInterval = 1, UINT flags = 0);

    // 清空渲染目标
    void ClearRenderTargetView(const FLOAT color[4]);
};