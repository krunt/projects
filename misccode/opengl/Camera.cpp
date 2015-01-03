
#include "Camera.h"

static const float kStepSize = 0.1f;

const playerView_t Camera::GetPlayerView( void ) const {
    playerView_t view;

    view.m_pos = m_pos;
    view.m_axis = m_axis;

    view.m_fovx = 74;
    view.m_fovy = 74;

    view.m_znear = 1.0f;
    view.m_zfar = 10000.0f;

    view.m_width = 640;
    view.m_height = 480;

    return view;
}

void Camera::MoveForward( void ) {
    idVec3 forward = m_axis[0];
    forward.y = -forward.y;
    m_pos += forward * kStepSize;
}

void Camera::MoveBackward( void ) {
    idVec3 forward = m_axis[0];
    forward.y = -forward.y;
    m_pos -= forward * kStepSize;
}

void Camera::StrafeLeft( void ) {
    /*
    idVec3 forward, right, up;
    m_angles.ToVectors( &forward, &right, &up );
    right.y = -right.y;
    */
    m_pos -= m_axis[1] * kStepSize;
}

void Camera::StrafeRight( void ) {
    /*
    idVec3 forward, right, up;
    m_angles.ToVectors( &forward, &right, &up );
    right.y = -right.y;
    */
    m_pos += m_axis[1] * kStepSize;
}

void Camera::TurnLeft( float degrees ) {
    //m_angles.yaw += DEG2RAD( degrees );
    idVec3 forward = m_axis[0];
    forward *= idRotation( idVec3( 0, 0, 0 ), m_axis[2], DEG2RAD( degrees ) );
    m_axis[0] = forward;
    forward.OrthogonalBasis( m_axis[1], m_axis[2] );
}

void Camera::TurnRight( float degrees ) {
    //m_angles.yaw -= DEG2RAD( degrees );
    idVec3 forward = m_axis[0];
    forward *= idRotation( idVec3( 0, 0, 0 ), m_axis[2], -DEG2RAD( degrees ) );
    m_axis[0] = forward;
    forward.OrthogonalBasis( m_axis[1], m_axis[2] );
}

void Camera::TurnUp( float degrees ) {
    idVec3 forward = m_axis[0];
    forward *= idRotation( idVec3( 0, 0, 0 ), m_axis[1], -DEG2RAD( degrees ) );
    m_axis[0] = forward;
    forward.OrthogonalBasis( m_axis[1], m_axis[2] );
}

void Camera::TurnDown( float degrees ) {
    idVec3 forward = m_axis[0];
    forward *= idRotation( idVec3( 0, 0, 0 ), m_axis[1], DEG2RAD( degrees ) );
    m_axis[0] = forward;
    forward.OrthogonalBasis( m_axis[1], m_axis[2] );
}

