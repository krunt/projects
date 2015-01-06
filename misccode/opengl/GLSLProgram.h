#ifndef GLSLPROGRAM__H_
#define GLSLPROGRAM__H_

#include "d3lib/Lib.h"

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "ObjectCache.h"

class Q3StagesBlock;

class GLSLProgram {
public:
    GLSLProgram() : m_linkedOk( false ) {}
    ~GLSLProgram();

    bool Init( const std::string &name );
    bool Init( const std::vector<std::string> &progs );

    void Bind( const std::string &name, float v );
    void Bind( const std::string &name, const idVec2 &v );
    void Bind( const std::string &name, const idVec3 &v );
    void Bind( const std::string &name, const idVec4 &v );
    void Bind( const std::string &name, const idMat2 &v );
    void Bind( const std::string &name, const idMat3 &v );
    void Bind( const std::string &name, const idMat4 &v );
    void Bind( const std::string &name, const Q3StagesBlock &obj );

    void Use();

    bool IsOk() const { return m_linkedOk; }

    void CreateUniformBuffer( const std::string &name );
    std::vector<GLint> GetUniformOffsets( const std::string &name ) const;

    int m_program;

private:
    int GetUniformSize( const std::string &name ) const;

    bool m_linkedOk;

    typedef std::pair<GLuint, GLuint> BufIndexPair;
    std::map<std::string, BufIndexPair> m_uniformMap;
};

extern ObjectCache<GLSLProgram> glProgramCache;

#endif /* GLSLPROGRAM__H_ */
