#include "d3lib/Lib.h"

#include <SDL/SDL.h>
#include <vector>
#include <string>

#include "Utils.h"

#include "Common.h"

#include "Model_md3.h"

#include "Camera.h"

#include "Render.h"

#include "OdePhysics.h"

#include "PhysicalEntity.h"

#include "Q3Material.h"

#include "Q3Map.h"

static int oldMs, curMs;

class MyBox: public MyPhysicalEntity  {
public:
    MyBox( const Map &m = Map() )
        : MyPhysicalEntity( m )
    {}

    virtual void Spawn( void ) {
        m_extents = idVec3( 2, 2, 2 );
        MyPhysicalEntity::Spawn();
        m_body.SetAsBox( m_extents, 1.0f );
    }

    virtual void Precache( void ) {
        int i, j;
        int cinds[6*2][3] = {
        { 7, 6, 2 },
        { 3, 7, 2 },
        { 5, 7, 3 },
        { 1, 5, 3 },
        { 4, 5, 1 },
        { 4, 1, 0 },
        { 6, 4, 0 },
        { 6, 0, 2 },
        { 5, 4, 6 },
        { 7, 5, 6 },
        { 0, 1, 3 },
        { 2, 0, 3 },
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
        //surf.m_matName = "images/2d/crosshaira.tga"; //checkerboard.tga";
        //surf.m_matName = "images/crosshair.tga"; //checkerboard.tga";

        gl_render.CacheSurface( surf, m_surf );
    }

    virtual void Render( void );

private:
    idVec3 m_extents;
    cached_surf_t m_surf;
};

class MyFloor: public MyPhysicalEntity {
public:
    MyFloor( const Map &m = Map() )
        : MyPhysicalEntity( m )
    {}

    virtual void Spawn( void ) {
        m_extents = idVec3( 100, 100, 10 );
        MyPhysicalEntity::Spawn();
        m_body.SetAsBox( m_extents, 1.0f );
        m_body.SetStatic();
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

    virtual void Render( void );

private:
    idVec3 m_extents;
    cached_surf_t m_surf;
};

void MyBox::Render( void ) {
    glsurf_t surf;

    surf.m_modelMatrix = idMat4( m_axis, m_pos );
    surf.m_surf = m_surf;

    gl_render.AddSurface( surf );
}

class MyQuad : public MyEntity {
public:

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
    
        surf.m_matName = "white";
        //surf.m_matName = "q3shaders/base_floor.q3a";

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

static void ProcessEvents( int ms )
{
    /* Our SDL event placeholder. */
    SDL_Event event;
    static mousePosition_t lastMousePos;
    int deltaX, deltaY;

    playerView_t pView( gl_camera.GetPlayerView() );

    const float stepSize = 1.0f;
    const float angleStepSize = 0.2f; // degrees
    /*
    const float angleDegrees = 5.0f;
    const float angleKeyStepSize = DEG2RAD(angleDegrees); // degrees
    */

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

        case SDL_KEYDOWN:
            switch ( event.key.keysym.sym ) {
            case SDLK_q: {
                Quit(0);
                break;
            }

            case SDLK_h: {
                gl_camera.NormalizeView();
                break;
            }};
            break;

        case SDL_MOUSEBUTTONDOWN:
            switch ( event.button.button ) {
            case SDL_BUTTON_WHEELUP:
                //v.m_pos += 0.25 * stepSize * v.m_axis[0];
                //kState[ 1 ].m_keys[ SDLK_w ] = 1;
                //gl_camera.MoveForward();
            break;

            case SDL_BUTTON_WHEELDOWN:
                //v.m_pos -= 0.25 * stepSize * v.m_axis[0];
                //gl_camera.MoveBackward();
            break;

            case SDL_BUTTON_LEFT: {

                Map m;
                m[ "pos" ] = ( pView.m_pos + GetForwardVector( pView.m_axis ) * 5 ).ToString();
                m[ "linear_velocity" ] = ( 20 * GetForwardVector( pView.m_axis ) ).ToString();
                m[ "angular_velocity" ] = ( 5 * ( (float)rand() / RAND_MAX ) 
                        * pView.m_axis[1] ).ToString();

                MyBox *b = new MyBox( m );
                gl_game.AddEntity( *b );
                b->Spawn();

                break;
            }

            case SDL_BUTTON_RIGHT:
                gl_camera.NormalizeView();
                break;
            };
            break;

        case SDL_MOUSEMOTION: {

            //deltaX = (int)event.motion.x - lastMousePos.m_x;
            //deltaY = (int)event.motion.y - lastMousePos.m_y;

            deltaX = event.motion.xrel;
            deltaY = event.motion.yrel;

            /*
            float xFrac = (float)deltaX / (float)gl_camera.GetPlayerView().m_width;
            float yFrac = (float)deltaY / (float)gl_camera.GetPlayerView().m_height;
            */

            //float alphaX = DEG2RAD( xFrac * 180.0f );
            //float alphaX = -50 * xFrac * 180.0f;
            //float alphaY = DEG2RAD( yFrac * 180.0f );

            //v.m_axis = GetRotationOY( alphaY ) * GetRotationOZ( alphaX ) * v.m_axis;
            //v.m_axis = GetRotationOZ( alphaX ) * v.m_axis;
            //phi -= alphaX;


            //printf( "%f\n", alphaX );
            //printf( "%f/%f/%f\n", v.m_pos[0], v.m_pos[1], v.m_pos[2] );

            /*
            if ( alphaX ) {
                gl_camera.TurnLeft( alphaX );
            }
            */

            gl_camera.Yaw( deltaX * 0.15f );
            gl_camera.Pitch( deltaY * 0.15f );

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

    idVec3 accel( 0, 0, 0 );
    const float velocity = 80.0f;
    const float angleDegrees = 1.f;
    float moveCoeff = velocity * ( (float)ms / 1000.0f );

        if ( kState[ 1 ].m_keys[ SDLK_w ] ) {
            accel += gl_camera.MoveForward( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_s ] ) {
            accel += gl_camera.MoveBackward( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_a ] ) {
            accel += gl_camera.MoveLeft( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_d ] ) {
            accel += gl_camera.MoveRight( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_r ] ) {
            accel += gl_camera.MoveUp( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_f ] ) {
            accel += gl_camera.MoveDown( moveCoeff );
        }

        if ( kState[ 1 ].m_keys[ SDLK_t ] ) {
            gl_camera.Yaw( -angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_g ] ) {
            gl_camera.Yaw( angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_u ] ) {
            gl_camera.Pitch( -angleDegrees );
        }

        if ( kState[ 1 ].m_keys[ SDLK_j ] ) {
            gl_camera.Pitch( angleDegrees );
        }

        /*
        if ( kState[ 1 ].m_keys[ SDLK_r ] ) {
            accel += gl_camera.TurnLeft( moveCoeff );
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
        */

        gl_camera.Move( accel );

        kState[0] = kState[1];
}

void SetupOpengl( int width, int height ) {
    /* Culling. */
    /*
    glCullFace( GL_BACK );
    glFrontFace( GL_CW );
    glDisable( GL_CULL_FACE );
    */
    
    /*
    glCullFace( GL_BACK );
    glFrontFace( GL_CW );
    glEnable( GL_CULL_FACE );
    */

    glClearColor( 0, 0, 0, 0 );

    glViewport( 0, 0, width, height );

    //glDisable( GL_CULL_FACE );
    //glDisable( GL_DEPTH_TEST );
    //glEnable( GL_DEPTH_TEST );

    glEnable( GL_DEPTH_TEST );
    glDepthFunc(GL_LESS);

    glDisable( GL_STENCIL_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    idMat3 m = mat3_identity;
    //m[0] *= -1;
    gl_camera.Init( idVec3( 19.6, 2.6, 2.38 ), m );

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

    /*
    SDL_ShowCursor( 0 );

    SDL_WM_GrabInput( SDL_GRAB_ON );
    */

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
        m_jumpInterval = 1000;
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
        m_extents = idVec3( 1000, 1000, 1000 );

        MyEntity::Spawn();
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

class MyPostProcessingQuad: public MyEntity {
public:

    virtual void Precache( void ) {
        int i, j;

        surf_t surf;
        std::vector<drawVert_t> &v = surf.m_verts;
        std::vector<GLushort> &ind = surf.m_indices;
    
        v.push_back( drawVert_t( idVec3( -1,  -1, 0 ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  1,  -1, 0 ),
                    idVec2( 1, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -1,   1, 0 ),
                    idVec2( 0, 1 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  1,   1, 0 ),
                    idVec2( 1, 1 ), idVec3( 0, 0, 0 ) ) );

        ind.push_back( 0 );
        ind.push_back( 1 );
        ind.push_back( 2 );

        ind.push_back( 1 );
        ind.push_back( 3 );
        ind.push_back( 2 );
    
        surf.m_matName = "postprocess";

        gl_render.CacheSurface( surf, m_surf );
    }

    void Think( int ) {}

    virtual void Render( void ) {
        glsurf_t surf;

        surf.m_modelMatrix = idMat4( m_axis, m_pos );
        surf.m_surf = m_surf;

        gl_render.AddSurface( surf );
    }

private:
    cached_surf_t m_surf;
};

class MyCrosshair: public MyEntity {
public:

    virtual void Precache( void ) {
        int i, j;

        surf_t surf;
        std::vector<drawVert_t> &v = surf.m_verts;
        std::vector<GLushort> &ind = surf.m_indices;
    
        const float kScale = 0.01;
        v.push_back( drawVert_t( idVec3( -kScale,  -kScale, 0 ),
                    idVec2( 0, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  kScale,  -kScale, 0 ),
                    idVec2( 1, 0 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -kScale,   kScale, 0 ),
                    idVec2( 0, 1 ), idVec3( 0, 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  kScale,   kScale, 0 ),
                    idVec2( 1, 1 ), idVec3( 0, 0, 0 ) ) );

        ind.push_back( 0 );
        ind.push_back( 1 );
        ind.push_back( 2 );

        ind.push_back( 1 );
        ind.push_back( 3 );
        ind.push_back( 2 );
    
        surf.m_matName = "crosshair";

        gl_render.CacheSurface( surf, m_surf );
    }

    void Think( int ) {}

    virtual void Render( void ) {
        glsurf_t surf;

        surf.m_modelMatrix = idMat4( m_axis, m_pos );
        surf.m_surf = m_surf;

        gl_render.AddSurface( surf );
    }

private:
    cached_surf_t m_surf;
};

class LegsAnim : public GLRenderModelMD3 {
public:
    LegsAnim()
        : GLRenderModelMD3( "models/lower.md3", "images/crash.tga" ),
        m_prevMs( 0 )
    {
        RegisterAnim( TORSO_ATTACK, 40, 46, 15 );
    }

    virtual void Spawn( void ) {
        m_pos = vec3_origin;
        m_axis = mat3_identity;

        Precache();
    }

    void Think( int ms ) {
        this->GLRenderModelMD3::Think( ms );

        m_prevMs += ms;

        if ( m_prevMs > 5000 ) {
            PlayAnim( TORSO_ATTACK );

            m_prevMs = 0;
        }
    }

private:
    int m_prevMs;
};

int main() {
    //light_t light;

    idLib::Init();

    gl_physics.Init();

    Q3Material::ScanShaders();

    MySky sky;
    gl_game.AddEntity( sky );

    /*
    MyFloor floor;
    gl_game.AddEntity( floor );
    */

    //lightPos = idVec4( -50, 50, 50, 1 );
    //lightDir = idVec4( 0, 0, 0, 1 ) - lightPos;

    /*
    GLDirectionLight light( 
            ToWorld( lightPos ), ToWorld( lightDir ) );
    gl_render.AddLight( &light );
    */

    /*
    MyQuad mq;
    gl_game.AddEntity( mq );
    */

    //GLRenderModelMD3 ammo( "models/shotgunam.md3" );

    Map dict;

    /*
    dict["pos"] = idVec3( 0, 0, 20 ).ToString();

    GLRenderModelMD3 ammo( "models/machinegun.md3", "images/machinegun.tga", dict );
    gl_game.AddEntity( ammo );
    */

    /*
    LegsAnim legs;
    gl_game.AddEntity( legs );
    */

    /*
    MyShotgun shotgun;
    gl_game.AddEntity( shotgun );
    */

    /*
    MyQuad tquad;
    gl_game.AddEntity( tquad );

    GLRenderModelMD3 flag( "models/b_flag.md3", "images/b_flag2.tga" );
    gl_game.AddEntity( flag );
    */

    dict.clear();
    dict["map"] = "maps/q3dm0.bsp";

    Q3Map q3map( dict );
    gl_game.AddEntity( q3map );

    /*
    MyCrosshair mCrosshair;
    gl_game.AddEntity( mCrosshair );
    */

    MyPostProcessingQuad quad;
    gl_game.AddEntity( quad );

    InitVideo();

    gl_render.Init( gl_camera.GetPlayerView() );


    /*
    GLTextureCube cubeTexture;
    cubeTexture.Init( "images/cloudy", 2 );
    */

    gl_game.Spawn();

    //flag.SetPosition( idVec3( 50, 50, 10 ) );

    oldMs = idLib::Milliseconds();

    oldMs -= 10;

    while( 1 ) {
        curMs = idLib::Milliseconds();

        ProcessEvents( curMs - oldMs );
        gl_game.RunFrame( curMs - oldMs );
        gl_physics.RunFrame( curMs - oldMs );

        RenderVideo();

        oldMs = curMs;
    }

    gl_render.Shutdown();

    gl_physics.Shutdown();

    idLib::ShutDown();
    return 0;
}

