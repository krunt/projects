#include "d3lib/Lib.h"

#include <SDL/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <vector>
#include <string>

#include "Utils.h"

#define VERTEX_PROGRAM \
    "#version 410 core\n" \
    "layout (location = 0) in vec3 position;\n" \
    "layout (location = 1) in vec2 texcoord;\n" \
    "out VS_OUT\n" \
    "{\n" \
    " vec2 texcoord;\n" \
    "} vs_out;\n" \
    "\n" \
    "uniform mat4 mvp_matrix;\n" \
    "\n" \
    "void main(void)\n" \
    "{\n" \
    " gl_Position = mvp_matrix * vec4( position, 1.0f );\n" \
    " vs_out.texcoord = vec2(gl_Position.x, gl_Position.y);\n" \
    "}\n" \

    //"layout (location = 1) in vec2 texcoord;\n" \
    //" gl_Position = mvp_matrix * position;\n" 
    //" gl_Position = mvp_matrix * position;\n" \


#define FRAGMENT_PROGRAM \
    "#version 410 core\n" \
    "uniform sampler2D tex;\n" \
    "in VS_OUT\n" \
    "{\n" \
    " vec2 texcoord;\n" \
    "} fs_in;\n" \
    "out vec4 color;\n" \
    "\n" \
    "void main(void) {\n" \
    " color = vec4( 1.0f, 1.0f, 1.0f, 1.0f );\n" \
    "}\n" \

    //" color = vec4( 1.0f, 0.5f, 1.0f, 1.0f );\n" \
    //" color = texture(tex, fs_in.texcoord);\n" \

static void checkError() {
    int err, anyErr = 0;
    const char *s;
    while ( ( err = glGetError() ) != GL_NO_ERROR ) {
        switch ( err ) {
        case GL_INVALID_ENUM: s = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: s = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: s = "GL_INVALID_OPERATION"; break;
        default: s = "other"; break;
        };
        fprintf( stderr, "err: %s\n", s );
        anyErr = 1;
    }

    if ( anyErr ) {
        assert( 0 );
    }
}

#define _CH(x) \
    (x); checkError()

static void printVector( const idVec2 &vec ) {
    fprintf( stderr, "(%f/%f)\n", vec.x, vec.y );
}

static void printVector( const idVec3 &vec ) {
    fprintf( stderr, "(%f/%f/%f)\n", vec.x, vec.y, vec.z );
}

static void printVector( const idVec4 &vec ) {
    fprintf( stderr, "(%f/%f/%f/%f)\n", vec[0], vec[1], vec[2], vec[3] );
}

static void printProjectedVector( const idVec4 &vec ) {
    idVec4 v(vec);
    v /= vec[3];
    printVector(v);
}

struct material_t {
    GLuint m_matId;
};

struct __attribute__((packed)) drawVert_t {
    drawVert_t( const idVec3 &p, const idVec2 &t )
    {
        VectorCopy( p, m_pos );

        m_tex[0] = t[0];
        m_tex[1] = t[1];
    }

    float m_pos[3];
    float m_tex[2];
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
    void Shutdown( void );

private:
    void SetupViewMatrix( const playerView_t &view );
    void SetupProjectionMatrix( const playerView_t &view );
    void CreateStandardShaders( void );
    void CreateShaderProgram( void );
    void RenderSurface( const glsurf_t &surf );

    std::vector<glsurf_t> m_surfs;

    GLuint m_whiteTexture;

    GLuint m_shaderProgram;
    GLuint m_modelViewProjLocation;

    idMat4 m_viewMatrix;
    idMat4 m_projectionMatrix;
};

void MyRender::SetupViewMatrix( const playerView_t &view ) {
    m_viewMatrix = idMat4( view.m_axis, -view.m_pos );
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
    byte white[3] = { 0xFF, 0xFF, 0xFF };

    glGenTextures( 1, &m_whiteTexture );
    glBindTexture( GL_TEXTURE_2D, m_whiteTexture );
    glTexImage2D( GL_TEXTURE_2D, 
            0, /* level */
            GL_RGB8,  /* format */
            1,  /* w */
            1,  /* h */
            0,  /* border */
            GL_RGB,  /* format */
            GL_UNSIGNED_BYTE, /* type */
            white );

    glBindTexture( GL_TEXTURE_2D, 0 );
}

void MyRender::CreateShaderProgram( void ) {
    char compileMessage[4096];
    GLuint vs, fs; 
    GLint compileStatus, linkStatus;
    const char *vsSource[] = { VERTEX_PROGRAM };
    const char *fsSource[] = { FRAGMENT_PROGRAM };

    if ( ! ( m_shaderProgram = glCreateProgram() ) ) {
        msg_failure0( "error creating shader program\n" );
    }

    if ( ! ( vs = glCreateShader( GL_VERTEX_SHADER ) ) ) {
        msg_failure0( "error creating vertex shader\n" );
    }

    _CH(glShaderSource( vs, 1, vsSource, NULL ));
    _CH(glCompileShader( vs ));

    _CH(glGetShaderiv( vs, GL_COMPILE_STATUS, &compileStatus ));
    assert( compileStatus == GL_TRUE );

    /*
    glGetProgramInfoLog( vs, sizeof( compileMessage ),
            NULL, compileMessage );
            */

    if ( ! ( fs = glCreateShader( GL_FRAGMENT_SHADER ) ) ) {
        msg_failure0( "error creating fragment shader\n" );
    }

    _CH(glShaderSource( fs, 1, fsSource, NULL ));
    _CH(glCompileShader( fs ));

    _CH(glGetShaderiv( fs, GL_COMPILE_STATUS, &compileStatus ));
    assert( compileStatus == GL_TRUE );

    /*
    glGetProgramInfoLog( vs, sizeof( compileMessage ),
            NULL, compileMessage );
            */

    _CH(glAttachShader( m_shaderProgram, vs ));
    _CH(glAttachShader( m_shaderProgram, fs ));

    _CH(glLinkProgram( m_shaderProgram ));

    _CH(glGetProgramiv( m_shaderProgram, GL_LINK_STATUS, &linkStatus ));
    assert( linkStatus == GL_TRUE );

    _CH(glDeleteShader( vs ));
    _CH(glDeleteShader( fs ));

    m_modelViewProjLocation = glGetUniformLocation( m_shaderProgram, "mvp_matrix" );

    checkError();
}

void MyRender::CacheSurface( const surf_t &s, cached_surf_t &cached ) {
    GLuint vao;
    GLuint positionBuffer, indexBuffer;

    _CH(glGenVertexArrays( 1, &vao ));
    _CH(glBindVertexArray( vao ));

    _CH(glGenBuffers( 1, &positionBuffer ));
    _CH(glBindBuffer( GL_ARRAY_BUFFER, positionBuffer ));
    _CH(glBufferData( GL_ARRAY_BUFFER, s.m_verts.size() * 5 * sizeof(float),
        (void *)s.m_verts.data(), GL_STATIC_DRAW ));

    _CH(glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        (void *)(0 * sizeof(float ) ) ));
    _CH(glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        (void *)(3 * sizeof(float ) ) ));

    _CH(glEnableVertexAttribArray( 0 ));
    _CH(glEnableVertexAttribArray( 1 ));

    _CH(glGenBuffers( 1, &indexBuffer ));
    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer ));
    _CH(glBufferData( GL_ELEMENT_ARRAY_BUFFER, s.m_indices.size() * sizeof(GLushort),
        (void *)s.m_indices.data(), GL_STATIC_DRAW ));

    cached.m_vao = vao;
    cached.m_numIndices = s.m_indices.size();
    cached.m_indexBuffer = indexBuffer;

    if ( s.m_matName == "white" ) {
        cached.m_material.m_matId = m_whiteTexture;
    } else {
        assert( 0 );
    }
}

void MyRender::Init( void ) {
    CreateStandardShaders();
    CreateShaderProgram();
}

void MyRender::Shutdown( void ) {
    _CH(glDeleteTextures( 1, &m_whiteTexture ));
    _CH(glDeleteProgram( m_shaderProgram ));
}

void MyRender::RenderSurface( const glsurf_t &surf ) {
    float flipMatrixData[4][4] = {
        { 1, 0, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 0, 1 },
    };

    idMat4 mvpMatrix;
    const idMat4 flipMatrix( flipMatrixData );

    mvpMatrix = m_projectionMatrix * m_viewMatrix * surf.m_modelMatrix * flipMatrix;

    /*
    printProjectedVector( mvpMatrix * idVec4( 10, 0, -10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( -10, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 10, 0, 10, 1 ) );
    */

    _CH(glUseProgram( m_shaderProgram ));

    mvpMatrix = mvpMatrix.Transpose();
    _CH(glUniformMatrix4fv( m_modelViewProjLocation, 1, GL_FALSE, mvpMatrix.ToFloatPtr() ));

    _CH(glBindVertexArray( surf.m_surf.m_vao ));

    _CH(glBindTexture( GL_TEXTURE_2D, surf.m_surf.m_material.m_matId ));

    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, surf.m_surf.m_indexBuffer ));

    _CH(glDrawElements( GL_TRIANGLES, surf.m_surf.m_numIndices, GL_UNSIGNED_SHORT, 
                0 ));

    _CH(glBindTexture( GL_TEXTURE_2D, 0 ) );
}

void MyRender::Render( const playerView_t &view ) {
    int i;

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
    
        v.push_back( drawVert_t( idVec3( -10, 0, -10 ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  -10, 0, 10 ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  10, 0, 10 ),
                    idVec2( 0, 0 ) ) );
        v.push_back( drawVert_t( idVec3(  10, 0, -10 ),
                    idVec2( 0, 0 ) ) );

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

static void ProcessEvents( void )
{
    /* Our SDL event placeholder. */
    SDL_Event event;

    /* Grab all the events off the queue. */
    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            /* Handle key presses. */
            break;
        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            Quit( 0 );
            break;
        }

    }
}

void SetupOpengl( int width, int height ) {
    /*
     * Change to the projection matrix and set
     * our viewing volume.
     */
    /*
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_STENCIL_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    gl_playerView.m_pos = idVec3( 0, 0, -50 );
    gl_playerView.m_axis = mat3_identity;

    gl_playerView.m_axis[0][0] = +gl_playerView.m_axis[0][0];
    gl_playerView.m_axis[1][0] = +gl_playerView.m_axis[1][0];
    gl_playerView.m_axis[2][0] = +gl_playerView.m_axis[2][0];

    gl_playerView.m_fovx = 90;
    gl_playerView.m_fovy = 90;

    gl_playerView.m_znear = 3.0f;
    gl_playerView.m_zfar = 10000.0f;

    gl_playerView.m_width = 512;
    gl_playerView.m_height = 512;

    if ( glewInit() != GLEW_OK ) {
        fprintf( stderr, "glewInit() != GLEW_OK\n" );
        Quit( 1 );
    }
    */

    float ratio = (float) width / (float) height;

    /* Our shading model--Gouraud (smooth). */
    //glShadeModel( GL_SMOOTH );

    /* Culling. */
    //glCullFace( GL_BACK );
    //glFrontFace( GL_CCW );
    //glEnable( GL_CULL_FACE );
    
    glDisable( GL_CULL_FACE );

    /* Set the clear color. */
    glClearColor( 0, 0, 0, 0 );

    /* Setup our viewport. */
    glViewport( 0, 0, width, height );

    /*
     * Change to the projection matrix and set
     * our viewing volume.
     */
    /*
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );
    */
    /*
     * EXERCISE:
     * Replace this with a call to glFrustum.
     */
    //gluPerspective( 60.0, ratio, 1.0, 1024.0 );

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_STENCIL_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    gl_playerView.m_pos = idVec3( 0, 0, 50 );
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

static void draw_screen( void )
{
    /* Our angle of rotation. */
    static float angle = 0.0f;

    /*
     * EXERCISE:
     * Replace this awful mess with vertex
     * arrays and a call to glDrawElements.
     *
     * EXERCISE:
     * After completing the above, change
     * it to use compiled vertex arrays.
     *
     * EXERCISE:
     * Verify my windings are correct here ;).
     */
    static GLfloat v0[] = { -1.0f, 1.0f,  -1.0f };
    static GLfloat v1[] = {  1.0f, 1.0f,  -1.0f };
    static GLfloat v2[] = { -1.0f, 1.0f,   1.0f };
    static GLfloat v3[] = {  1.0f, 0.0f,   1.0f };

    /* Send our triangle data to the pipeline. */
    glBegin( GL_TRIANGLES );

    glVertex3fv( v0 );
    glVertex3fv( v1 );
    glVertex3fv( v2 );

    glVertex3fv( v3 );
    glVertex3fv( v1 );
    glVertex3fv( v2 );

    glEnd( );

    SDL_GL_SwapBuffers( );
}

static void process_events( void )
{
    /* Our SDL event placeholder. */
    SDL_Event event;

    /* Grab all the events off the queue. */
    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            /* Handle key presses. */
            exit(0);
            break;
        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            exit(0);
            break;
        }

    }

}

void RenderVideo( void ) {
    process_events( );

    gl_game.Render();
    gl_render.Render( gl_playerView );

    //draw_screen();

    SDL_GL_SwapBuffers( );
}

int main() {
    int oldMs, curMs;

    /*
    MyFloor floor;
    gl_game.AddEntity( floor );
    */

    MyQuad quad;
    gl_game.AddEntity( quad );

    idLib::Init();

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

