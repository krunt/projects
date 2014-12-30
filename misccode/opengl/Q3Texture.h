
class Q3Material: public MaterialBase {
public:
    Q3Material() : m_loadOk( false ) {}
    virtual ~Q3Material() {}

    virtual bool Init( const std::string &name );
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

private:
    bool IsOk() const { return m_loadOk; }

    bool ParseShader( const std::string &name );
    bool InitUniformVariables( void );
    void BindTextures( const CommonMaterialParams &params );

    int    m_loadOk;

    shader_t m_shader;

    int m_texIndex;
    Q3StagesBlock m_stagesBlock;
};
