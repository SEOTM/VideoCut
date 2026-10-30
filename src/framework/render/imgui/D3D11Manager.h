#pragma once
#include <d3d11.h>
#include <dxgi.h>

class D3D11Manager
{
private:
    D3D11Manager(const D3D11Manager&) = delete;
    D3D11Manager& operator=(const D3D11Manager&) = delete;

    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;

    HWND m_hWnd = nullptr;
    BOOL m_swapChainOccluded = FALSE;
    UINT m_resizeWidth = 0;
    UINT m_resizeHeight = 0;

    bool CreateDeviceD3DInternal(HWND hWnd);
    void CreateRenderTargetInternal();

public:
    // 函数: D3D11Manager
    // 描述: 默认构造
    D3D11Manager() = default;
    // 函数: ~D3D11Manager
    // 描述: 析构时调用 Shutdown 统一释放资源
    ~D3D11Manager() { Shutdown(); }

    bool Initialize(HWND hWnd);
    void Shutdown();

    void CreateRenderTarget();
    void CleanupRenderTarget();

    void SetResizeDimensions(UINT width, UINT height);
    void ProcessResize();

    ID3D11Device* GetDevice() const { return m_pd3dDevice; }
    ID3D11DeviceContext* GetDeviceContext() const { return m_pd3dDeviceContext; }
    ID3D11RenderTargetView* GetMainRenderTargetView() const { return m_mainRenderTargetView; }
    IDXGISwapChain* GetSwapChain() const { return m_pSwapChain; }

    BOOL IsSwapChainOccluded() const { return m_swapChainOccluded; }
    void SetSwapChainOccluded(BOOL occluded) { m_swapChainOccluded = occluded; }

    HRESULT Present(UINT syncInterval = 1, UINT flags = 0);

    void ClearRenderTargetView(const FLOAT color[4]);
};
