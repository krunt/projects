static WindowsApplication *d3dApplication = NULL;

static LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return d3dApplication->MsgProc(hwnd, msg, wParam, lParam);
}

LRESULT WindowsApplication::MsgProc(HWND hwnd, 
        UINT msg, WPARAM wParam, LPARAM lParam) 
{
    switch ( msg ) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        MouseButtonKey k = msg == WM_LBUTTONDOWN 
            ? MouseButtonKey::SDL_BUTTON_LEFT : MouseButtonKey::SDL_BUTTON_RIGHT;
        OnMouseDown( k );
        return 0;
    }

    case WM_MOUSEMOVE: {
        MouseEvent ev;
        ev.m_mouseX = GET_X_LPARAM(lParam);
        ev.m_mouseY = GET_Y_LPARAM(lParam);
        OnMouseMove( ev );
        return 0;
    }

    case WM_KEYUP:
    case WM_KEYDOWN: {
        KeyButtonKey k = KeyButtonKey::BUTTON_NONE;
        switch ( wParam ) {
        case 0x57: // 'w'
            k = KeyButtonKey::BUTTON_W; break;
        case 0x53: // 's'
            k = KeyButtonKey::BUTTON_S; break;
        case 0x41: // 'a'
            k = KeyButtonKey::BUTTON_A; break;
        case 0x44: // 'a'
            k = KeyButtonKey::BUTTON_D; break;
        };
        if ( k != KeyButtonKey::BUTTON_NONE ) {
            if ( msg == WM_KEYDOWN ) {
                OnKeyDown( k );
            } else {
                OnKeyUp( k );
            }
        }
        return 0;
    }};

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowsApplication::InitMainWindow() {
    WNDCLASS wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc; 
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = m_hInstance;
    wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = L"D3DWndClassName";
    
    if( !RegisterClass(&wc) )
    {
        msg_failure0( "RegisterClass Failed." );
    }
    
    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = { 0, 0, m_clientWidth, m_clientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width  = R.right - R.left;
    int height = R.bottom - R.top;
    
    mhMainWnd = CreateWindow(L"D3DWndClassName", mMainWndCaption.c_str(), 
    	WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
        width, height, 0, 0, mhAppInst, 0); 

    if( !mhMainWnd ) {
        msg_failure0( "CreateWindow Failed." );
    }
    
    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);
}

WindowsApplication::WindowsApplication() {
    m_hInstance = GetModuleHandle(NULL);
    m_clientWidth = 640;
    m_clientHeight = 480;

    idLib::Init();

    gl_render.Init();

    gl_game.Init();

    InitMainWindow();
    InitDirect3D();

    d3dApplication = this;
}

WindowsApplication::~WindowsApplication() {
    idLib::Shutdown();
}

void WindowsApplication::PumpMessages() {
    MSG msg = { 0 };
    while ( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) ) {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }
}

void WindowsApplication::Tick(  int ms ) {
    gl_game.RunFrame( ms );

    gl_game.Render();
    gl_render.Render( gl_camera.GetPlayerView() );
}

