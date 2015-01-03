
#include "MyEntity.h"

MyEntity::MyEntity( const Map &m ) 
    : m_map( m ) 
{}

void MyEntity::Spawn( void ) {
    if ( m_map.find( "pos" ) != m_map.end() ) {
        m_pos = idVec3::FromString( m_map[ "pos" ] );
    } else {
        m_pos = vec3_origin;
    }
    m_axis = mat3_identity;

    Precache();
}
