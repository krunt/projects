void MyGame::Init( void ) {
    AddEntity(new std::shared_ptr<MyFloor>( new MyFloor() ));

    SpawnEntities();
}

void MyGame::Shutdown( void ) {
}

void MyGame::RunFrame( int ms ) {
    for ( int i = 0; i < m_ents.size(); ++i ) {
        m_ents[i]->Think( ms );
    }
}

void MyGame::Render( void ) {
    for ( int i = 0; i < m_ents.size(); ++i ) {
        m_ents[i]->Render();
    }
}

void MyGame::SpawnEntities( void ) {
    for ( int i = 0; i < m_ents.size(); ++i ) {
        m_ents[i]->Spawn();
    }
}

void MyGame::AddEntity( const std::shared_ptr<MyEntity> &ent ) {
    m_ents.push_back( ent );
}
