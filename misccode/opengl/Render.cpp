
#include "Render.h"
#include "Utils.h"

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

void MyRender::SetupViewMatrix( const playerView_t &view ) {
    //idMat4 res( view.m_axis.Transpose(), idVec3( 0, 0, 0 ) );
    idMat4 res( view.m_axis, idVec3( 0, 0, 0 ) );
    idVec4 pos( view.m_pos[0], view.m_pos[1], view.m_pos[2], 0.0f );

    /*
    res[0][3] = - res[0][0] * pos[0] + - res[1][0] * pos[1] + - res[2][0] * pos[2];
    res[1][3] = - res[0][1] * pos[0] + - res[1][1] * pos[1] + - res[2][1] * pos[2];
    res[2][3] = - res[0][2] * pos[0] + - res[1][2] * pos[1] + - res[2][2] * pos[2];
    */

    res[0][3] = - res[0][0] * pos[0] + - res[0][1] * pos[1] + - res[0][2] * pos[2];
    res[1][3] = - res[1][0] * pos[0] + - res[1][1] * pos[1] + - res[1][2] * pos[2];
    res[2][3] = - res[2][0] * pos[0] + - res[2][1] * pos[1] + - res[2][2] * pos[2];

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
    //m[2][3] = +2 * zfar * znear / depth;

    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = -1.0f;
    m[3][3] = 0;
}

void MyRender::CreateStandardShaders() {
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

    if ( !m_skyTexture.Init( "images/cloudy", 1 ) ) {
        exit(1);
    }

    if ( !m_screenTexture.Init( 1 ) ) {
        exit( 1 );
    }

    if ( !m_crosshairTexture.Init( "images/crosshair.tga", 1 ) ) {
        exit( 1 );
    }
}

void MyRender::CreateShaderProgram( void ) {
    std::vector<std::string> shaderList;

    shaderList.push_back( "shaders/simple.vs.glsl" );
    shaderList.push_back( "shaders/simple.ps.glsl" );

    assert( m_shaderProgram.Init( shaderList ) );
    assert( m_shaderProgram.IsOk() );

    shaderList.clear();

    shaderList.push_back( "shaders/sky.vs.glsl" );
    shaderList.push_back( "shaders/sky.ps.glsl" );

    assert( m_skyProgram.Init( shaderList ) );
    assert( m_skyProgram.IsOk() );

    shaderList.clear();

    shaderList.push_back( "shaders/post.vs.glsl" );
    shaderList.push_back( "shaders/post.ps.glsl" );

    assert( m_postProcessProgram.Init( shaderList ) );
    assert( m_postProcessProgram.IsOk() );

    shaderList.clear();

    shaderList.push_back( "shaders/crosshair.vs.glsl" );
    shaderList.push_back( "shaders/crosshair.ps.glsl" );

    assert( m_crosshairProgram.Init( shaderList ) );
    assert( m_crosshairProgram.IsOk() );
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

    cached.m_material.m_matProgram = &m_shaderProgram;
    //cached.m_material.m_matProgram = NULL;
    if ( s.m_matName == "white" ) {
        cached.m_material.m_matPtr = &m_whiteTexture;
    } else if ( s.m_matName == "logo" ) {
        cached.m_material.m_matPtr = &m_logoTexture;
    } else if ( s.m_matName == "sky" ) {
        cached.m_material.m_matPtr = &m_skyTexture;
        cached.m_material.m_matProgram = &m_skyProgram;
    } else if ( s.m_matName == "postprocess" ) {
        cached.m_material.m_matPtr = &m_screenTexture;
        cached.m_material.m_matProgram = &m_postProcessProgram;
    } else if ( s.m_matName == "crosshair" ) {
        cached.m_material.m_matPtr = &m_crosshairTexture;
        cached.m_material.m_matProgram = &m_crosshairProgram;
    } else {
        if ( m_textureCache.find( s.m_matName ) != m_textureCache.end() ) {
            cached.m_material.m_matPtr = m_textureCache[ s.m_matName ];
        } else {
            GLTexture *t = new GLTexture();
            if ( !t->Init( s.m_matName, 1 ) ) {
                msg_failure( "Can't cache material `%s'\n", s.m_matName.c_str() );
                return;
            }
            m_textureCache[ s.m_matName ] = t;
            cached.m_material.m_matPtr = t;
        }
    }
}

void MyRender::UncacheSurface( const cached_surf_t &cached ) {
    glDeleteVertexArrays( 1, &cached.m_vao );
}

void MyRender::Init( const playerView_t &view ) {
    CreateStandardShaders();
    CreateShaderProgram();

    m_offScreenTarget.Init( view.m_width, view.m_height );
}

void MyRender::Shutdown( void ) {
}

void MyRender::RenderSurface( const glsurf_t &surf ) {
    int i;
    idMat4 mvpMatrix;

    mvpMatrix = m_projectionMatrix * flipMatrix 
        * m_viewMatrix * surf.m_modelMatrix;

    //printVector( flipMatrix * idVec4( 0, 10, 0 ) );
    /*
    printVector( surf.m_modelMatrix * flipMatrix * idVec4( 0, 10, 10, 1 ) );
    printVector( m_viewMatrix * surf.m_modelMatrix * flipMatrix * idVec4( 0, 10, 10, 1 ) );
    printVector( mvpMatrix * idVec4( 0, 10, 10, 1 ) );
    */

    //printProjectedVector( mvpMatrix * idVec4( 0, -10, -10, 1 ) );
    //printProjectedVector( mvpMatrix * idVec4( 0, 0, 10, 1 ) );

    /*
    printVector( flipMatrix *  m_viewMatrix * surf.m_modelMatrix * idVec4( 0, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 0, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 10, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 20, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 30, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 40, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( 50, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( -10, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( -20, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( -30, 0, 10, 1 ) );
    printProjectedVector( mvpMatrix * idVec4( -40, 0, 10, 1 ) );
    */
    //printProjectedVector( mvpMatrix * idVec4( 10, 10, 10, 1 ) );
    //printProjectedVector( mvpMatrix * idVec4( 10, -10, 10, 1 ) );
    //printVector( flipMatrix *  m_viewMatrix * surf.m_modelMatrix * idVec4( 0, 0, 10, 1 ) );

    GLSLProgram *program = surf.m_surf.m_material.m_matProgram;

    program->Use();
    program->Bind( "mvp_matrix", mvpMatrix.Transpose() );
    program->Bind( "model_matrix", ( flipMatrix * surf.m_modelMatrix ).Transpose() );

    /*
    if ( !m_lights.empty() ) {
        for ( i = 0; i < m_lights.size(); ++i ) {
            m_shaderProgram.Bind( m_lights[i]->GetName(), *m_lights[i] );
        }
    }
    */

    program->Bind( "eye_pos", ToWorld( m_eye.ToVec3() ) );
    program->Bind( "light_pos", ToWorld( m_lightPos.ToVec3() ) );
    program->Bind( "light_dir", ToWorld( m_lightDir.ToVec3() ) );

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
    m_lightPos = idVec4( view.m_pos + idVec3( 0, 0, 50 ), 1.0f );
    m_lightDir = idVec4( 0, 0, -1, 1 );

    SetupViewMatrix( view );
    SetupProjectionMatrix( view );

    _CH(glViewport( 0, 0, view.m_width, view.m_height ));
    _CH(glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ));

    m_offScreenTarget.Bind();

    glEnable( GL_DEPTH_TEST );
    _CH(glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ));
    glDepthFunc( GL_LESS );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    glDepthMask( GL_TRUE );

    for ( i = 0; i < m_surfs.size(); ++i ) {
        if ( m_surfs[i].m_surf.m_material.m_matPtr == &m_screenTexture ) {
            continue;
        }

        RenderSurface( m_surfs[i] );
    }

    m_offScreenTarget.CopyToTexture( m_screenTexture );
    glDefaultTarget.Bind();

    //postprocessing
    for ( i = 0; i < m_surfs.size(); ++i ) {
        if ( m_surfs[i].m_surf.m_material.m_matPtr != &m_screenTexture ) {
            continue;
        }

        RenderSurface( m_surfs[i] );
    }

    m_surfs.clear();
}

MyRender gl_render;
