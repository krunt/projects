#ifndef MATERIAL__H_
#define MATERIAL__H_

struct CommonMaterialParams {
    idMat4 m_mvpMatrix;
    idMat4 m_modelMatrix;
    idVec3 m_eye;
    idVec4 m_lightDir;
    float  m_time;
};

class MaterialBase {
public:
    virtual ~MaterialBase() {}
    virtual bool Init( const std::string &name ) = 0;
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

protected:
    GLSLProgram m_program;
};

#endif
