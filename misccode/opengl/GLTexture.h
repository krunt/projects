#ifndef TEXTURE__H_
#define TEXTURE__H_

#include <string>

class GLTexture {
public:
    GLTexture() {}
    ~GLTexture();

    bool Init( const std::string &name, int textureUnit = 0 );
    void Bind( void );
    void Unbind( void );

    bool IsOk() const { return m_loadOk; }

private:
    GLuint m_texture;
    int    m_loadOk;
    int    m_textureUnit;
};

#endif
