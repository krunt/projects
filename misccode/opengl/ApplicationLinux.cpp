
void SetupOpenGL() {
    const SDL_VideoInfo* info = NULL;
    /* Dimensions of our window. */
    int width = 0;
    int height = 0;
    /* Color depth in bits of our window. */
    int bpp = 0;
    /* Flags we will pass into SDL_SetVideoMode. */
    int flags = 0;

    /* First, initialize SDL's video subsystem. */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        msg_failure( "Video initialization failed: %s\n", SDL_GetError( ) );
    }

    /* Let's get some video information. */
    info = SDL_GetVideoInfo( );

    if( !info ) {
        /* This should probably never happen. */
        msg_failure( "Video query failed: %s\n", SDL_GetError( ) );
    }

    width = 640;
    height = 480;
    bpp = info->vfmt->BitsPerPixel;

    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    flags = SDL_OPENGL;

    if( SDL_SetVideoMode( width, height, bpp, flags ) == 0 ) {
        msg_failure( "Video mode set failed: %s\n", SDL_GetError( )
    }

    SDL_ShowCursor( 0 );

    /* Culling. */
    //glCullFace( GL_BACK );
    //glFrontFace( GL_CW );
    glDisable( GL_CULL_FACE );

    glClearColor( 0, 0, 0, 0 );

    glViewport( 0, 0, width, height );

    //glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_STENCIL_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    gl_camera.Init( idVec3( -50, 0, 10 ), idAngles() );

    if ( glewInit() != GLEW_OK ) {
        msg_failure0( "glewInit() != GLEW_OK\n" );
    }
}

LinuxApplication::LinuxApplication() {
    idLib::Init();

    SetupOpenGL();

    gl_render.Init();

    gl_game.Init();
}

LinuxApplication::~LinuxApplication() {
    idLib::Shutdown();
}


void LinuxApplication::PumpMessages() {
    SDL_Event event;
    while ( SDL_PollEvent( &event ) ) {
        switch( event.type ) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            KeyButtonKey k = KeyButtonKey::BUTTON_NONE;

            switch ( event.key.keysym.sym ) {
            case SDLK_w: k = KeyButtonKey::BUTTON_W; break;
            case SDLK_s: k = KeyButtonKey::BUTTON_S; break;
            case SDLK_a: k = KeyButtonKey::BUTTON_A; break;
            case SDLK_d: k = KeyButtonKey::BUTTON_D; break;
            };

            if ( k != KeyButtonKey::BUTTON_NONE ) {
                if ( event.type == SDL_KEYDOWN ) {
                    OnKeyDown( k );
                } else {
                    OnKeyUp( k );
                }
            }
            break;
        }

        case SDL_MOUSEBUTTONDOWN: {
            MouseButtonKey k = MouseButtonKey::BUTTON_NONE;
            switch ( event.button.button ) {
            case SDL_BUTTON_LEFT:
                k = MouseButtonKey::BUTTON_LEFT;
                break;
            case SDL_BUTTON_RIGHT:
                k = MouseButtonKey::BUTTON_RIGHT;
                break;
            };
            if ( k != MouseButtonKey::BUTTON_NONE ) {
                OnMouseDown( k );
            }
            break;
        }

        case SDL_MOUSEMOTION: {
            MouseEvent ev;
            ev.m_mouseX = event.motion.x;
            ev.m_mouseY = event.motion.y;
            OnMouseMove( ev );
            break;
        }

        };
    }
}

void LinuxApplication::Tick(  int ms ) {
    gl_game.RunFrame( ms );

    gl_game.Render();
    gl_render.Render( gl_camera.GetPlayerView() );
}

