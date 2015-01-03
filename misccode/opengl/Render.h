#ifndef MYRENDER__H_
#define MYRENDER__H_

#include "Common.h"
#include "GLLight.h"
#include "GLSLProgram.h"
#include "GLTexture.h"
#include "GLTarget.h"

class MyRender {
public:
    void Init( const playerView_t &view );
    void Render( const playerView_t &view );
    void CacheSurface( const surf_t &s, cached_surf_t &cached );
    void UncacheSurface( const cached_surf_t &cached );
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
    void CreateStandardShaders();
    void CreateShaderProgram( void );
    void RenderSurface( const glsurf_t &surf );

    std::vector<glsurf_t> m_surfs;
    std::vector<GLLight *> m_lights;

    std::map<std::string, GLTexture *> m_textureCache;

    idVec4 m_eye; /* eye-position */
    idVec4 m_lightPos; /* eye-position */
    idVec4 m_lightDir; /* light-dir */

    GLTexture m_whiteTexture;
    GLTexture m_logoTexture;
    GLTextureCube m_skyTexture;
    GLTexture m_screenTexture;

    GLSLProgram m_shaderProgram;
    GLSLProgram m_skyProgram;
    GLSLProgram m_postProcessProgram;

    GLTarget m_offScreenTarget;

    GLuint m_modelViewProjLocation;

    idMat4 m_viewMatrix;
    idMat4 m_projectionMatrix;
};

extern MyRender gl_render;

#endif /* MYRENDER__H_ */
