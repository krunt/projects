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

private:
    bool IsOk() const { return m_loadOk; }

    int GetAnimationIndexFromBundle( const textureBundle_t &bundle, float bTime ) const;

    bool ParseShader( const std::string &name, const std::string &contents );
    bool InitUniformVariables( void );
    void BindTextures( const CommonMaterialParams &params );

    int    m_loadOk;

    shader_t m_shader;
    Q3StagesBlock m_stagesBlock;
};

#endif /* Q3MATERIAL__H_ */
