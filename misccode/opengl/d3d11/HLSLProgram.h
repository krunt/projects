#ifndef HLSLPROGRAM__H_
#define HLSLPROGRAM__H_

#include "d3lib/Lib.h"

class HLSLProgram {
public:
    HLSLProgram();
    ~HLSLProgram();

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
    bool m_linkedOk;

    std::map<std::string, std::array<float, 16>> m_uniformMap;
    std::vector<std::string> m_constantBufferVars;

    std::string m_vertexShaderBytecode;
    ID3D11VertexShader *m_vertexShader;
    ID3D11PixelShader *m_pixelShader;
    ID3D11InputLayout *m_inputLayout;
    ID3D11Buffer      *m_constantBuffer;
};

typedef HLSLProgram GpuProgram;

#endif /* HLSLPROGRAM__H_ */
