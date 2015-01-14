#include "GLSLProgram.h"

HLSLProgram::HLSLProgram()
    : m_linkedOk( false ),
      m_vertexShader( nullptr ),
      m_pixelShader( nullptr ),
      m_inputLayout( nullptr ),
      m_constantBuffer( nullptr )
{}

HLSLProgram::~HLSLProgram() {
    if ( IsOk() ) {
        D3DObjectDeleter()(m_vertexShader);
        D3DObjectDeleter()(m_pixelShader);
        D3DObjectDeleter()(m_inputLayout);
        D3DObjectDeleter()(m_constantBuffer);
    }
}

enum class ShaderTypeKey {
    D3D_NONE,
    D3D_VERTEX_SHADER,
    D3D_PIXEL_SHADER,
};

void HLSLProgram::CreateInputLayout() {
    D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 
            0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 
            0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 
            0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    HR(md3dDevice->CreateInputLayout(vertexDesc, 3,
        mVertexBytecode.c_str(), mVertexBytecode.size(), &m_inputLayout));
}

void HLSLProgram::CreateConstantBuffer() {
    ID3D11ShaderReflection *mIShaderReflection;

    HRESULT hr = D3DReflect(
                m_vertexShaderBytecode.data(),
                m_vertexShaderBytecode.size(),
                IID_ID3D11ShaderReflection,
                (void **) &mIShaderReflection);


    if ( FAILED(hr) ) {
        msg_warning0( "can't D3DReflect on vertex shader" );
        return;
    }

    D3D11_SHADER_DESC desc;
    hr = mIShaderReflection->GetDesc( &desc );

    if ( FAILED(hr) ) {
        msg_warning0( "can't get vertex shader description" );
        return;
    }

    if ( desc.ConstantBuffers > 1 ) {
        msg_warning0( "constant buffers > 1" );
        return;
    }

    if ( !desc.ConstantBuffers ) {
        return;
    }

    ID3D11ShaderReflectConstantBuffer *shaderReflectionConstantBuffer
        = mIShaderReflection->GetConstantBufferByIndex( 0 );

    D3D11_SHADER_BUFFER_DESC constantBufferDesc;
    hr = shaderReflectionConstantBuffer->GetDesc( &constantBufferDesc );

    if ( FAILED(hr) ) {
        msg_warning0( "can't get constant buffer description" );
        return;
    }

    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth = constantBufferDesc.Size;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    HR(md3dDevice->CreateBuffer( &cbDesc, NULL, &m_constantBuffer ));

    /* get constant-buffer variable names */
    for (int i = 0; i < constantBufferDesc.Variables; ++i ) {
        ID3D11ShaderReflectionVariable *varRef;
        varRef = shaderReflectionConstantBuffer->GetVariableByIndex( i );

        D3D11_SHADER_VARIABLE_DESC shaderVarDesc;
        hr = varRef->GetDesc(&shaderVarDesc);

        m_constantBufferVars.push_back(shaderVarDesc.Name);
    }
}

bool HLSLProgram::CompileShaderSource( const std::string &source,
        std::string &bytecode )
{
    ID3D10Blob *blob = NULL, *errors = NULL;
    
    UINT compileFlags = D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
    HRESULT hr = D3DX11CompileFromMemory(
        source.c_str(),
        source.size(),
        NULL,
        NULL,
        NULL,
        "main",
        "vs_5_0",
        compileFlags,
        0,
        NULL,
        &blob,
        &errors,
        NULL);
    
    if (FAILED(hr)) {
        msg_warning0( static_cast<const char*>(errors->GetBufferPointer()) );
        errors->Release();
        return false;
    }
    
    bytecode.resize( blob->GetBufferSize() );
    memcpy( &bytecode[0], blob->GetBufferPointer(), blob->GetBufferSize() );
    
    blob->Release();
    return true;
}

ID3D11VertexShader *HLSLProgram::CompileVertexShader( const std::string &source ) {
    std::string bytecode;
    if ( !CompileShaderSource( source, bytecode ) ) {
        return nullptr;
    }

    ID3DVertexShader *result;
    HR(gl_render.GetD3DDevice()->CreateVertexShader(
        bytecode.c_str(), bytecode.size(), NULL, &result));

    m_vertexShaderBytecode = bytecode;

    return result;
}

ID3D11PixelShader *HLSLProgram::CompilePixelShader( const std::string &source ) {
    std::string bytecode;
    if ( !CompileShaderSource( source, bytecode ) ) {
        return nullptr;
    }

    ID3DPixelShader *result;
    HR(gl_render.GetD3DDevice()->CreatePixelShader(
        bytecode.c_str(), bytecode.size(), NULL, &result));

    return result;
}

bool HLSLProgram::Init( const std::vector<std::string> &progs ) {
    ShaderTypeKey type;

    if ( progs.empty() ) {
        return false;
    }

    for ( int i = 0; i < progs.size(); ++i ) {
        type = ShaderTypeKey::D3D_NONE;

        if ( EndsWith( progs[i], ".vs.hlsl" ) ) {
            type = ShaderTypeKey::D3D_VERTEX_SHADER;
        }
        if ( EndsWith( progs[i], ".ps.hlsl" ) ) {
            type = ShaderTypeKey::D3D_PIXEL_SHADER;
        }

        if ( type == ShaderTypeKey::D3D_NONE ) {
            msg_warning( stderr, "unknown shader source `%s' found\n", 
                    progs[i].c_str() );
            return false;
        }

        std::string contents;
        BloatFile( progs[i], contents );

        if ( type == ShaderTypeKey::D3D_NONE ) {
            msg_warning( "shader-name `%s' is not supported", progs[i].c_str() );
            return false;
        }

        if ( type == ShaderTypeKey::D3D_VERTEX_SHADER ) {
            m_vertexShader = CompileVertexShader( progs[i] );
        } else {
            m_pixelShader = CompilePixelShader( progs[i] );
        }
    }

    if ( !m_vertexShader || !m_pixelShader ) {
        msg_warning0( "vertex and pixel shader must both be specified" );
        return false;
    }

    CreateInputLayout();
    CreateConstantBuffer();

    m_linkedOk = true;

    return true;
}

void HLSLProgram::Use() {
    D3D11_MAPPED_SUBRESOURCE pConstData;
    HR(gl_render.GetD3DContent()->Map(
        m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &pConstData));

    for ( int i = 0; i < m_constantBufferVars.size(); ++i, offset += 16 ) {
        if ( m_uniformMap.find( m_constantBufferVars[i] )
                == m_uniformMap.end() )
        {
            msg_warning( "did not found constant buffer variable named `%s'",
                    m_constantBufferVars[i].c_str() );
            return;
        }

        memcpy( pConstData.pData + offset,
                m_uniformMap[m_constantBufferVars[i]].data(),
                16 * sizeof(float) );
    }

    gl_render.GetD3DContext()->Unmap(m_constantBuffer, 0);

    gl_render.GetD3DContext()->IASetInputLayout( m_inputLayout );

    gl_render.GetD3DContext()->VSSetShader( m_vertexShader, NULL, 0 );
    gl_render.GetD3DContext()->PSSetShader( m_pixelShader, NULL, 0 );
}

void HLSLProgram::Bind( const std::string &name, float v ) {
    auto &val = m_uniformMap[name];
    val[0] = v;
}

void HLSLProgram::Bind( const std::string &name, const idVec2 &v ) {
    auto &val = m_uniformMap[name];
    val[0] = v[0]; val[1] = v[1];
}

void HLSLProgram::Bind( const std::string &name, const idVec3 &v ) {
    auto &val = m_uniformMap[name];
    val[0] = v[0]; val[1] = v[1]; val[2] = v[2];
}

void HLSLProgram::Bind( const std::string &name, const idVec4 &v ) {
    auto &val = m_uniformMap[name];
    val[0] = v[0]; val[1] = v[1]; val[2] = v[2]; val[3] = v[3];
}

void HLSLProgram::Bind( const std::string &name, const idMat2 &v ) {
    auto &val = m_uniformMap[name];
    val[0] = v[0][0]; val[1] = v[0][1]; val[2] = v[1][0]; val[3] = v[1][1];
}

void HLSLProgram::Bind( const std::string &name, const idMat3 &v ) {
    int k = 0;
    idMat3 tv = v.Transpose();
    auto &val = m_uniformMap[name];
    for ( int i = 0; i < 3; ++i ) {
        for ( int j = 0; j < 3; ++j ) {
            val[k++] = v[i][j];
        }
    }
}

void HLSLProgram::Bind( const std::string &name, const idMat4 &v ) {
    int k = 0;
    auto &val = m_uniformMap[name];
    for ( int i = 0; i < 3; ++i ) {
        for ( int j = 0; j < 3; ++j ) {
            val[k++] = v[i][j];
        }
    }
}

