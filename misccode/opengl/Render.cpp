
#include "Render.h"
#include "Utils.h"

void MyRenderBase::SetupViewMatrix( const playerView_t &view ) {
    idMat4 res( view.m_axis, idVec3( 0, 0, 0 ) );
    idVec4 pos( view.m_pos[0], view.m_pos[1], view.m_pos[2], 0.0f );

    res[0][3] = - res[0][0] * pos[0] + - res[0][1] * pos[1] + - res[0][2] * pos[2];
    res[1][3] = - res[1][0] * pos[0] + - res[1][1] * pos[1] + - res[1][2] * pos[2];
    res[2][3] = - res[2][0] * pos[0] + - res[2][1] * pos[1] + - res[2][2] * pos[2];

    res[3][3] = 1;

    m_viewMatrix = res;
}

void MyRenderBase::SetupProjectionMatrix( const playerView_t &view ) {
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

void MyRenderBase::OnPreRender() {
    m_eye = idVec4( view.m_pos, 1.0f );

    SetupViewMatrix( view );
    SetupProjectionMatrix( view );
}

void MyRenderBase::OnPostRender() {
}

bool MyRenderBase::CreateStandardShaders( void ) {
    byte white[16] = 
        { 0xFF, 0x00, 0xFF, 0xFF,
          0xFF, 0x00, 0x00, 0xFF,
          0x00, 0xFF, 0x00, 0xFF,
          0x00, 0x00, 0xFF, 0xFF,
        };

    m_whiteTexture = std::shared_ptr<GTexture>( new GTexture() );

    if ( !m_whiteTexture->Init( white, 2, 2, GL_RGBA, 0 ) ) {
        return false;
    }

    m_skyTexture = std::shared_ptr<GTexture>( new GTextureCube() );

    if ( !m_skyTexture->Init( "images/cloudy", 1 ) ) {
        return false;
    }

    return true;
}

void MyRenderBase::Init( void ) {
    CreateStandardShaders();
    CreateShaderPrograms();

    m_whiteMaterial.m_texture = m_whiteTexture;
    m_whiteMaterial.m_program = m_shaderProgram;

    m_skyMaterial.m_texture = m_skyTexture;
    m_skyMaterial.m_program = m_skyProgram;
}

void MyRenderBase::Shutdown( void ) {
}


void MyRenderBase::Render( const playerView_t &view ) {
    OnPreRender();

    for ( int i = 0; i < m_surfs.size(); ++i ) {
        RenderSurface( m_surfs[i] );
    }

    OnPostRender();

    m_surfs.clear();
}

