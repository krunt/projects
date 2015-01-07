
#include "Material.h"

#include "Utils.h"

#include "GLMaterial.h"
#include "Q3Material.h"

void MaterialBase::Bind( const CommonMaterialParams &params ) {
    m_program->Use();
    m_program->Bind( "mvp_matrix", params.m_mvpMatrix );
    m_program->Bind( "model_matrix", params.m_modelMatrix );
    m_program->Bind( "eye_pos", params.m_eye );
    m_program->Bind( "lightDir", params.m_lightDir );
    m_program->Bind( "time", params.m_time );
}

void MaterialBase::Unbind( void ) {
}


boost::shared_ptr<MaterialBase> MaterialCache::Setup( const std::string &name ) 
{
    boost::shared_ptr<MaterialBase> res;

    std::string nameCopy( name );

    if ( EndsWith( nameCopy, ".q3a" ) ) {
        nameCopy = name.substr( 0, nameCopy.size() - 4 );
        res = boost::shared_ptr<MaterialBase>( new Q3Material() );
    } else if ( EndsWith( nameCopy, ".l3a" ) ) {
        nameCopy = name.substr( 0, nameCopy.size() - 4 );
        res = boost::shared_ptr<MaterialBase>( new Q3LightMaterial() );
    } else {
        res = boost::shared_ptr<MaterialBase>( new GLMaterial() );
    }

    if ( !res->Init( nameCopy ) ) {
        return boost::shared_ptr<MaterialBase>();
    }

    return res;
}

MaterialCache glMaterialCache;
