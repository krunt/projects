#ifndef MYRENDER__H_
#define MYRENDER__H_

#include "Common.h"
#include "GLLight.h"
#include "GLSLProgram.h"
#include "GLTexture.h"

class MyRender {
public:
    void Init( void );
    void Render( const playerView_t &view );
    void CacheSurface( const surf_t &s, cached_surf_t &cached );
    void AddSurface( const glsurf_t &s ) {
        m_surfs.push_back( s );
    }
    void AddLight( const GLLight &lt ) {
        //m_lights.push_back( &lt );
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

extern MyRender gl_render;

#endif /* MYRENDER__H_ */