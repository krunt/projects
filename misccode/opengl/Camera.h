
#ifndef CAMERA__H_
#define CAMERA__H_

#include "Common.h"

class Camera {
public:
    void Init( const idVec3 &pos, const idMat3 &axis ) {
        m_pos = pos;
        m_quat = axis.ToQuat();
    }

    const playerView_t GetPlayerView( void ) const;

    idVec3 MoveForward( float stepSize ) const;
    idVec3 MoveBackward( float stepSize ) const;
    idVec3 MoveLeft( float stepSize ) const;
    idVec3 MoveRight( float stepSize ) const;
    idVec3 MoveUp( float stepSize ) const;
    idVec3 MoveDown( float stepSize ) const;

    void Move( const idVec3 &delta ) { m_pos += delta; }

    void Yaw( float degrees );
    void Pitch( float degrees );

    void NormalizeView( void );

private:
    idVec3 m_pos;
    idQuat m_quat;
};

extern Camera gl_camera;

#endif /* CAMERA__H_ */
