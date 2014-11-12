#include <stdio.h>
#include <string>
#include <assert.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "Utils.h"

#include "GLSLProgram.h"

GLSLProgram::~GLSLProgram() {
    if ( IsOk() ) {
        glDeleteProgram( m_program );
    }
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

