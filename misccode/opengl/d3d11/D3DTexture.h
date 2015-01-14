#ifndef TEXTURE__H_
#define TEXTURE__H_

#include "d3lib/Lib.h"

class D3DTexture {
public:
    GLTexture() {}
    virtual ~GLTexture();

    virtual bool Init( const std::string &name );
    bool Init( byte *data, int width, int height, int format );

    virtual void Bind( void );
    virtual void Unbind( void );

    bool IsOk() const { return m_loadOk; }

protected:
    int    m_loadOk;
};

class D3DTextureCube: public D3DTexture {
public:
    GLTextureCube() {}

    virtual bool Init( const std::string &name );

    virtual void Bind( void );
    virtual void Unbind( void );
};

typedef D3DTexture GTexture;
typedef D3DTextureCube GTextureCube;

#endif
