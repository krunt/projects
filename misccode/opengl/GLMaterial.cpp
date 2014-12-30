
bool GLMaterial::Init( const std::string &name ) {
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

