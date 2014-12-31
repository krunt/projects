#include "GLSLProgram.h"

#include "Utils.h"

#include "q3_common.h"

GLSLProgram::~GLSLProgram() {
    if ( IsOk() ) {
        glDeleteProgram( m_program );
    }
}

bool GLSLProgram::Init( const std::string &programName ) {
    std::vector<std::string> shaderList;

    if ( programName == "sky" ) {
        shaderList.push_back( "shaders/sky.vs.glsl" );
        shaderList.push_back( "shaders/sky.ps.glsl" );
    } else if ( programName == "q3shader" ) {
        shaderList.push_back( "shaders/q3.vs.glsl" );
        shaderList.push_back( "shaders/q3.ps.glsl" );
    } else {
        shaderList.push_back( "shaders/simple.vs.glsl" );
        shaderList.push_back( "shaders/simple.ps.glsl" );
    }

    return Init( shaderList );
}

bool GLSLProgram::Init( const std::vector<std::string> &progs ) {
    int shader, program, type;
    GLint compileStatus, linkStatus;

    if ( progs.empty() ) {
        return false;
    }

    program = glCreateProgram();

    std::vector<int> attachedShaders;
    for ( int i = 0; i < progs.size(); ++i ) {
        type = -1;
        if ( EndsWith( progs[i], ".vs.glsl" ) ) {
            type = GL_VERTEX_SHADER;
        }
        if ( EndsWith( progs[i], ".ps.glsl" ) ) {
            type = GL_FRAGMENT_SHADER;
        }
        if ( EndsWith( progs[i], ".gs.glsl" ) ) {
            type = GL_GEOMETRY_SHADER;
        }
        if ( EndsWith( progs[i], ".tcs.glsl" ) ) {
            type = GL_TESS_CONTROL_SHADER;
        }
        if ( EndsWith( progs[i], ".tes.glsl" ) ) {
            type = GL_TESS_EVALUATION_SHADER;
        }

        if ( type == -1 ) {
            fprintf( stderr, "unknown shader source `%s' found\n", 
                    progs[i].c_str() );
            return false;
        }

        std::string contents;

        BloatFile( progs[i], contents );

        shader = glCreateShader( type );

        const GLchar *ds = contents.data();
        glShaderSource( shader, 1, (const GLchar **)&ds, NULL );
        glCompileShader( shader );

        glGetShaderiv( shader, GL_COMPILE_STATUS, &compileStatus );

        if ( compileStatus != GL_TRUE ) {
            fprintf( stderr, "compilation of `%s' is unsuccessful\n", 
                    progs[i].c_str() );
            return false;
        }

        glAttachShader( program, shader );

        attachedShaders.push_back( shader );
    }

    if ( attachedShaders.empty() ) {
        return false;
    }

    glLinkProgram( program );
    glGetProgramiv( program, GL_LINK_STATUS, &linkStatus );

    if ( linkStatus != GL_TRUE ) {
        fprintf( stderr, "linking is unsuccessful\n" );
        return false;
    }

    for ( int i = 0; i < attachedShaders.size(); ++i ) {
        glDeleteShader( attachedShaders[i] );
    }

    m_program = program;
    m_linkedOk = true;

    return true;
}

void GLSLProgram::Use() {
    assert( IsOk() );
    glUseProgram( m_program );
}

void GLSLProgram::Bind( const std::string &name, float v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniform1f( location, (GLfloat)v );
}

void GLSLProgram::Bind( const std::string &name, const idVec2 &v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniform2f( location, (GLfloat)v[0], (GLfloat)v[1] );
}

void GLSLProgram::Bind( const std::string &name, const idVec3 &v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniform3f( location, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2] );
}

void GLSLProgram::Bind( const std::string &name, const idVec4 &v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniform4f( location, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2],
        (GLfloat)v[3] );
}

void GLSLProgram::Bind( const std::string &name, const idMat2 &v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniformMatrix2fv( location, 1, false, 
            v.ToFloatPtr() );
}

void GLSLProgram::Bind( const std::string &name, const idMat3 &v ) {
    idMat3 tv = v.Transpose();
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniformMatrix3fv( location, 1, false, v.ToFloatPtr() );
}

void GLSLProgram::Bind( const std::string &name, const idMat4 &v ) {
    GLint location = glGetUniformLocation( m_program, name.c_str() );
    glUniformMatrix4fv( location, 1, false, v.ToFloatPtr() );
}

int GLSLProgram::GetUniformSize( const std::string &name ) const
{
    GLuint uboIndex; GLint uboSize;
    uboIndex = glGetUniformBlockIndex( m_program, name.c_str() );
    _CH(glGetActiveUniformBlockiv( m_program, 
        uboIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize ));
    return uboSize;
}

std::vector<GLint> GLSLProgram::GetUniformOffsets( 
        const std::string &name ) const 
{
    int i, uniformCount;
    GLuint uboIndex;
    std::vector<GLint> indices, offsets;

    uboIndex = glGetUniformBlockIndex( m_program, name.c_str() );
    _CH(glGetActiveUniformBlockiv( m_program, 
        uboIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniformCount ));

    indices.resize( uniformCount );
    _CH(glGetActiveUniformBlockiv( m_program, 
        uboIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, indices.data() ));

    offsets.resize( uniformCount );
    glGetActiveUniformsiv( m_program, uniformCount, 
            (const GLuint *)indices.data(), GL_UNIFORM_OFFSET, offsets.data() );

    return offsets;
}

void GLSLProgram::CreateUniformBuffer( const std::string &name ) {
    GLuint ubo, uboIndex;

    uboIndex = glGetUniformBlockIndex( m_program, name.c_str() );

    _CH(glGenBuffers( 1, &ubo ));
    _CH(glBindBuffer( GL_UNIFORM_BUFFER, ubo ));
    _CH(glBufferData( GL_UNIFORM_BUFFER, 
        GetUniformSize( name ), NULL, GL_DYNAMIC_DRAW ));
    glBindBufferBase( GL_UNIFORM_BUFFER, uboIndex, ubo );

    m_uniformMap[ name ] = std::make_pair( uboIndex, ubo );
}

void GLSLProgram::Bind( const std::string &name, const Q3StagesBlock &obj ) {
    BufIndexPair bPair;

    if ( m_uniformMap.find( name ) == m_uniformMap.end() ) {
        CreateUniformBuffer( name );
    }
    
    bPair = m_uniformMap[name];

    glBindBuffer( GL_UNIFORM_BUFFER, bPair.second );
    byte *pBuffer = (byte *)glMapBufferRange( GL_UNIFORM_BUFFER, 0, 
        GetUniformSize( name ), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT );

    checkError();

    assert( pBuffer );

    obj.Pack( pBuffer, GetUniformOffsets( name ) );

    glUnmapBuffer( GL_UNIFORM_BUFFER );

    glBindBufferBase( GL_UNIFORM_BUFFER, bPair.first, bPair.second );
}

ObjectCache<GLSLProgram> glProgramCache;
