#ifndef TEXTURE__H_
#define TEXTURE__H_

#include "d3lib/Lib.h"

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

class MyRender;

class GLTexture {
public:
    GLTexture() {}
    virtual ~GLTexture();

    virtual bool Init( const std::string &name, int textureUnit = 0 );
    virtual bool Init( int textureUnit = 0 );
    bool Init( byte *data, int width, int height, 
            int format, int textureUnit = 0 );

    virtual void Bind( void );
    virtual void Unbind( void );

    bool IsOk() const { return m_loadOk; }

    friend class GLTarget;

protected:
    GLuint m_texture;
    int    m_loadOk;
    int    m_textureUnit;
};

class GLTextureCube: public GLTexture {
public:
    GLTextureCube() {}

    virtual bool Init( const std::string &name, int textureUnit = 0 );
    virtual bool Init( int textureUnit = 0 );

    virtual void Bind( void );
    virtual void Unbind( void );
};

#endif
