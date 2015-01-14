
#ifndef CAMERA__H_
#define CAMERA__H_

#include "Common.h"

class Camera {
public:
    void Init( const idVec3 &pos, const idAngles &angles ) {
        m_pos = pos;
        m_angles = angles;
    }

    const playerView_t GetPlayerView( void ) const;

    void MoveForward( void );
    void MoveBackward( void );
    void StrafeLeft( void );
    void StrafeRight( void );

    void TurnLeft( float degrees );
    void TurnRight( float degrees );

    void TurnUp( float degrees );
    void TurnDown( float degrees );

private:
    idVec3 m_pos;
    idAngles m_angles;
};

#endif /* CAMERA__H_ */
