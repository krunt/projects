#include "d3lib/Lib.h"

#include <SDL/SDL.h>
#include <vector>
#include <string>

#include "Utils.h"

#include "Common.h"

#include "Model_md3.h"

#include "Camera.h"

#include "Render.h"

static Camera gl_camera;

class MyFloor: public MyEntity {
public:

    virtual void Spawn( void ) {
        m_pos = vec3_origin;
        m_axis = mat3_identity;
        m_extents = idVec3( 100, 100, 1 );

        Precache();
    }

    virtual void Precache( void ) {
        int i, j;
        int cinds[6*2][3] = {
        { 6, 7, 2 },
        { 7, 3, 2 },
        { 7, 5, 3 },
        { 5, 1, 3 },
        { 5, 4, 1 },
        { 1, 4, 0 },
        { 4, 6, 0 },
        { 0, 6, 2 },
        { 4, 5, 6 },
        { 5, 7, 6 },
        { 1, 0, 3 },
        { 0, 2, 3 },
        };

        surf_t surf;
        std::vector<drawVert_t> &v = surf.m_verts;
        std::vector<GLushort> &ind = surf.m_indices;
    
    
        v.push_back( drawVert_t( idVec3( -m_extents[0], -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0], -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 1, 1 ) ) );
    
        v.push_back( drawVert_t( idVec3( -m_extents[0], -m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0], -m_extents[1], m_extents[2] ),
                    idVec2( 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1], m_extents[2] ),
                    idVec2( 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0],  m_extents[1], m_extents[2] ),
                    idVec2( 1, 1 ) ) );
    
        for ( i = 0; i < 6*2; ++i ) {
            for ( j = 0; j < 3; ++j ) {
                ind.push_back( cinds[i][j] );
            }
        }
    
        surf.m_matName = "images/checkerboard.tga";

        gl_render.CacheSurface( surf, m_surf );
    }

    virtual void Think( int ms ) {}
    virtual void Render( void );

private:
    idVec3 m_extents;
    cached_surf_t m_surf;
};

class MyQuad : public MyEntity {
public:
    virtual void Spawn( void ) {
        m_pos = vec3_origin;
        m_axis = mat3_identity;

        Precache();
    }

    virtual void Precache( void ) {
        int i, j;
        int cinds[2][3] = {
        { 1, 2, 0 },
        { 2, 3, 0 },
        };

        surf_t surf;
        std::vector<drawVert_t> &v = surf.m_verts;
        std::vector<GLushort> &ind = surf.m_indices;
    
        v.push_back( drawVert_t( idVec3( 0, -10, -10 ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  0, -10, 10 ),
                    idVec2( 0, 1 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  0, 10, 10 ),
                    idVec2( 1, 1 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  0, 10, -10 ),
                    idVec2( 1, 0 ), idVec3( 0, 1, 0 ) ) );

        for ( i = 0; i < 2; ++i ) {
            for ( j = 0; j < 3; ++j ) {
                ind.push_back( cinds[i][j] );
            }
        }
    
        //surf.m_matName = "white";
        surf.m_matName = "q3shaders/base_floor.q3a";

        gl_render.CacheSurface( surf, m_surf );
    }

    virtual void Think( int ms ) {}
    virtual void Render( void );

private:
    cached_surf_t m_surf;
};

void MyQuad::Render( void ) {
    glsurf_t surf;

    /* transported automatically */
    surf.m_modelMatrix = idMat4( m_axis, m_pos );
    surf.m_surf = m_surf;

    gl_render.AddSurface( surf );
}

class MyGame {
public:
    void RunFrame( int ms ) {
        int i;
        for ( i = 0; i < m_ents.size(); ++i ) {
            m_ents[i]->Think( ms );
        }
    }

    void Render( void ) {
        int i;
        for ( i = 0; i < m_ents.size(); ++i ) {
            m_ents[i]->Render();
        }
    }

    void Spawn( void ) {
        int i;
        for ( i = 0; i < m_ents.size(); ++i ) {
            m_ents[i]->Spawn();
        }
    }

    void AddEntity( MyEntity &ent ) {
        m_ents.push_back( &ent );
    }

private:
    std::vector<MyEntity *> m_ents;
};

static MyGame gl_game;

static void Quit( int code )
{
    idLib::ShutDown();

    /*
     * Quit SDL so we can release the fullscreen
     * mode and restore the previous video settings,
     * etc.
     */
    SDL_Quit( );

    /* Exit program. */
    exit( code );
}

struct mousePosition_t {
    mousePosition_t()
        : m_x(0), m_y(0)
    {}

    Uint16 m_x;
    Uint16 m_y;
};

idMat3 GetRotationOZ( float angle ) {
    idMat3 rotationOZ( 
        idMath::Cos( angle ), -idMath::Sin( angle ), 0,
        idMath::Sin( angle ), idMath::Cos( angle ), 0,
        0, 0, 1);
    return rotationOZ;
}

idMat3 GetRotationOX( float angle ) {
    idMat3 rotationOX( 
        1, 0, 0,
        0, idMath::Cos( angle ), -idMath::Sin( angle ),
        0, idMath::Sin( angle ), idMath::Cos( angle ) );
    return rotationOX;
}

idMat3 GetRotationOY( float angle ) {
    idMat3 rotationOY( 
        idMath::Cos( angle ), 0, idMath::Sin( angle ),
        0, 1, 0,
        -idMath::Sin( angle ), 0, idMath::Cos( angle ) );
    return rotationOY;
}

idVec3 GetForwardFromAngles( float phi, float mu ) {
    idVec3 res;
    res.x = idMath::Sin( mu ) * idMath::Cos( phi );
    res.y = idMath::Sin( mu ) * idMath::Sin( phi );
    res.z = idMath::Cos( mu );
    return res;
}

struct KeyState {
    KeyState() { std::fill( m_keys, m_keys + 256, false ); }

    bool m_keys[256];
};

static void ProcessEvents( void )
{
    /* Our SDL event placeholder. */
    SDL_Event event;
    static mousePosition_t lastMousePos;
    int deltaX, deltaY;

    const float stepSize = 1.0f;
    const float angleStepSize = 0.2f; // degrees
    const float angleDegrees = 5.0f;
    const float angleKeyStepSize = DEG2RAD(angleDegrees); // degrees

    static KeyState kState[2];

    kState[1] = kState[0];

    /* Grab all the events off the queue. */
    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            kState[ 1 ].m_keys[ (int)event.key.keysym.sym ] = 1;
            break;

        case SDL_KEYUP:
            kState[ 1 ].m_keys[ (int)event.key.keysym.sym ] = 0;
            break;
        };

        switch( event.type ) {
        /*
        case SDL_KEYDOWN:
            switch ( event.key.keysym.sym ) {
            case SDLK_w:
                //v.m_pos += 10 * stepSize * v.m_axis[0];
                gl_camera.MoveForward();
            break;

            case SDLK_s:
                //v.m_pos -= 10 * stepSize * v.m_axis[0];
                gl_camera.MoveBackward();
            break;

            case SDLK_a:
                //v.m_axis = GetRotationOZ( angleKeyStepSize ) * v.m_axis;
                //phi += angleKeyStepSize;
                //v.m_axis[0] *= idRotation( idVec3( 0, 0, 0 ), idVec3( 0, 0, 1 ),
                        //angleDegrees );
                gl_camera.StrafeLeft();
            break;

            case SDLK_d:
                //v.m_axis = GetRotationOZ( -angleKeyStepSize ) * v.m_axis;
                //phi -= angleKeyStepSize;
                //v.m_axis[0] *= idRotation( idVec3( 0, 0, 0 ), idVec3( 0, 0, 1 ),
                        //-angleDegrees );
                gl_camera.StrafeRight();
            break;

            case SDLK_r:
                //v.m_axis = GetRotationOY( angleKeyStepSize ) * v.m_axis;
                //mu -= angleKeyStepSize;
                gl_camera.TurnLeft( angleDegrees );
            break;

            case SDLK_f:
                //v.m_axis = GetRotationOY( -angleKeyStepSize ) * v.m_axis;
                //mu += angleKeyStepSize;
                gl_camera.TurnRight( angleDegrees );
            break;

            case SDLK_t:
                //v.m_axis = GetRotationOX( angleKeyStepSize ) * v.m_axis;
                gl_camera.TurnUp( angleDegrees );
            break;

            case SDLK_g:
                //v.m_axis = GetRotationOX( -angleKeyStepSize ) * v.m_axis;
                gl_camera.TurnDown( angleDegrees );
            break;

            };
            break;
            */

        case SDL_MOUSEBUTTONDOWN:
            switch ( event.button.button ) {
            case SDL_BUTTON_WHEELUP:
                //v.m_pos += 0.25 * stepSize * v.m_axis[0];
                //kState[ 1 ].m_keys[ SDLK_w ] = 1;
                gl_camera.MoveForward();
            break;

            case SDL_BUTTON_WHEELDOWN:
                //v.m_pos -= 0.25 * stepSize * v.m_axis[0];
                gl_camera.MoveBackward();
            break;
            };
            break;

            /*
        case SDL_MOUSEBUTTONUP:
            switch ( event.button.button ) {
            case SDL_BUTTON_WHEELUP:
                //v.m_pos += 0.25 * stepSize * v.m_axis[0];
                kState[ 1 ].m_keys[ SDLK_w ] = 0;
            break;

            case SDL_BUTTON_WHEELDOWN:
                //v.m_pos -= 0.25 * stepSize * v.m_axis[0];
                kState[ 1 ].m_keys[ SDLK_s ] = 0;
            break;

            };
            break;
            */

        case SDL_MOUSEMOTION: {

            //deltaX = (int)event.motion.x - lastMousePos.m_x;
            //deltaY = (int)event.motion.y - lastMousePos.m_y;

            deltaX = event.motion.xrel;
            deltaY = event.motion.yrel;

            float xFrac = (float)deltaX / (float)gl_camera.GetPlayerView().m_width;
            float yFrac = (float)deltaY / (float)gl_camera.GetPlayerView().m_height;

            //float alphaX = DEG2RAD( xFrac * 180.0f );
            float alphaX = -50 * xFrac * 180.0f;
            //float alphaY = DEG2RAD( yFrac * 180.0f );

            //v.m_axis = GetRotationOY( alphaY ) * GetRotationOZ( alphaX ) * v.m_axis;
            //v.m_axis = GetRotationOZ( alphaX ) * v.m_axis;
            //phi -= alphaX;


            //printf( "%f\n", alphaX );
            //printf( "%f/%f/%f\n", v.m_pos[0], v.m_pos[1], v.m_pos[2] );

            if ( alphaX ) {
                gl_camera.TurnLeft( alphaX );
            }

            break;
        }

        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            Quit( 0 );
            break;
        };



        /*
        v.m_pos = idVec3(
            -50.f * idMath::Cos( phi ),
            -50.f * idMath::Sin( phi ),
            1 );

        v.m_axis[0] = -v.m_pos;
        v.m_axis[0].Normalize();

        //printf("%f,%f,%f\n", v.m_pos[0], v.m_pos[1], v.m_pos[2] );

        //v.m_axis[0] = GetForwardFromAngles( phi, mu );
        //v.m_axis[0].Normalize();

        */

        //v.m_axis[0].OrthogonalBasis( v.m_axis[1], v.m_axis[2] );

    }

        if ( kState[ 1 ].m_keys[ SDLK_w ] ) {
            gl_camera.MoveForward();
        }

        if ( kState[ 1 ].m_keys[ SDLK_s ] ) {
            gl_camera.MoveBackward();
        }

        if ( kState[ 1 ].m_keys[ SDLK_a ] ) {
            gl_camera.StrafeLeft();
        }

        if ( kState[ 1 ].m_keys[ SDLK_d ] ) {
            gl_camera.StrafeRight();
        }

        if ( kState[ 1 ].m_keys[ SDLK_r ] ) {
            gl_camera.TurnLeft( angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_f ] ) {
            gl_camera.TurnRight( angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_t ] ) {
            gl_camera.TurnUp( angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_g ] ) {
            gl_camera.TurnDown( angleDegrees );
        }

        kState[0] = kState[1];
}

void SetupOpengl( int width, int height ) {
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
        fprintf( stderr, "glewInit() != GLEW_OK\n" );
        Quit( 1 );
    }
}

void InitVideo() {
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
        /* Failed, exit. */
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
        Quit( 1 );
    }

    /* Let's get some video information. */
    info = SDL_GetVideoInfo( );

    if( !info ) {
        /* This should probably never happen. */
        fprintf( stderr, "Video query failed: %s\n",
             SDL_GetError( ) );
        Quit( 1 );
    }

    /*
     * Set our width/height to 640/480 (you would
     * of course let the user decide this in a normal
     * app). We get the bpp we will request from
     * the display. On X11, VidMode can't change
     * resolution, so this is probably being overly
     * safe. Under Win32, ChangeDisplaySettings
     * can change the bpp.
     */
    width = 640;
    height = 480;
    bpp = info->vfmt->BitsPerPixel;

    /*
     * Now, we want to setup our requested
     * window attributes for our OpenGL window.
     * We want *at least* 5 bits of red, green
     * and blue. We also want at least a 16-bit
     * depth buffer.
     *
     * The last thing we do is request a double
     * buffered window. '1' turns on double
     * buffering, '0' turns it off.
     *
     * Note that we do not use SDL_DOUBLEBUF in
     * the flags to SDL_SetVideoMode. That does
     * not affect the GL attribute state, only
     * the standard 2D blitting setup.
     */
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    /*
     * We want to request that SDL provide us
     * with an OpenGL window, in a fullscreen
     * video mode.
     *
     * EXERCISE:
     * Make starting windowed an option, and
     * handle the resize events properly with
     * glViewport.
     */
    flags = SDL_OPENGL;

    /*
     * Set the video mode
     */
    if( SDL_SetVideoMode( width, height, bpp, flags ) == 0 ) {
        /* 
         * This could happen for a variety of reasons,
         * including DISPLAY not being set, the specified
         * resolution not being available, etc.
         */
        fprintf( stderr, "Video mode set failed: %s\n",
             SDL_GetError( ) );
        Quit( 1 );
    }

    SDL_ShowCursor( 0 );

    /*
     * At this point, we should have a properly setup
     * double-buffered window for use with OpenGL.
     */
    SetupOpengl( width, height );
}

void MyFloor::Render( void ) {
    glsurf_t surf;

    surf.m_modelMatrix = idMat4( m_axis, m_pos );
    surf.m_surf = m_surf;

    gl_render.AddSurface( surf );
}

void RenderVideo( void ) {
    gl_game.Render();
    gl_render.Render( gl_camera.GetPlayerView() );

    SDL_GL_SwapBuffers( );
}

class MyShotgun: public GLRenderModelMD3 {
public:
    MyShotgun() : GLRenderModelMD3( "models/shotgun.md3", "images/shotgun.tga" )
    {
        m_jumpInterval = 2000;
        m_elapsedTime = 0;
        m_isUp = true;
    }

    void Think( int ms ) {
        this->GLRenderModelMD3::Think( ms );

        if ( m_elapsedTime > m_jumpInterval ) {
            m_isUp = !m_isUp;
            m_elapsedTime -= m_jumpInterval;
        }

        m_axis[0] *= idRotation( m_pos, idVec3( 0, 0, 1 ), 0.025f );
        m_axis[0].OrthogonalBasis( m_axis[1], m_axis[2] );

        m_pos[2] += m_isUp ? 0.01f : -0.01f;

        m_elapsedTime += ms;
    }

private:
    int m_jumpInterval;
    int m_elapsedTime;
    bool m_isUp;
};

class MySky: public MyEntity {
public:
    virtual void Spawn( void ) {
        m_pos = vec3_origin;
        m_axis = mat3_identity;
        m_extents = idVec3( 1000, 1000, 1000 );

        Precache();
    }

    virtual void Precache( void ) {
        int i, j;
        int cinds[6*2][3] = {
        { 0, 1, 2 },
        { 1, 3, 2 },
        { 4+0, 4+1, 4+2 },
        { 4+1, 4+3, 4+2 },
        { 8+0, 8+1, 8+2 },
        { 8+1, 8+3, 8+2 },
        { 8+4+0, 8+4+1, 8+4+2 },
        { 8+4+1, 8+4+3, 8+4+2 },
        { 16+0, 16+1, 16+2 },
        { 16+1, 16+3, 16+2 },
        { 16+4+0, 16+4+1, 16+4+2 },
        { 16+4+1, 16+4+3, 16+4+2 },
        };

        surf_t surf;
        std::vector<drawVert_t> &v = surf.m_verts;
        std::vector<GLushort> &ind = surf.m_indices;
    
    
        v.push_back( drawVert_t( idVec3( -m_extents[0], -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0], -m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );

        v.push_back( drawVert_t( idVec3( m_extents[0], -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0], -m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],  m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );

        v.push_back( drawVert_t( idVec3( -m_extents[0],  -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],   -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  -m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],   -m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );

        v.push_back( drawVert_t( idVec3( -m_extents[0],   m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],   m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    m_extents[1],  m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );

        v.push_back( drawVert_t( idVec3( -m_extents[0],   -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    -m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],   m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );

        v.push_back( drawVert_t( idVec3( -m_extents[0],   -m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    -m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 1 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],   m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 0 ) ) );
        v.push_back( drawVert_t( idVec3( m_extents[0],    m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ), idVec3( 0, 1, 1 ) ) );
    
        for ( i = 0; i < 6 * 2; ++i ) {
            for ( j = 0; j < 3; ++j ) {
                ind.push_back( cinds[i][j] );
            }
        }
    
        surf.m_matName = "sky";

        gl_render.CacheSurface( surf, m_surf );
    }

    virtual void Think( int ms ) {}
    virtual void Render( void ) {
        glsurf_t surf;

        surf.m_modelMatrix = idMat4( m_axis, m_pos );
        surf.m_surf = m_surf;

        gl_render.AddSurface( surf );
    }

private:
    idVec3 m_extents;
    cached_surf_t m_surf;
};

int main() {
    int oldMs, curMs;
    //light_t light;

    idLib::Init();

    MySky sky;
    gl_game.AddEntity( sky );

    MyFloor floor;
    gl_game.AddEntity( floor );

    /*
    MyShotgun shotgun;
    gl_game.AddEntity( shotgun );
    */

    MyQuad tquad;
    gl_game.AddEntity( tquad );

    InitVideo();

    gl_render.Init();


    /*
    GLTextureCube cubeTexture;
    cubeTexture.Init( "images/cloudy", 2 );
    */

    gl_game.Spawn();

    oldMs = idLib::Milliseconds();

    oldMs -= 10;

    while( 1 ) {
        curMs = idLib::Milliseconds();

        ProcessEvents();
        gl_game.RunFrame( curMs - oldMs );

        RenderVideo();

        oldMs = curMs;
    }

    gl_render.Shutdown();

    idLib::ShutDown();
    return 0;
}

