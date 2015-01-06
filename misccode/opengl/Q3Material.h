#ifndef Q3MATERIAL__H_
#define Q3MATERIAL__H_

#include "Material.h"
#include "q3_common.h"

class Q3Material: public MaterialBase {
public:
    Q3Material() : m_loadOk( false ) {}
    virtual ~Q3Material() {}

    virtual bool Init( const std::string &name );
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

private:
    bool IsOk() const { return m_loadOk; }

    int GetAnimationIndexFromBundle( const textureBundle_t &bundle, float bTime ) const;

    bool ParseShader( const std::string &name );
    bool InitUniformVariables( void );
    void BindTextures( const CommonMaterialParams &params );

    void BindStages();
    void BindShaderStage( const std::string &prefix, const shaderStage_t &t );
    void BindTexBundle( const std::string &prefix, const textureBundle_t &t );
    void BindTexMod( const std::string &prefix, const texModInfo_t &t );
    void BindWaveform( const std::string &prefix, const waveForm_t &w );


    int    m_loadOk;

    int m_texIndex;

    shader_t m_shader;
    Q3StagesBlock m_stagesBlock;
};

#endif /* Q3MATERIAL__H_ */
