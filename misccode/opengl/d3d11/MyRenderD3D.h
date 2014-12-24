

class MyRenderD3D : public MyRenderBase {
public:
    void Init();
    void Render( const playerView_t &view );
    std::shared_ptr<gsurface_t> CacheSurface( const surf_t &s );

private:

    void OnPreRender( const playerView_t &view );
    virtual void RenderSurface( const std::shared_ptr<gsurface_t> &surf );
    bool CreateShaderPrograms();

    void SetupDirect3D();
    void CreateInputLayout();

    ID3D11Device *m_d3dDevice;
    ID3D11DeviceContext *m_d3dContext;
    IDXGISwapChain *m_swapChain;
    ID3D11Texture2D *m_depthStencilBuffer;
    ID3D11RenderTargetView *m_renderTargetView;
    ID3D11RenderTargetView *m_depthStencilView;
    D3D11_VIEWPORT m_screenViewport;
};

typedef MyRenderD3D MyRender;
extern MyRender gl_render;
