#ifndef TEXTURE__H_
#define TEXTURE__H_

#include "d3lib/Lib.h"

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

class GLTexture {
public:
    GLTexture() {}
    ~GLTexture();

    bool Init( const std::string &name, int textureUnit = 0 );
    bool Init( byte *data, int width, int height, 
            int format, int textureUnit = 0 );

    void Bind( void );
    void Unbind( void );

    bool IsOk() const { return m_loadOk; }

private:
    GLuint m_texture;
    int    m_loadOk;
    int    m_textureUnit;
};

#endif
