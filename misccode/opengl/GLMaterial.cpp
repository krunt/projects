#include "GLMaterial.h"

bool GLMaterial::Init( const std::string &name ) {
    m_texture = glTextureCache.Get( name );

    if ( !m_texture ) {
        return false;
    }

    m_program = glProgramCache.Get( name );

    if ( !m_program ) {
        return false;
    }

    m_loadOk = true;
    return true;
}

void GLMaterial::Bind( const CommonMaterialParams &params ) {
    assert( IsOk() );
    MaterialBase::Bind( params );
    m_texture->Bind( 0 );
}

void GLMaterial::Unbind( void ) {
    m_texture->Unbind();
}

