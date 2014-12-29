#version 410 core 

#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK FUNCTABLE_SIZE-1

float sinTable[FUNCTABLE_SIZE] = {};
float triangleTable[FUNCTABLE_SIZE] = {};
float squareTable[FUNCTABLE_SIZE] = {};
float sawtoothTable[FUNCTABLE_SIZE] = {};

#define GF_NONE 0
#define GF_SIN  1
#define GF_SQUARE 2
#define GF_TRIANGLE 3 
#define GF_SAWTOOTH 4 
#define GF_INVERSE_SAWTOOTH 5 
#define GF_NOISE 6 

#define DEFORM_NONE 0
#define DEFORM_WAVE 1
#define DEFORM_NORMALS 2
#define DEFORM_BULGE 3
#define DEFORM_MOVE 4
#define DEFORM_PROJECTION_SHADOW 5
#define DEFORM_AUTOSPRITE 6
#define DEFORM_AUTOSPRITE2 7

#define GL_ONE 0
#define GL_ZERO 1
#define GL_DST_COLOR 2
#define GL_ONE_MINUS_DST_COLOR 3
#define GL_SRC_ALPHA 4
#define GL_ONE_MINUS_SRC_ALPHA 5
#define GL_SRC_COLOR 6
#define GL_ONE_MINUS_SRC_COLOR 7

#define CGEN_BAD 0
#define CGEN_IDENTITY_LIGHTING 1
#define CGEN_IDENTITY 2
#define CGEN_ENTITY 3
#define CGEN_ONE_MINUS_ENTITY 4
#define CGEN_EXACT_VERTEX 5
#define CGEN_VERTEX 6
#define CGEN_ONE_MINUS_VERTEX 7
#define CGEN_WAVEFORM 8
#define CGEN_LIGHTING_DIFFUSE 9
#define CGEN_FOG 10
#define CGEN_CONST 11

#define	AGEN_IDENTITY 0
#define	AGEN_SKIP 1
#define	AGEN_ENTITY 2
#define	AGEN_ONE_MINUS_ENTITY 3
#define	AGEN_VERTEX 4
#define	AGEN_ONE_MINUS_VERTEX 5
#define	AGEN_LIGHTING_SPECULAR 6
#define	AGEN_WAVEFORM 7
#define	AGEN_PORTAL 8
#define	AGEN_CONST 9

#define	TCGEN_BAD 0
#define	TCGEN_IDENTITY 1
#define	TCGEN_LIGHTMAP 2
#define	TCGEN_TEXTURE 3
#define	TCGEN_ENVIRONMENT_MAPPED 4
#define	TCGEN_FOG 5
#define	TCGEN_VECTOR 6

#define	TMOD_NONE 0
#define	TMOD_TRANSFORM 1
#define	TMOD_TURBULENT 2
#define	TMOD_SCROLL 3
#define	TMOD_SCALE 4
#define	TMOD_STRETCH 5
#define	TMOD_ROTATE 6
#define	TMOD_ENTITY_TRANSLATE 7

in VS_OUT 
{ 
 vec3 position; 
 vec3 normal; 
 vec2 texcoord; 
} fs_in; 


struct WaveForm_t {
    int m_type;
    float m_base;
    float m_amplitude;
    float m_phase;
    float m_frequency;
};

/*
struct DeformStage_t {
    int m_deformType;

    vec3 m_moveVector;
    WaveForm_t m_deformWave;
    float m_deformSpread;

    float m_bulgeWidth;
    float m_bulgeHeight;
    float m_bulgeSpeed;
};
*/

struct TexModInfo_t {
    int m_texModType;

    WaveForm_t m_waveForm;

    mat2 m_rotate;
    vec2 m_translate;
    vec2 m_scale;
    float2 m_scroll;
    float m_rotateSpeed;
};

struct TexBundle_t {
    int m_texture;

    int m_tcGenMod;
    int m_numTexMods;
    TexModInfo_t m_texMods[8];
};

struct ShaderStage_t {
    WaveForm_t m_rgbWave;
    int m_rgbGen;

    WaveForm_t m_alphaWave;
    int m_alphaGen;

    vec4 m_constantColor;

    TexBundle_t m_texBundle;
    
    int m_glBlend;
};

#define MAX_SHADER_STAGES 8

uniform float time;

uniform int numShaderStages;
uniform ShaderStage_t stages[MAX_SHADER_STAGES];
sampler2DArray samples;

#define EvaluateWaveform(t, wave) \
    ( wave.m_base \
        + t[ ( ( wave.m_phase + time * wave.m_frequency ) \
            * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * wave.m_amplitude )

float GetWaveFormValue(WaveForm_t wave) {
    float res = 0;
    if ( wave.type == GF_SIN ) {
        res = EvaluateWaveform( sinTable, wave );
    } else if ( wave.type == GF_SQUARE ) {
        res = EvaluateWaveform( squareTable, wave );
    } else if ( wave.type == GF_TRIANGLE ) {
        res = EvaluateWaveform( triangleTable, wave );
    } else if ( wave.type == GF_SAWTOOTH ) {
        res = EvaluateWaveform( sawtoothTable, wave );
    } else if ( wave.type == GF_INVERSE_SAWTOOTH  ) {
        res = 1.0f - EvaluateWaveform( sawtoothTable, wave );
    }
    return res;
}

vec4 GetTextureColor( vec2 uv, int texNum ) {
    return texture( samples, vec3( uv, texNum ) );
}

vec4 BlendColor_Internal( vec4 dstColor, vec4 srcColor, int blendCoeff ) {
    vec4 outColor = vec4( 0 );
    switch ( blendCoeff ) {
    case GL_ZERO:
        outColor = vec4( 0 );
        break;
    case GL_ONE:
        outColor = vec4( 1 );
        break;
    case GL_DST_COLOR:
        outColor = dstColor;
        break;
    case GL_ONE_MUNUS_DST_COLOR
        outColor = vec4( 1 ) - dstColor;
        break;
    case GL_SRC_ALPHA:
        outColor = vec4( srcColor.w );
        break;
    case GL_ONE_MINUS_SRC_ALPHA:
        outColor = vec4( 1 - srcColor.w );
    case GL_SRC_COLOR:
        outColor = srcColor;
        break;
    case GL_ONE_MINUS_SRC_COLOR:
        outColor = vec4( 1 ) - srcColor;
        break;
    };
    return outColor;
}

vec4 BlendColor( vec4 dstColor, vec4 srcColor, int blendCoeff ) {
    int srcBlendCoeff = blendCoeff & 31;
    int dstBlendCoeff = blendCoeff >> 5;

    vec4 srcCoeff = BlendColor_Internal( dstColor, srcColor, srcBlendCoeff );
    vec4 dstCoeff = BlendColor_Internal( dstColor, srcColor, dstBlendCoeff );

    return srcCoeff * srcColor + dstCoeff * dstColor;
}

vec2 GetTexCoordinates(ShaderStage_t stage) {
    vec2 res = fs_in.texcoord;

    if ( stage.m_texBundle.m_tcGenMod != TCGEN_TEXTURE ) {
        return vec2( 0 );
    }

    for ( int i = 0; i < stage.m_texBundle.m_numTexMods; ++i ) {
        TexModInfo_t texMode = stage.m_texBundle.m_texMods[i];
        switch ( texMode.m_texModType ) {
        case TMOD_SCALE:

            break;
        };
    }
    return res;
}

vec2 GetRgbGenColor(ShaderStage_t stage) {
    switch ( stage.m_rgbGen ) { 
    case 
    };
}

vec2 GetRgbGenColor(ShaderStage_t stage) {
    switch ( stage.m_rgbGen ) { 
    };
}

vec4 GetStageColor(ShaderStage_t stage) {
    vec4 resColor = vec4( 0 );

    vec3 rgbColor = GetRgbGenColor(stage);
    float rgbAlpha = GetRgbGenAlpha(stage);

    return vec4( rgbColor, rgbAlpha )
        * GetTextureColor( GetTexCoordinates(stage), stage.m_texBundle.m_texture );
}

out vec4 shaderOutColor; 
void main(void) {
    vec4 outColor = vec4( 0 );
    for ( int i = 0; i < numShaderStages; ++i ) {
        vec4 stageColor = GetStageColor( stages[i] );
        outColor = BlendColor( outColor, stageColor, stages[i].m_glBlend );
    }
    shaderOutColor = outColor;
}