#version 410 core 

//#define FUNCTABLE_SIZE 1024
//#define FUNCTABLE_SIZE 2
#define FUNCTABLE_SIZE 16
#define FUNCTABLE_MASK FUNCTABLE_SIZE-1


float sinTable[FUNCTABLE_SIZE] = float[FUNCTABLE_SIZE]( 0.00,0.41,0.74,0.95,0.99,0.86,0.59,0.20,-0.21,-0.59,-0.87,-1.00,-0.95,-0.74,-0.40,0.01 );
float triangleTable[FUNCTABLE_SIZE] = float[FUNCTABLE_SIZE]( 0.00,0.27,0.53,0.80,0.93,0.67,0.40,0.13,-0.13,-0.40,-0.67,-0.93,-0.80,-0.53,-0.27,-0.00 );
float squareTable[FUNCTABLE_SIZE] = float[FUNCTABLE_SIZE]( 1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,-1.00,-1.00,-1.00,-1.00,-1.00,-1.00,-1.00,-1.00 );
float sawtoothTable[FUNCTABLE_SIZE] = float[FUNCTABLE_SIZE]( 0.00,0.07,0.13,0.20,0.27,0.33,0.40,0.47,0.53,0.60,0.67,0.73,0.80,0.87,0.93,1.00 );
float noiseTable[FUNCTABLE_SIZE] = float[FUNCTABLE_SIZE]( 0.72,0.38,-0.10,0.54,-0.44,-0.16,0.73,-0.67,0.19,0.54,-0.07,-0.45,-0.00,0.30,0.34,-0.17 );


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
#define GL_DST_ALPHA 8
#define GL_ONE_MINUS_DST_ALPHA 9

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

out vec4 shaderOutColor; 

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
    vec2 m_scroll;
    float m_rotateSpeed;
};

struct TexBundle_t {
    int m_texture;

    int m_tcGenMod;
    int m_numTexMods;
    TexModInfo_t m_texMods[4];
};

layout (std140) struct ShaderStage_t {
    WaveForm_t m_rgbWave;
    int m_rgbGen;

    WaveForm_t m_alphaWave;
    int m_alphaGen;

    vec4 m_constantColor;

    TexBundle_t m_texBundle;
    
    int m_glBlend;
};

#define MAX_SHADER_STAGES 8
#define MAX_SAMPLERS 30

// uniforms begin
uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 lightDir;
uniform float time;

/*
layout (std140) uniform StagesBlock {
    ShaderStage_t stages[MAX_SHADER_STAGES];
};
*/

uniform int numShaderStages;
uniform ShaderStage_t stages[MAX_SHADER_STAGES];

uniform sampler2D samples[MAX_SAMPLERS];

#define EvaluateWaveform(t, wave) ( (wave).m_base + (t)[ int( ( (wave).m_phase + ( time - floor(time) )* (wave).m_frequency ) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * (wave).m_amplitude )

float GetWaveFormValue(WaveForm_t wave) {
    float res = 0;
    if ( wave.m_type == GF_SIN ) {
        res = EvaluateWaveform( sinTable, wave );
    } else if ( wave.m_type == GF_SQUARE ) {
        res = EvaluateWaveform( squareTable, wave );
    } else if ( wave.m_type == GF_TRIANGLE ) {
        res = EvaluateWaveform( triangleTable, wave );
    } else if ( wave.m_type == GF_SAWTOOTH ) {
        res = EvaluateWaveform( sawtoothTable, wave );
    } else if ( wave.m_type == GF_INVERSE_SAWTOOTH  ) {
        res = 1.0f - EvaluateWaveform( sawtoothTable, wave );
    } else if ( wave.m_type == GF_NOISE  ) {
        res = EvaluateWaveform( noiseTable, wave );
    }
    return res;
}

vec4 GetTextureColor( vec2 uv, int texNum ) {
    return texture( samples[texNum], uv );
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
    case GL_ONE_MINUS_DST_COLOR:
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
    int dstBlendCoeff = ( blendCoeff >> 5 ) & 31;

    vec4 srcCoeff = BlendColor_Internal( dstColor, srcColor, srcBlendCoeff );
    vec4 dstCoeff = BlendColor_Internal( dstColor, srcColor, dstBlendCoeff );

    return srcCoeff * srcColor + dstCoeff * dstColor;
}

vec2 TexGetScrollCoordinates(vec2 v, TexModInfo_t texMode) {
    float adjS = texMode.m_scroll[0] * time;
    float adjT = texMode.m_scroll[1] * time;

    adjS -= floor(adjS);
    adjT -= floor(adjT);

    v += vec2( adjS, adjT );

    return v;
}

vec2 TexCalcStretchTexCoords(vec2 res, TexModInfo_t texMode) {
    float p = 1.0f / GetWaveFormValue(texMode.m_waveForm);

    mat2 transMat = mat2( p );
    vec2 transVec = vec2( 0.5f - 0.5f * p );

    return res * transMat + transVec;
}

vec2 TexCalcRotateTexCoords(vec2 res, TexModInfo_t texMode) {
    float degs; 
    float radians;

    degs = -texMode.m_rotateSpeed * time;
    radians = degs / 360.0f;
    radians -= floor( radians );

    float sinValue = sinTable[ int(radians * FUNCTABLE_SIZE) ];
    float cosValue = sqrt( 1 - sinValue * sinValue );

    mat2 mrot = mat2( vec2( cosValue, -sinValue ),
        vec2( sinValue, cosValue ) );
    vec2 translateVec = vec2( 0.5f - 0.5f * cosValue + 0.5f * sinValue,
            0.5f - 0.5f * cosValue - 0.5f * sinValue );

    return res * mrot + translateVec;
}

vec2 GetTexCoordinates(ShaderStage_t stage) {
    vec2 res = fs_in.texcoord;

    /*
    if ( stage.m_texBundle.m_tcGenMod != TCGEN_TEXTURE ) {
        //return vec2( 0 );
        return res;
    }
    */

    for ( int i = 0; i < stage.m_texBundle.m_numTexMods; ++i ) {
        TexModInfo_t texMode = stage.m_texBundle.m_texMods[i];
        switch ( texMode.m_texModType ) {
        case TMOD_SCALE:
            res[0] *= texMode.m_scale[0];
            res[1] *= texMode.m_scale[1];
            break;

        case TMOD_SCROLL:
            res = TexGetScrollCoordinates(res, texMode);
            break;

        case TMOD_TRANSFORM:
            res = res * texMode.m_rotate + texMode.m_translate;
            break;

        case TMOD_STRETCH:
            res = TexCalcStretchTexCoords(res, texMode);
            break;

        case TMOD_ROTATE:
            res = TexCalcRotateTexCoords(res, texMode);
            break;

        };
    }
    return res;
}

vec3 GetRgbGenColor(ShaderStage_t stage) {
    vec3 res = vec3(1);
    switch ( stage.m_rgbGen ) { 
    case CGEN_IDENTITY:
        res = vec3( 1.0f );
        break;
    case CGEN_CONST:
        res = stage.m_constantColor.xyz;
        break;
    case CGEN_WAVEFORM:
        res = vec3( GetWaveFormValue(stage.m_rgbWave) );
        break;
    case CGEN_LIGHTING_DIFFUSE:
        res = vec3( dot(fs_in.normal, lightDir) );
        break;
    };
    return res;
}

float GetRgbGenAlpha(ShaderStage_t stage) {
    float res = 1;
    switch ( stage.m_alphaGen ) { 
    case AGEN_IDENTITY:
        res = 1.0f;
        break;
    case AGEN_CONST:
        res = stage.m_constantColor.w;
        break;
    case AGEN_WAVEFORM:
        res = GetWaveFormValue(stage.m_alphaWave);
        break;
    };
    return res;
}

vec4 GetStageColor(ShaderStage_t stage) {
    vec4 resColor = vec4( 0 );

    vec3 rgbColor = GetRgbGenColor(stage);
    float rgbAlpha = GetRgbGenAlpha(stage);

    return vec4( rgbColor, rgbAlpha )
        * GetTextureColor( GetTexCoordinates(stage), stage.m_texBundle.m_texture );
}

void main(void) {
    vec4 outColor = vec4( 0 );
    for ( int i = 0; i < numShaderStages; ++i ) {
        vec4 stageColor = GetStageColor( stages[i] );
        outColor = BlendColor( outColor, stageColor, stages[i].m_glBlend );
    }
    shaderOutColor = outColor;
    //shaderOutColor = GetTextureColor( fs_in.texcoord * vec2(0.1,0.1) 
            //+ vec2(time-floor(time),EvaluateWaveForm), 0 );
    /*
    vec4 x1 = GetTextureColor( fs_in.texcoord * vec2(0.1,0.1) 
            + vec2(time-floor(time), 0), 0 );
            */
    //vec4 x2 = GetTextureColor( fs_in.texcoord, 0 );
    //vec4 x2 = GetTextureColor( GetTexCoordinates( stages[0] ), 0 );
    //shaderOutColor = BlendColor(  x1, x2, ( GL_ZERO << 5 ) | GL_ONE );
}

