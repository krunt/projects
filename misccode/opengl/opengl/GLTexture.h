#ifndef TEXTURE__H_
#define TEXTURE__H_

#include "d3lib/Lib.h"

class GLTexture {
public:
    GLTexture() {}
    virtual ~GLTexture();

    virtual bool Init( const std::string &name );
    bool Init( byte *data, int width, int height, int format );

    virtual void Bind( void );
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

    virtual void Bind( void );
    virtual void Unbind( void );
};

typedef GLTexture GTexture;
typedef GLTextureCube GTextureCube;

#endif
