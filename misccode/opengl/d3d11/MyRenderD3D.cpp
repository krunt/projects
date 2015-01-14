

static float flipMatrixData[4][4] = {
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { 1, 0, 0, 0 },
    { 0, 0, 0, 1 },
};

static const idMat4 glflipMatrix( flipMatrixData );

void MyRenderD3D::SetupDirect3D() {
    // Create the device and device context.
    
    UINT createDeviceFlags = 0;
    
    #if defined(DEBUG) || defined(_DEBUG)  
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        0,                 // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        0,                 // no software device
        createDeviceFlags, 
        0, 0,              // default feature level array
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &featureLevel,
        &m_d3dContext);
    
    if( FAILED(hr) ) {
        msg_failure0( "D3D11CreateDevice Failed." );
    }
    
    if( featureLevel != D3D_FEATURE_LEVEL_11_0 ) {
        msg_failure0( "Direct3D Feature Level 11 unsupported." );
    }
    
    // Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.
    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width  = m_clientWidth;
    sd.BufferDesc.Height = m_clientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount  = 1;
    sd.OutputWindow = mhMainWnd;
    sd.Windowed     = true;
    sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags        = 0;
    
    IDXGIDevice* dxgiDevice = 0;
    HR(m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), 
                        (void**)&dxgiDevice));
          
    IDXGIAdapter* dxgiAdapter = 0;
    HR(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter));
    
    IDXGIFactory* dxgiFactory = 0;
    HR(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory));
    
    HR(dxgiFactory->CreateSwapChain(m_d3dDevice, &sd, &m_swapChain));
    
    ReleaseCOM(dxgiDevice);
    ReleaseCOM(dxgiAdapter);
    ReleaseCOM(dxgiFactory);

    // Resize the swap chain and recreate the render target view.
    HR(m_swapChain->ResizeBuffers(1, 
                m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
    
    ID3D11Texture2D* backBuffer;
    HR(m_swapChain->GetBuffer(0, 
        __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)));
    HR(m_d3dDevice->CreateRenderTargetView(backBuffer, 0, &m_renderTargetView));
    ReleaseCOM(backBuffer);
    
    // Create the depth/stencil buffer and view.
    D3D11_TEXTURE2D_DESC depthStencilDesc;
    depthStencilDesc.Width     = mClientWidth;
    depthStencilDesc.Height    = mClientHeight;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count   = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Usage          = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
    depthStencilDesc.CPUAccessFlags = 0; 
    depthStencilDesc.MiscFlags      = 0;
    
    HR(md3dDevice->CreateTexture2D(&depthStencilDesc,
                0, &m_depthStencilBuffer));
    HR(md3dDevice->CreateDepthStencilView(m_depthStencilBuffer, 
        0, &m_depthStencilView));
    
    
    // Bind the render target view and depth/stencil view to the pipeline.
    m_d3dContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
    
    // Set the viewport transform.
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width    = static_cast<float>(m_clientWidth);
    m_screenViewport.Height   = static_cast<float>(m_clientHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;
    
    m_d3dContext->RSSetViewports(1, &m_screenViewport);
}

void MyRenderD3D::Init() {
    SetupDirect3D();

    MyRenderBase::Init();
}

void MyRenderD3D::CreateShaderPrograms() {
    std::vector<std::string> shaderList;

    shaderList.push_back( "d3d11/hlsl/simple.vs.hlsl" );
    shaderList.push_back( "d3d11/hlsl/simple.ps.hlsl" );

    m_shaderProgram = std::shared_ptr<GpuProgram>( new GpuProgram() );
    assert( m_shaderProgram->Init( shaderList ) );
    assert( m_shaderProgram->IsOk() );

    shaderList.clear();

    shaderList.push_back( "d3d11/hlsl/sky.vs.hlsl" );
    shaderList.push_back( "d3d11/hlsl/sky.ps.hlsl" );

    m_skyProgram = std::shared_ptr<GpuProgram>( new GpuProgram() );
    assert( m_skyProgram->Init( shaderList ) );
    assert( m_skyProgram->IsOk() );
}

template <typename T>
class D3DObjectDeleter {
public:
    void operator()( T *buf ) {
        if ( buf != nullptr ) {
            ReleaseCOM( buf );
        }
    }
};

std::shared_ptr<gsurface_t> MyRenderD3D::CacheSurface( const surf_t &s ) {
    std::shared_ptr<gsurface_t> gsurf( new gsurface_t() );
    std::shared_ptr<ID3D11Buffer> vertBuffer = nullptr, indexBuffer = nullptr;

    ID3D11Buffer *p;

    D3D11_BUFFER_DESC vbd;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.ByteWidth = sizeof(drawVert_t) * s.m_verts.size();
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    vbd.MiscFlags = 0;
    vbd.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA vInitData;
    vInitData.pSysMem = s.m_verts.data();
    HR(md3dDevice->CreateBuffer(&vbd, &vInitData, &p));
    vertBuffer.Reset( p, D3DBufferDeleter<ID3D11Buffer>() );

    D3D11_BUFFER_DESC ibd;
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.ByteWidth = s.m_numIndices * sizeof(int);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    ibd.MiscFlags = 0;
    ibd.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA iInitData;
    iInitData.pSysMem = s.indices.data();
    HR(md3dDevice->CreateBuffer(&ibd, &iInitData, &p));
    indexBuffer.Reset( p, D3DBufferDeleter<ID3D11Buffer>() );

    gsurf->m_vertexBuffer = vertBuffer;
    gsurf->m_indexBuffer = indexBuffer;
    gsurf->m_numIndices = s.m_numIndices;

    if ( s.m_matName == "white" ) {
        gsurf->m_material = m_whiteMaterial;
    } else if ( s.m_matName == "sky" ) {
        gsurf->m_material = m_skyMaterial;
    } else {
        if ( m_materialCache.find( s.m_matName ) != m_materialCache.end() ) {
            cached->m_material = m_materialCache[ s.m_matName ];
        } else {
            std::shared_ptr<material_t> t = new material_t();
            t->m_texture = CreateTexture( s.m_matName );
            t->m_program = m_shaderProgram;
            if ( !t.IsValid() ) {
                msg_failure( "Can't cache material `%s'\n", s.m_matName.c_str() );
                return nullptr;
            }
            m_materialCache[ s.m_matName ] = t;
            gsurf->m_material = t;
        }
    }

    return gsurf;
}

static idVec3 ToWorld( idVec3 &v ) {
    return ( glflipMatrix * idVec4( v, 1.0f ) ).ToVec3();
}

void MyRenderD3D::FlushFrame() {
    HR(m_swapChain->Present(0, 0));
}

void MyRenderD3D::OnPreRender() {
    float blackColor[4] = { 0, 0, 0, 0 };

    m_d3dContext->ClearRenderTargetView(
        m_renderTargetView, blackColor );

    m_d3dContext->ClearDepthStencilView( m_depthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void MyRenderD3D::RenderSurface( const std::shared_ptr<gsurface_t> &surf ) {
    idmat4 mvpMatrix;

    mvpMatrix = m_projectionMatrix * glflipMatrix 
        * m_viewMatrix * surf.m_modelMatrix;

    surf->m_material.m_program->Bind( "mvp_matrix", mvpMatrix.Transpose() );
    surf->m_material.m_program->Bind( "model_matrix", 
            ( flipMatrix * surf.m_modelMatrix ).Transpose() );
    surf->m_material.m_program->Bind( "eye_pos", ToWorld( m_eye.ToVec3() ) );

    surf->m_material.m_program->Use();

    surf->m_material.m_texture->Bind();

    m_d3dContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    int stride = sizeof(drawVert_t);
    int offset = 0;
    m_d3dContext->IASetVertexBuffers( 0, 1, m_vertexBuffer.get(),
            &stride, &offset );
    m_d3dContext->IASetIndexBuffer( m_indexBuffer.get(), 
        DXGI_FORMAT_R32_UINT, 0 );

    m_d3dContext->DrawIndexed( surf.numIndices, 0, 0 );
}

