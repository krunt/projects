
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

    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = -1.0f;
    m[3][3] = 0;
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

    cached.m_material = glMaterialCache.Get( s.m_matName );

    if ( !cached.m_material ) {
        msg_failure( "material `%s' not found\n", s.m_matName.c_str() );
        return;
    }
}

void MyRender::Init( void ) {
    m_lightDir = idVec4( 0, 0, 1, 1 );
    m_time = 0;
}

void MyRender::Shutdown( void ) {
}

void MyRender::RenderSurface( const glsurf_t &surf ) {
    idMat4 mvpMatrix;

    mvpMatrix = m_projectionMatrix * flipMatrix 
        * m_viewMatrix * surf.m_modelMatrix;

    CommonMaterialParams params;
    params.m_mvpMatrix = mvpMatrix.Transpose();
    params.m_modelMatrix = ( flipMatrix * surf.m_modelMatrix ).Transpose();
    params.m_eye = ToWorld( m_eye.ToVec3() );
    params.m_lightDir = ToWorld( m_lightDir.ToVec3() );
    params.m_time = m_time;

    _CH(glBindVertexArray( surf.m_surf.m_vao ));

    surf.m_surf.m_material->Bind( params );

    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, surf.m_surf.m_indexBuffer ));

    _CH(glDrawElements( GL_TRIANGLES, 
                surf.m_surf.m_numIndices, GL_UNSIGNED_SHORT, 0 ));

    surf.m_surf.m_material->Unbind();
}

void MyRender::Render( const playerView_t &view ) {
    int i;

    m_eye = idVec4( view.m_pos, 1.0f );
    m_time = (float)idLib::Milliseconds() / 1000.0f;

    SetupViewMatrix( view );
    SetupProjectionMatrix( view );

    _CH(glViewport( 0, 0, view.m_width, view.m_height ));
    _CH(glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ));

    for ( i = 0; i < m_surfs.size(); ++i ) {
        RenderSurface( m_surfs[i] );
    }

    m_surfs.clear();
}

MyRender gl_render;
