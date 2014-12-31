#ifndef MYRENDER__H_
#define MYRENDER__H_

#include "Common.h"
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
    void Shutdown( void );

private:
    void SetupViewMatrix( const playerView_t &view );
    void SetupProjectionMatrix( const playerView_t &view );
    void RenderSurface( const glsurf_t &surf );

    std::vector<glsurf_t> m_surfs;

    idVec4 m_eye; /* eye-position */

    idMat4 m_viewMatrix;
    idMat4 m_projectionMatrix;

    float m_time;

    idVec4 m_lightDir;
};

extern MyRender gl_render;

#endif /* MYRENDER__H_ */
