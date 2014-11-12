#ifndef GLSLPROGRAM__H_
#define GLSLPROGRAM__H_

#include <string>
#include <vector>
#include "d3lib/Lib.h"

class GLSLProgram {
public:
    GLSLProgram() : m_linkedOk( false ) {}
    ~GLSLProgram();

    bool Init( const std::vector<std::string> &progs );

    void Bind( const std::string &name, float v );
    void Bind( const std::string &name, const idVec2 &v );
    void Bind( const std::string &name, const idVec3 &v );
    void Bind( const std::string &name, const idVec4 &v );
    void Bind( const std::string &name, const idMat2 &v );
    void Bind( const std::string &name, const idMat3 &v );
    void Bind( const std::string &name, const idMat4 &v );

    void Use();

    bool IsOk() const { return m_linkedOk; }

private:
    int m_program;
    bool m_linkedOk;
};

#endif
