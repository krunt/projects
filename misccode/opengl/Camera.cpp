
#include "Camera.h"

static const float kStepSize = 0.1f;

const playerView_t Camera::GetPlayerView( void ) const {
    playerView_t view;

    view.m_pos = m_pos;
    view.m_axis = m_quat.ToMat3();

    view.m_fovx = 74;
    view.m_fovy = 74;

    view.m_znear = 1.0f;
    view.m_zfar = 10000.0f;

    view.m_width = 640;
    view.m_height = 480;

    return view;
}

idVec3 Camera::MoveForward( float stepSize ) const {
    return  m_quat * idVec3( 1, 0, 0 ) * stepSize;
}

idVec3 Camera::MoveBackward( float stepSize ) const {
    return MoveForward( -stepSize );
}

idVec3 Camera::MoveLeft( float stepSize ) const {
    return  m_quat * idVec3( 0, 1, 0 ) * -stepSize;
}

idVec3 Camera::MoveRight( float stepSize ) const {
    return MoveLeft( -stepSize );
}

idVec3 Camera::MoveUp( float stepSize ) const {
    return  m_quat * idVec3( 0, 0, 1 ) * stepSize;
}

idVec3 Camera::MoveDown( float stepSize ) const {
    return MoveUp( -stepSize );
}

void Camera::Yaw( float degrees ) {
    idVec3 axis =  m_quat * idVec3( 0, 0, 1 );
    //idVec3 axis =  idVec3( 0, 0, 1 );
    idQuat rotQuat( axis, DEG2RAD( degrees ) );
    rotQuat.Normalize();
    m_quat = rotQuat * m_quat;
}

void Camera::Pitch( float degrees ) {
    idVec3 axis =  m_quat * idVec3( 0, 1, 0 );
    //idVec3 axis =  idVec3( 0, 1, 0 );
    idQuat rotQuat( axis, DEG2RAD( degrees ) );
    rotQuat.Normalize();
    m_quat = rotQuat * m_quat;
}

void Camera::NormalizeView( void ) {
    m_quat = mat3_identity.ToQuat();
}
