
#include "Material.h"

void MaterialBase::Bind( const CommonMaterialParams &params ) {
    m_program.Use();
    m_program.Bind( "mvp_matrix", params.m_mvpMatrix );
    m_program.Bind( "model_matrix", params.m_modelMatrix );
    m_program.Bind( "eye_pos", params.m_eye );
    m_program.Bind( "lightDir", params.m_lightDir );
    m_program.Bind( "time", params.m_time );
}

void MaterialBase::Unbind( void ) {
}

