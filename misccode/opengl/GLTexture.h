
class GLTexture {
public:
    GLTexture() {}
    ~GLTexture();

    bool Init( const std::string &name );
    void Bind( int textureUnit = 0 );

    bool IsOk() const { return m_loadOk; }

private:
    GLuint m_texture;
    int    m_loadOk;
};
