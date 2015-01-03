#ifndef MYENTITY__H_
#define MYENTITY__H_

#include "d3lib/Lib.h"

class MyEntity {
public:
    MyEntity( const Map &m = Map() );
    virtual ~MyEntity() {}

    virtual void Spawn( void );
    virtual void Precache( void ) = 0;
    virtual void Think( int ms ) = 0;
    virtual void Render( void ) = 0;

protected:
    idVec3 m_pos;
    idMat3 m_axis;
    Map m_map;
};

#endif /* MYENTITY__H_ */
