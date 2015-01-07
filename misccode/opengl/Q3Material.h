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

    static void ScanShaders();
    static std::map<std::string, std::string> m_shadersTextMap;

protected:
    bool IsOk() const { return m_loadOk; }

    int GetAnimationIndexFromBundle( const textureBundle_t &bundle, float bTime ) const;

    bool ParseShader( const std::string &name, const std::string &contents );
    bool InitUniformVariables( void );
    void BindTextures( const CommonMaterialParams &params );

    void BindStages();
    void BindShaderStage( const std::string &prefix, const shaderStage_t &t );
    void BindTexBundle( const std::string &prefix, const textureBundle_t &t );
    void BindTexMod( const std::string &prefix, const texModInfo_t &t );
    void BindWaveform( const std::string &prefix, const waveForm_t &w );

    int    m_loadOk;

    shader_t m_shader;
    Q3StagesBlock m_stagesBlock;

    int m_texIndex;
};

class Q3LightMaterial: public Q3Material {
public:
    Q3LightMaterial() : m_stage(NULL) {}
    virtual ~Q3LightMaterial() {}

    virtual bool Init( const std::string &name );
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

private:
    void InitMaterialStage();

    shaderStage_t *m_stage;
};

#endif /* Q3MATERIAL__H_ */
