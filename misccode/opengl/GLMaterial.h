#ifndef GL_MATERIAL__H_
#define GL_MATERIAL__H_

class GLMaterial: public MaterialBase {
public:
    GLMaterial() : m_loadOk( false ) {}
    virtual ~GLMaterial() {}

    virtual bool Init( const std::string &name );
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

private:
    bool IsOk() const { return m_loadOk; }

    bool m_loadOk;

    std::shared_ptr<GLTexture> m_texture;
};

#endif /* GL_MATERIAL__H_ */
