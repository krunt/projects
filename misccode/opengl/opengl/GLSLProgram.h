#ifndef GLSLPROGRAM__H_
#define GLSLPROGRAM__H_

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
    void CreateUniformBuffer( const std::string &name );
    int GetUniformSize( const std::string &name ) const;
    std::vector<GLint> GetUniformOffsets( const std::string &name ) const;

    int m_program;
    bool m_linkedOk;

    typedef std::pair<GLuint, GLuint> BufIndexPair;
    std::map<std::string, BufIndexPair> m_uniformMap;
};

typedef GLSLProgram GpuProgram;

#endif /* GLSLPROGRAM__H_ */
