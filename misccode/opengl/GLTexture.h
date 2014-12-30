#ifndef TEXTURE__H_
#define TEXTURE__H_

#include "d3lib/Lib.h"

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

class GLTexture: public TextureBase {
public:
    GLTexture() : m_loadOk( false ) {}
    virtual ~GLTexture();

    virtual bool Init( const std::string &name );
    virtual bool Init( byte *data, int width, int height, int format );

    virtual void Bind( int unit );
    virtual void Unbind( void );

    bool IsOk() const { return m_loadOk; }

protected:
    GLuint m_texture;
    int    m_loadOk;
};

class GLTextureCube: public GLTexture {
public:
    GLTextureCube() {}

    virtual bool Init( const std::string &name );
    virtual bool Init( byte *data, int width, int height, int format );

    virtual void Bind( int unit );
    virtual void Unbind( void );
};

extern ObjectCache<GLTexture> glTextureCache;

#endif
