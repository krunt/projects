
#include "Camera.h"

static const float kStepSize = 0.05f;

const playerView_t Camera::GetPlayerView( void ) const {
    playerView_t view;

    view.m_pos = m_pos;
    view.m_axis = m_angles.ToMat3();

    view.m_fovx = 74;
    view.m_fovy = 74;

    view.m_znear = 3.0f;
    view.m_zfar = 10000.0f;

    view.m_width = 640;
    view.m_height = 480;

    return view;
}

void Camera::MoveForward( void ) {
    idVec3 forward = m_angles.ToForward();
    forward.y = -forward.y;
    forward.z = -forward.z;
    m_pos += forward * kStepSize;
}

void Camera::MoveBackward( void ) {
    idVec3 forward = m_angles.ToForward();
    forward.y = -forward.y;
    forward.z = -forward.z;
    m_pos -= forward * kStepSize;
}

void Camera::StrafeLeft( void ) {
    idVec3 forward, right, up;
    m_angles.ToVectors( &forward, &right, &up );
    right.y = -right.y;
    m_pos -= right * kStepSize;
}

void Camera::StrafeRight( void ) {
    idVec3 forward, right, up;
    m_angles.ToVectors( &forward, &right, &up );
    right.y = -right.y;
    m_pos += right * kStepSize;
}

void Camera::TurnLeft( float degrees ) {
    m_angles.yaw += DEG2RAD( degrees );
}

void Camera::TurnRight( float degrees ) {
    m_angles.yaw -= DEG2RAD( degrees );
}

void Camera::TurnUp( float degrees ) {
    m_angles.pitch += DEG2RAD( degrees );
}

void Camera::TurnDown( float degrees ) {
    m_angles.pitch -= DEG2RAD( degrees );
}

