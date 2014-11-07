#include <string>
#include <fstream>
#include <streambuf>
#include <assert.h>

static bool EndsWith( const std::string &what, const std::string &suffix ) {
    if ( suffix.size() >= what.size() ) {
        return false;
    }
    return what.substr( what.size() - suffix.size() ) == suffix;
}

static void BloatFile( const std::string &filename, std::string &contents ) {
    std::ifstream t( filename );
    contents = std::string((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
}

GLSLProgram::~GLSLProgram() {
    glDeleteProgram( m_program );
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
        if ( endsWith( progs[i], ".fs.glsl" ) ) {
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
        glShaderSource( shader, 1, (const GLchar **)&contents.data(), NULL );
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

