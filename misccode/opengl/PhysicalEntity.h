#ifndef PHYSICAL_ENTITY__H_
#define PHYSICAL_ENTITY__H_

#include "OdePhysics.h"
#include "MyEntity.h"

class MyPhysicalEntity: public MyEntity {
public:
    MyPhysicalEntity( const Map &m = Map() );

    virtual void Spawn( void );
    virtual void Precache( void ) = 0;
    virtual void Think( int ms );
    virtual void Render( void ) = 0;

protected:
    OdeBody m_body;
};

#endif /* PHYSICAL_ENTITY__H_ */
