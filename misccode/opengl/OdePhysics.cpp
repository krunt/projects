
#include "OdePhysics.h"

void OdePhysics::Init() {
    dInitODE();
    m_world.reset( new OdeWorld( dWorldCreate() ) );
    m_world->SetGravity( idVec3( 0, 0, -9.81 ));
}

void OdePhysics::RunFrame( int ms ) {
    m_world->Step( ms );
}

void OdePhysics::Shutdown() {
    if ( m_world ) {
        m_world.reset();
    }
    dCloseODE();
}

OdeWorld &OdePhysics::GetWorld() const {
    return *m_world;
}

void OdeWorld::SetGravity( const idVec3 &vec ) const {
    dWorldSetGravity( m_world, vec[0], vec[1], vec[2] );
}

OdeBody OdeWorld::CreateBody() const {
    return OdeBody( dBodyCreate( m_world ) );
}

void OdeBody::SetStatic( void ) {
    OdeWorld &world = gl_physics.GetWorld();
    dJointID staticJoint = dJointCreateFixed( world.m_world, world.m_jointGroupStatic );
    dJointAttach( staticJoint, m_body, 0 );

}

struct CallbackData {
    dWorldID m_world;
    dJointGroupID m_contactGroup;
};

static void OdeNearCallback(void *data, dGeomID o1, dGeomID o2)
{
    CallbackData *cdata = static_cast<CallbackData *>( data );

    dBodyID b1 = dGeomGetBody(o1);
    dBodyID b2 = dGeomGetBody(o2);

    const int MAX_CONTACTS = 8;
    dContact contact[MAX_CONTACTS];
    
    int numc = dCollide (o1, o2, MAX_CONTACTS,
                        &contact[0].geom,
                        sizeof(dContact));
    
    for (int i=0; i<numc; i++) {
        contact[i].surface.mode = dContactApprox1;
        contact[i].surface.mu = 5;
        dJointID c = dJointCreateContact (cdata->m_world, cdata->m_contactGroup, contact+i);
        dJointAttach (c, b1, b2);
    }
}

void OdeWorld::Step( int ms ) {
    float stepsize = (float) ms / 1000.0f;
    if ( stepsize ) {
        CallbackData cdata;
        cdata.m_world = m_world;
        cdata.m_contactGroup = m_contactGroup;

        dSpaceCollide( m_space, static_cast<void*>(&cdata), &OdeNearCallback );
        dWorldQuickStep( m_world, 0.003f );
        dJointGroupEmpty( m_contactGroup );
    }
}


void OdeBody::SetPosition( const idVec3 &pos ) {
    dBodySetPosition( m_body, pos[0], pos[1], pos[2] );
}

idVec3 OdeBody::GetPosition( void ) const {
    idVec3 res;
    const dReal *r = dBodyGetPosition( m_body );
    res[0] = r[0]; res[1] = r[1]; res[2] = r[2];
    return res;
}

void OdeBody::SetOrientation( const idQuat &q ) {
    dQuaternion r;
    r[0] = q[0]; r[1] = q[1]; r[2] = q[2]; r[3] = q[3];
    dBodySetQuaternion( m_body, r );
}

idQuat OdeBody::GetOrientation( void ) const {
    idQuat res;
    const dReal *r = dBodyGetQuaternion( m_body );
    res.x = r[0]; res.y = r[1]; res.z = r[2]; res.w = r[3];
    return res;
}

void OdeBody::SetLinearVelocity( const idVec3 &v ) {
    dBodySetLinearVel( m_body, v[0], v[1], v[2] );
}

void OdeBody::SetAngularVelocity( const idVec3 &v ) {
    dBodySetAngularVel( m_body, v[0], v[1], v[2] );
}

void OdeBody::SetAsBox( const idVec3 &extents, float density ) {
    dMass m;
    dMassSetBox( &m, density, 2 * extents[0], 2 * extents[1], 2 * extents[2] );
    dBodySetMass( m_body, &m ); /* TODO: set rotation */

    OdeWorld &world = gl_physics.GetWorld();
    m_geom = dCreateBox( world.m_space, 2 * extents[0], 2 * extents[1], 2 * extents[2] );
    dGeomSetBody( m_geom, m_body );
}

OdePhysics gl_physics;
