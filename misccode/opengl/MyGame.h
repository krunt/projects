#ifndef MYGAME__H_
#define MYGAME__H_

class MyGame {
public:
    void Init( void );
    void Shutdown( void );

    void RunFrame( int ms );
    void Render( void );
    void SpawnEntities( void );
    void AddEntity( MyEntity &ent );

private:
    std::vector<std::shared_ptr<MyEntity>> m_ents;
};

extern MyGame gl_game;

#endif /* MYGAME__H_ */
