#ifndef GLTARGET__H_
#define GLTARGET__H_

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

class GLTexture;
class GLTarget {
public:
    GLTarget() {}
    GLTarget( int framebufferId ) 
        : m_frameBufferId( framebufferId )
    {}
    ~GLTarget();

    void Init( int width, int height );
    void Bind( void );

    void CopyToTexture( const GLTexture &texture );

private:
    GLuint m_frameBufferId;
    GLuint m_renderBufferId;
    GLuint m_renderTexture;
    GLuint m_renderDepthBufferId;
    int m_width, m_height;
};

extern GLTarget glDefaultTarget;

#endif /* GLTARGET__H_*/

