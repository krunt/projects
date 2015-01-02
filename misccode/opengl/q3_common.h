#ifndef Q3_COMMON__H_
#define Q3_COMMON__H_

#include "d3lib/Lib.h"

#define GLS_BLEND_ONE 0
#define GLS_BLEND_ZERO 1
#define GLS_BLEND_DST_COLOR 2
#define GLS_BLEND_ONE_MINUS_DST_COLOR 3
#define GLS_BLEND_SRC_ALPHA 4
#define GLS_BLEND_ONE_MINUS_SRC_ALPHA 5
#define GLS_BLEND_SRC_COLOR 6
#define GLS_BLEND_ONE_MINUS_SRC_COLOR 7
#define GLS_BLEND_DST_ALPHA 8
#define GLS_BLEND_ONE_MINUS_DST_ALPHA 9

#define MAX_SHADER_STAGES 8
#define MAX_SHADER_DEFORMS 3
#define TR_MAX_TEXMODS 4

typedef enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH, 
	GF_INVERSE_SAWTOOTH, 

	GF_NOISE

} genFunc_t;


typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
} deform_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	// tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,				// standard fog
	CGEN_CONST				// fixed color
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_FOG,
	TCGEN_VECTOR			// S and T from world coordinates
} texCoordGen_t;

typedef struct {
	genFunc_t	func;

	float base;
	float amplitude;
	float phase;
	float frequency;
} waveForm_t;

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
} texMod_t;

typedef struct {
	deform_t	deformation;			// vertex coordinate modification type

	idVec3		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct {
	texMod_t		type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t		wave;

	// used for TMOD_TRANSFORM
	idMat2			matrix;		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	idVec2			translate;		// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
	idVec2			scale;			// s *= scale[0]
	                                    // t *= scale[1]

	// used for TMOD_SCROLL
	idVec2			scroll;			// s' = s + scroll[0] * time
										// t' = t + scroll[1] * time

	// + = clockwise
	// - = counterclockwise
	float			rotateSpeed;

} texModInfo_t;


#define	MAX_IMAGE_ANIMATIONS	8

typedef struct {
    std::string			image[MAX_IMAGE_ANIMATIONS];
	int				numImageAnimations;
	float			imageAnimationSpeed;

	texCoordGen_t	tcGen;
	idVec3			tcGenVectors[2];

	int				numTexMods;
	texModInfo_t	texMods[TR_MAX_TEXMODS];
} textureBundle_t;


typedef struct {
	textureBundle_t	bundle;

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	idVec4			constantColor;

    int             blendBits;

} shaderStage_t;

typedef struct {
    std::vector<deformStage_t> m_deforms;
    std::vector<shaderStage_t> m_stages;
} shader_t;

class Q3StagesBlock {
public:
    void Init( const std::vector<shaderStage_t> &stages ) { m_stages = stages; }
    void Pack( byte *pBuffer, const std::vector<int> &offsets ) const;

private:
    byte *PackInternal( byte *pBuffer, const waveForm_t &waveform ) const;
    byte *PackInternal( byte *pBuffer, const texModInfo_t &texMod ) const;
    byte *PackInternal( byte *pBuffer, const textureBundle_t &texBundle ) const;
    byte *PackInternal( byte *pBuffer, const shaderStage_t &shaderStage ) const;

    mutable int m_texIndex;
    std::vector<shaderStage_t> m_stages;
};

#endif
