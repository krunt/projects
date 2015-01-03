#ifndef ODE_PHYSICS__H_
#define ODE_PHYSICS__H_

#include <ode/ode.h>
#include <boost/scoped_ptr.hpp>
#include "d3lib/Lib.h"

class OdeWorld;
class OdeBody;

class OdePhysics {
public:
    void Init();
    void RunFrame( int ms );
    void Shutdown();

    OdeWorld &GetWorld() const;

private:
    boost::scoped_ptr<OdeWorld> m_world;
};


class OdeBody;
class OdeWorld {
public:

    OdeWorld( dWorldID w )
        : m_world( w )
    {
        m_space = dSimpleSpaceCreate( 0 );
        m_contactGroup = dJointGroupCreate( 0 );
        m_jointGroupStatic = dJointGroupCreate( 0 );
    }

    ~OdeWorld() {
        if ( m_world ) {
            dJointGroupDestroy( m_jointGroupStatic );
            dJointGroupDestroy( m_contactGroup );
            dSpaceDestroy( m_space );
            dWorldDestroy( m_world );
        }
    }

    OdeBody CreateBody() const;
    void Step( int ms );

    void SetGravity( const idVec3 &vec ) const;

    friend class OdeBody;

private:
    dWorldID m_world;
    dJointGroupID m_contactGroup;
    dJointGroupID m_jointGroupStatic;
    dSpaceID m_space;
};


class OdeBody {
public:
    OdeBody( dBodyID d )
        : m_body( d ), m_geom( NULL )
    {}

    void   SetPosition( const idVec3 &pos );
    idVec3 GetPosition( void ) const;

    void   SetOrientation( const idQuat &r );
    idQuat GetOrientation( void ) const;

    void   SetLinearVelocity( const idVec3 &v );
    //idVec3 GetLinearVelocity( void ) const;

    void   SetAngularVelocity( const idVec3 &v );
    //idVec3 GetAngularVelocity( void ) const;

    void   SetAsBox( const idVec3 &extents, float density );

    /* make body static */
    void   SetStatic( void ); 

private:
    dBodyID m_body;
    dGeomID m_geom;
};

extern OdePhysics gl_physics;

#endif
