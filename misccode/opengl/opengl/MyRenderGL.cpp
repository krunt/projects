

static float flipMatrixData[4][4] = {
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { -1, 0, 0, 0 },
    { 0, 0, 0, 1 },
};

static const idMat4 glflipMatrix( flipMatrixData );

void MyRenderGL::CreateShaderPrograms() {
    std::vector<std::string> shaderList;

    shaderList.push_back( "opengl/glsl/simple.vs.glsl" );
    shaderList.push_back( "opengl/glsl/simple.ps.glsl" );

    m_shaderProgram = std::shared_ptr<GpuProgram>( new GpuProgram() );
    assert( m_shaderProgram->Init( shaderList ) );
    assert( m_shaderProgram->IsOk() );

    shaderList.clear();

    shaderList.push_back( "opengl/glsl/sky.vs.glsl" );
    shaderList.push_back( "opengl/glsl/sky.ps.glsl" );

    m_skyProgram = std::shared_ptr<GpuProgram>( new GpuProgram() );
    assert( m_skyProgram->Init( shaderList ) );
    assert( m_skyProgram->IsOk() );
}

std::shared_ptr<gsurface_t> MyRenderGL::CacheSurface( const surf_t &s ) {
    GLuint vao;
    GLuint positionBuffer, indexBuffer;

    _CH(glGenVertexArrays( 1, &vao ));
    _CH(glBindVertexArray( vao ));

    _CH(glGenBuffers( 1, &positionBuffer ));
    _CH(glBindBuffer( GL_ARRAY_BUFFER, positionBuffer ));
    _CH(glBufferData( GL_ARRAY_BUFFER, s.m_verts.size() * 8 * sizeof(float),
        (void *)s.m_verts.data(), GL_STATIC_DRAW ));

    _CH(glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(0 * sizeof(float ) ) ));
    _CH(glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(3 * sizeof(float ) ) ));
    _CH(glVertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
        (void *)(5 * sizeof(float ) ) ));

    _CH(glEnableVertexAttribArray( 0 ));
    _CH(glEnableVertexAttribArray( 1 ));
    _CH(glEnableVertexAttribArray( 2 ));

    _CH(glGenBuffers( 1, &indexBuffer ));
    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer ));
    _CH(glBufferData( GL_ELEMENT_ARRAY_BUFFER, 
                s.m_indices.size() * sizeof(GLushort),
        (void *)s.m_indices.data(), GL_STATIC_DRAW ));

    std::shared_ptr<gsurface_t> cached = CreateSurface();

    cached->m_vao = vao;
    cached->m_numIndices = s.m_indices.size();
    cached->m_indexBuffer = indexBuffer;

    if ( s.m_matName == "white" ) {
        cached->m_material = m_whiteMaterial;
    } else if ( s.m_matName == "sky" ) {
        cached->m_material = m_skyMaterial;
    } else {
        if ( m_materialCache.find( s.m_matName ) != m_materialCache.end() ) {
            cached->m_material = m_materialCache[ s.m_matName ];
        } else {
            std::shared_ptr<material_t> t = new material_t();
            t->m_texture = CreateTexture( s.m_matName );
            t->m_program = m_shaderProgram;
            if ( !t.IsValid() ) {
                msg_failure( "Can't cache material `%s'\n", s.m_matName.c_str() );
                return nullptr;
            }
            m_materialCache[ s.m_matName ] = t;
            cached->m_material = t;
        }
    }
    return cached;
}

static idVec3 ToWorld( idVec3 &v ) {
    return ( glflipMatrix * idVec4( v, 1.0f ) ).ToVec3();
}

void MyRenderGL::OnPreRender() {
    this->MyRender::OnPreRender();

    _CH(glViewport( 0, 0, view.m_width, view.m_height ));
    _CH(glClear( GL_COLOR_BUFFER_BIT 
        | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ));
}


void MyRenderGL::FlushFrame() {
    SDL_GL_SwapBuffers();
}

void MyRenderGL::RenderSurface( const std::shared_ptr<gsurface_t> &surf ) {
    idmat4 mvpMatrix;

    mvpMatrix = m_projectionMatrix * glflipMatrix 
        * m_viewMatrix * surf.m_modelMatrix;

    surf->m_material.m_program->Bind( "mvp_matrix", mvpMatrix.Transpose() );
    surf->m_material.m_program->Bind( "model_matrix", 
            ( flipMatrix * surf.m_modelMatrix ).Transpose() );
    surf->m_material.m_program->Bind( "eye_pos", ToWorld( m_eye.ToVec3() ) );

    surf->m_material.m_program->Use();

    surf->m_material.m_texture->Bind();

    _CH(glBindVertexArray( surf->m_vao ));

    _CH(glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, surf->m_indexBuffer ));

    _CH(glDrawElements( GL_TRIANGLES, 
                surf->m_numIndices, GL_UNSIGNED_SHORT, 0 ));
}


