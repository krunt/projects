
#include "Common.h"
#include "PhysicalEntity.h"

MyPhysicalEntity::MyPhysicalEntity( const Map &m ) 
    : MyEntity( m ), m_body( gl_physics.GetWorld().CreateBody() )
{}

void MyPhysicalEntity::Spawn( void ) {
    MyEntity::Spawn();

    m_body.SetPosition( m_pos );
    m_body.SetOrientation( m_axis.ToQuat() );

    if ( m_map.find( "linear_velocity" ) != m_map.end() ) {
        m_body.SetLinearVelocity( idVec3::FromString( m_map[ "linear_velocity" ] ) );
    }

    if ( m_map.find( "angular_velocity" ) != m_map.end() ) {
        m_body.SetAngularVelocity( idVec3::FromString( m_map[ "angular_velocity" ] ) );
    }
}

void MyPhysicalEntity::Think( int ms ) {
    m_pos = m_body.GetPosition();
    m_axis = m_body.GetOrientation().ToMat3();
}
