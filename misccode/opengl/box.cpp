#include "d3lib/Lib.h"

#include <SDL/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <vector>
#include <string>

#include "Utils.h"

#include "Model_md3.h"

#include "GLSLProgram.h"
#include "GLTexture.h"

static float flipMatrixData[4][4] = {
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { -1, 0, 0, 0 },
    { 0, 0, 0, 1 },
};

static const idMat4 flipMatrix( flipMatrixData );

static idVec3 ToWorld( idVec3 &v ) {
    return ( flipMatrix * idVec4( v, 1.0f ) ).ToVec3();
}

struct material_t {
    GLTexture *m_matPtr;
};

struct __attribute__((packed)) drawVert_t {
    drawVert_t() {}

    drawVert_t( const idVec3 &p, const idVec2 &t )
    {
        VectorCopy( p, m_pos );

        m_tex[0] = t[0];
        m_tex[1] = t[1];
    }

    drawVert_t( const idVec3 &p, const idVec2 &t, const idVec3 &normal )
    {
        VectorCopy( p, m_pos );

        m_tex[0] = t[0];
        m_tex[1] = t[1];

        VectorCopy( normal, m_normal );
    }

    float m_pos[3];
    float m_tex[2];
    float m_normal[3];
};

struct surf_t {
    std::vector<drawVert_t> m_verts;
    std::vector<GLushort> m_indices;
    std::string m_matName;
};

struct cached_surf_t {
    GLuint m_vao;
    int m_numIndices;
    int m_indexBuffer;
    material_t m_material;
};

struct glsurf_t {
    idMat4 m_modelMatrix;
    cached_surf_t m_surf;
};

struct playerView_t {
    idVec3 m_pos;
    idMat3 m_axis;

    float m_fovx, m_fovy;

    float m_znear, m_zfar;
    int m_width, m_height;
};

class MyRender {
public:
    void Init( void );
    void Render( const playerView_t &view );
    void CacheSurface( const surf_t &s, cached_surf_t &cached );
    void AddSurface( const glsurf_t &s ) {
        m_surfs.push_back( s );
    }
    void AddLight( const GLLight &lt ) {
        m_lights.push_back( &lt );
    }
    void Shutdown( void );

private:
    void SetupViewMatrix( const playerView_t &view );
    void SetupProjectionMatrix( const playerView_t &view );
    void CreateStandardShaders( void );
    void CreateShaderProgram( void );
    void RenderSurface( const glsurf_t &surf );

    std::vector<glsurf_t> m_surfs;
    std::vector<GLLight *> m_lights;

    idVec4 m_eye; /* eye-position */

    GLTexture m_whiteTexture;
    GLTexture m_logoTexture;

    GLSLProgram m_shaderProgram;

    GLuint m_modelViewProjLocation;

    idMat4 m_viewMatrix;
    idMat4 m_projectionMatrix;
};

void MyRender::SetupViewMatrix( const playerView_t &view ) {
    idMat4 res( view.m_axis.Transpose(), idVec3( 0, 0, 0 ) );
    idVec4 pos( view.m_pos[0], view.m_pos[1], view.m_pos[2], 0.0f );

    res[0][3] = - res[0][0] * pos[0] + - res[1][0] * pos[1] + - res[2][0] * pos[2];
    res[1][3] = - res[0][1] * pos[0] + - res[1][1] * pos[1] + - res[2][1] * pos[2];
    res[2][3] = - res[0][2] * pos[0] + - res[1][2] * pos[1] + - res[2][2] * pos[2];
    res[3][3] = 1;

    m_viewMatrix = res;
}

void MyRender::SetupProjectionMatrix( const playerView_t &view ) {
    idMat4 &m = m_projectionMatrix;

    float znear, zfar;

    znear = view.m_znear;
    zfar = view.m_zfar;

	float ymax = znear * tan( view.m_fovy * idMath::PI / 360.0f );
	float ymin = -ymax;
	
	float xmax = znear * tan( view.m_fovx * idMath::PI / 360.0f );
	float xmin = -xmax;
	
	const float width = xmax - xmin;
	const float height = ymax - ymin;

	float depth = zfar - znear;

    m[0][0] = 2 * znear / width;
    m[0][1] = 0;
    m[0][2] = ( xmax + xmin ) / width;
    m[0][3] = 0;

    m[1][0] = 0;
    m[1][1] = 2 * znear / height;
    m[1][2] = ( ymax + ymin ) / height;
    m[1][3] = 0;

    m[2][0] = 0;
    m[2][1] = 0;
    m[2][2] = - ( zfar + znear ) / depth;
    m[2][3] = -2 * zfar * znear / depth;

    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = -1.0f;
    m[3][3] = 0;
}

void MyRender::CreateStandardShaders( void ) {
    byte white[16] = 
        { 0xFF, 0x00, 0xFF, 0xFF,
          0xFF, 0x00, 0x00, 0xFF,
          0x00, 0xFF, 0x00, 0xFF,
          0x00, 0x00, 0xFF, 0xFF,
        };

    if ( !m_whiteTexture.Init( white, 2, 2, GL_RGBA, 0 ) ) {
        exit(1);
    }

    if ( !m_logoTexture.Init( "images/logo1.tga", 1 ) ) {
        exit(1);
    }
}

void MyRender::CreateShaderProgram( void ) {
    std::vector<std::string> shaderList;

    shaderList.push_back( "shaders/simple.vs.glsl" );
    shaderList.push_back( "shaders/simple.ps.glsl" );

    assert( m_shaderProgram.Init( shaderList ) );
    assert( m_shaderProgram.IsOk() );
}

void MyRender::CacheSurface( const surf_t &s, cached_surf_t &cached ) {
    GLuint vao;
    GLuint positionBuffer, indexBuffer;

    _CH(glGenVertexArrays( 1, &vao ));
    _CH(glBindVertexArray( vao ));

    _CH(glGenBuffers( 1, &positionBuffer ));
    _CH(glBindBuffer( GL_ARRAY_BUFFER, positionBuffer ));
    _CH(glBufferData( GL_ARRAY_BUFFER, s.m_verts.size() * 8 * sizeof(float),
        (void *)s.m_verts.data(), GL_STATIC_DRAW ));

    _CH(glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(0 * sizeof(float ) ) ));
    _CH(glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(3 * sizeof(float ) ) ));
    _CH(glVertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(5 * sizeof(float ) ) ));

    _CH(glEnableVertexAttribArray( 0 ));
    _CH(glEnableVertexAttribArray( 1 ));
    _CH(glEnableVertexAttribArray( 2 ));

    _CH(glGenBuffers( 1, &indexBuffer ));
    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer ));
    _CH(glBufferData( GL_ELEMENT_ARRAY_BUFFER, s.m_indices.size() * sizeof(GLushort),
        (void *)s.m_indices.data(), GL_STATIC_DRAW ));

    cached.m_vao = vao;
    cached.m_numIndices = s.m_indices.size();
    cached.m_indexBuffer = indexBuffer;

    if ( s.m_matName == "white" ) {
        cached.m_material.m_matPtr = &m_whiteTexture;
    } else if ( s.m_matName == "logo" ) {
        cached.m_material.m_matPtr = &m_logoTexture;
    } else {
        assert( 0 );
    }
}

void MyRender::Init( void ) {
    CreateStandardShaders();
    CreateShaderProgram();
}

void MyRender::Shutdown( void ) {
}

void MyRender::RenderSurface( const glsurf_t &surf ) {
    int i;
    idMat4 mvpMatrix;

    mvpMatrix = m_projectionMatrix * flipMatrix 
        * m_viewMatrix * surf.m_modelMatrix;

    /*
    printVector( flipMatrix * idVec4( 0, 10, 10, 1 ) );
    printVector( surf.m_modelMatrix * flipMatrix * idVec4( 0, 10, 10, 1 ) );
    printVector( m_viewMatrix * surf.m_modelMatrix * flipMatrix * idVec4( 0, 10, 10, 1 ) );
    printVector( mvpMatrix * idVec4( 0, 10, 10, 1 ) );
    */

    //printProjectedVector( mvpMatrix * idVec4( 0, -10, -10, 1 ) );
    //printProjectedVector( mvpMatrix * idVec4( 0, 0, 10, 1 ) );
    //printProjectedVector( mvpMatrix * idVec4( 0, 10, 10, 1 ) );

    m_shaderProgram.Use();
    m_shaderProgram.Bind( "mvp_matrix", mvpMatrix.Transpose() );

    if ( !m_lights.empty() ) {
        for ( i = 0; i < m_lights.size(); ++i ) {
            m_shaderProgram.Bind( m_lights[i]->GetName(), m_lights[i] );
        }
    }

    m_shaderProgram.Bind( "eye_pos", ToWorld( m_eye ) );

    _CH(glBindVertexArray( surf.m_surf.m_vao ));

    surf.m_surf.m_material.m_matPtr->Bind();

    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, surf.m_surf.m_indexBuffer ));

    _CH(glDrawElements( GL_TRIANGLES, 
                surf.m_surf.m_numIndices, GL_UNSIGNED_SHORT, 0 ));

    surf.m_surf.m_material.m_matPtr->Unbind();
}

void MyRender::Render( const playerView_t &view ) {
    int i;

    m_eye = idVec4( view.m_pos, 1.0f );

    SetupViewMatrix( view );
    SetupProjectionMatrix( view );

    _CH(glViewport( 0, 0, view.m_width, view.m_height ));
    _CH(glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ));

    for ( i = 0; i < m_surfs.size(); ++i ) {
        RenderSurface( m_surfs[i] );
    }

    m_surfs.clear();
}

static MyRender gl_render;
static playerView_t gl_playerView;

class MyEntity {
public:
    virtual ~MyEntity() {}

    virtual void Spawn( void ) = 0;
    virtual void Precache( void ) = 0;
    virtual void Think( int ms ) = 0;
    virtual void Render( void ) = 0;

protected:
    idVec3 m_pos;
    idMat3 m_axis;
};

class MyFloor: public MyEntity {
public:

    virtual void Spawn( void ) {
        m_pos = vec3_origin;
        m_axis = mat3_identity;
        m_extents = idVec3( 10, 10, 10 );

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
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0],  m_extents[1], -m_extents[2] ),
                    idVec2( 0, 0 ) ) );
    
        v.push_back( drawVert_t( idVec3( -m_extents[0], -m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0], -m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3( -m_extents[0],  m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  m_extents[0],  m_extents[1], m_extents[2] ),
                    idVec2( 0, 0 ) ) );
    
        for ( i = 0; i < 6*2; ++i ) {
            for ( j = 0; j < 3; ++j ) {
                ind.push_back( cinds[i][j] );
            }
        }
    
        surf.m_matName = "white";

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
    
        surf.m_matName = "white";

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

static void ProcessEvents( void )
{
    /* Our SDL event placeholder. */
    SDL_Event event;
    static mousePosition_t lastMousePos;
    int deltaX, deltaY;
    playerView_t &v = gl_playerView;

    const float stepSize = 1.0f;
    const float angleStepSize = 0.2f; // degrees
    const float angleDegrees = 22.5f;
    const float angleKeyStepSize = DEG2RAD(angleDegrees); // degrees

    /* Grab all the events off the queue. */
    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            switch ( event.key.keysym.sym ) {
            case SDLK_w:
                v.m_pos += stepSize * v.m_axis[0];
            break;

            case SDLK_s:
                v.m_pos -= stepSize * v.m_axis[0];
            break;

            case SDLK_a:
                v.m_axis = GetRotationOZ( angleKeyStepSize ) * v.m_axis;
            break;

            case SDLK_d:
                v.m_axis = GetRotationOZ( -angleKeyStepSize ) * v.m_axis;
            break;

            case SDLK_r:
                v.m_axis = GetRotationOY( angleKeyStepSize ) * v.m_axis;
            break;

            case SDLK_f:
                v.m_axis = GetRotationOY( -angleKeyStepSize ) * v.m_axis;
            break;

            case SDLK_t:
                v.m_axis = GetRotationOX( angleKeyStepSize ) * v.m_axis;
            break;

            case SDLK_g:
                v.m_axis = GetRotationOX( -angleKeyStepSize ) * v.m_axis;
            break;

            };
            break;

        case SDL_MOUSEBUTTONDOWN:
            switch ( event.button ) {
            case SDL_BUTTON_WHEELUP:
                v.m_pos += 0.25 * stepSize * v.m_axis[0];
            break;

            case SDL_BUTTON_WHEELDOWN:
                v.m_pos -= 0.25 * stepSize * v.m_axis[0];
            break;
            };
            break;

        case SDL_MOUSEMOTION: {
            if ( lastMousePos.m_x == lastMousePos.m_y && lastMousePos.m_y == 0 ) {
                lastMousePos.m_x = event.motion.x;
                lastMousePos.m_y = event.motion.y;
                continue;
            }

            deltaX = (int)event.motion.x - lastMousePos.m_x;
            deltaY = (int)event.motion.y - lastMousePos.m_y;

            float xFrac = (float)deltaX / (float)v.m_width;
            float yFrac = (float)deltaY / (float)v.m_height;

            float alphaX = DEG2RAD( xFrac * angleDegrees );
            float alphaY = DEG2RAD( yFrac * angleDegrees );

            v.m_axis = GetRotationOY( alphaY ) 
                * GetRotationOZ( alphaX ) * v.m_axis;
            break;
        }

        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            Quit( 0 );
            break;
        }

    }
}

void SetupOpengl( int width, int height ) {
    /* Culling. */
    glCullFace( GL_BACK );
    glFrontFace( GL_CW );
    //glEnable( GL_CULL_FACE );

    glClearColor( 0, 0, 0, 0 );

    glViewport( 0, 0, width, height );

    //glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_STENCIL_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    // in local space
    gl_playerView.m_pos = idVec3( -50, 0, 0 ); 
    gl_playerView.m_axis = mat3_identity;

    gl_playerView.m_fovx = 74;
    gl_playerView.m_fovy = 74;

    gl_playerView.m_znear = 3.0f;
    gl_playerView.m_zfar = 10000.0f;

    gl_playerView.m_width = 640;
    gl_playerView.m_height = 480;

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
    gl_render.Render( gl_playerView );

    //draw_screen();

    SDL_GL_SwapBuffers( );
}

int main() {
    int oldMs, curMs;
    light_t light;

    idLib::Init();

    /*
    MyFloor floor;
    gl_game.AddEntity( floor );
    */

    lightPos = idVec4( -50, 50, 50, 1 );
    lightDir = idVec4( 0, 0, 0, 1 ) - lightPos;

    /*
    GLDirectionLight light( 
            ToWorld( lightPos ), ToWorld( lightDir ) );
    gl_render.AddLight( &light );
    */

    MyQuad quad;
    gl_game.AddEntity( quad );

    InitVideo();

    gl_render.Init();

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

