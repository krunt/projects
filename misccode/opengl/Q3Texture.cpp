#include "q3_common.h"

#define qtrue true
#define qfalse false

#define MAX_TOKEN_CHARS 1024

/* I am lazy */
static  char    com_shadername[MAX_TOKEN_CHARS];
static	char	com_token[MAX_TOKEN_CHARS];
static	int		com_lines;

int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

int Q_stricmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

void Q_strncpyz( char *dest, const char *src, int destsize ) {
	strncpy( dest, src, destsize-1 );
    dest[destsize-1] = 0;
}

static char *SkipWhitespace( char *data, bool *hasNewLines ) {
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}

void SkipRestOfLine ( const char **data ) {
	char	*p;
	int		c;

	p = *data;
	while ( (c = *p++) != 0 ) {
		if ( c == '\n' ) {
			com_lines++;
			break;
		}
	}

	*data = p;
}

static char *COM_ParseExt( const char **data_p, bool allowLineBreaks )
{
	int c = 0, len;
	bool hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if ( c == '\n' )
			com_lines++;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
		len = 0;
	}
	com_token[len] = 0;

	*data_p = ( char * ) data;
	return com_token;
}

static bool ParseVector( const char **text, int count, float *v ) {
	char	*token;
	int		i;

	// FIXME: spaces are currently required after parens, should change parseext...
	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, "(" ) ) {
		msg_warning( "WARNING: missing parenthesis in shader '%s'\n", com_shadername );
		return qfalse;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			msg_warning( "WARNING: missing vector element in shader '%s'\n", com_shadername );
			return qfalse;
		}
		v[i] = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, ")" ) ) {
		msg_warning( "WARNING: missing parenthesis in shader '%s'\n", com_shadername );
		return qfalse;
	}

	return qtrue;
}

static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	msg_warning( "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, com_shadername );
	return GF_SIN;
}

static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	msg_warning( "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, com_shadername );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	msg_warning( "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, com_shadername );
	return GLS_DSTBLEND_ONE;
}



static void ParseWaveForm( const char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		msg_warning( "WARNING: missing waveform parm in shader '%s'\n", com_shadername );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		msg_warning( "WARNING: missing waveform parm in shader '%s'\n", com_shadername );
		return;
	}
	wave->base = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		msg_warning( "WARNING: missing waveform parm in shader '%s'\n", com_shadername );
		return;
	}
	wave->amplitude = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		msg_warning( "WARNING: missing waveform parm in shader '%s'\n", com_shadername );
		return;
	}
	wave->phase = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		msg_warning( "WARNING: missing waveform parm in shader '%s'\n", com_shadername );
		return;
	}
	wave->frequency = atof( token );
}

static void ParseTexMod( char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle.numTexMods == TR_MAX_TEXMODS ) {
		msg_failure( "ERROR: too many tcMod stages in shader '%s'\n", 
                com_shadername );
		return;
	}

	tmi = &stage->bundle.texMods[stage->bundle.numTexMods];
	stage->bundle.numTexMods++;

	token = COM_ParseExt( text, qfalse );

	//
	// turb
	//
	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing tcMod turb parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.base = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing tcMod turb in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing tcMod turb in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.phase = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing tcMod turb in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing scale parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->scale[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing scale parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->scale[1] = atof( token );
		tmi->type = TMOD_SCALE;
	}
	//
	// scroll
	//
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing scale scroll parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->scroll[0] = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing scale scroll parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->scroll[1] = atof( token );
		tmi->type = TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing stretch parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing stretch parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.base = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing stretch parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing stretch parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.phase = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing stretch parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->wave.frequency = atof( token );
		
		tmi->type = TMOD_STRETCH;
	}
	//
	// transform
	//
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->matrix[0][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->matrix[0][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->matrix[1][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->matrix[1][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->translate[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing transform parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->translate[1] = atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			msg_warning( "WARNING: missing tcMod rotate parms in shader '%s'\n", com_shadername );
			return;
		}
		tmi->rotateSpeed = atof( token );
		tmi->type = TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		msg_warning( "WARNING: unknown tcMod '%s' in shader '%s'\n", token, com_shadername );
	}
}

static bool ParseStage( shaderStage_t *stage, const char **text )
{
	char *token;

    int blendSrcBits = 0;
    int blendDstBits = 0;

    stage->rgbGen = CGEN_IDENTITY;
    stage->alphaGen = AGEN_IDENTITY;

    stage->bundle.numTexMods = 0;
    stage->bundle.numImageAnimations = 0;

    stage->bundle.tcGen = TCGEN_IDENTITY;

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			msg_warning( "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		//
		// map <name>
		//
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'map' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}

			if ( !Q_stricmp( token, "$whiteimage" ) )
			{
                /* don't support $whiteimage at a time */
                return false;
			}
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
                /* don't support $lightmap at a time */
                return false;
			}
			else
			{
                stage->bundle.image[0] = token;
                /*
				if ( !stage->bundle.image[0] )
				{
					msg_warning( "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, com_shadername );
					return qfalse;
				}
                */
			}
		}
		//
		// clampmap <name>
		//
		else if ( !Q_stricmp( token, "clampmap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}


            stage->bundle.image[0] = token;

            /*
			stage->bundle.image[0] = R_FindImageFile( token, !shader.noMipMaps, !shader.noPicMip, GL_CLAMP );
			if ( !stage->bundle.image[0] )
			{
				msg_warning( "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, com_shadername );
				return qfalse;
			}
            */
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}
			stage->bundle.imageAnimationSpeed = atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( 1 ) {
				int		num;

				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle.numImageAnimations;
				if ( num < MAX_IMAGE_ANIMATIONS ) {
					stage->bundle.image[num] = token;
                    /*
					if ( !stage->bundle.image[num] )
					{
						msg_warning( "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, com_shadername );
						return qfalse;
					}
                    */
					stage->bundle.numImageAnimations++;
				}
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
            /*
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'videoMmap' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}
			stage->bundle.videoMapHandle = ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
			if (stage->bundle.videoMapHandle != -1) {
				stage->bundle.isVideoMap = qtrue;
				stage->bundle.image[0] = tr.scratchImage[stage->bundle.videoMapHandle];
			}
            */

            /* don't support now */
            return false;
		}
		//
		// alphafunc <func>
		//
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}

			//atestBits = NameToAFunc( token );
		}
		//
		// depthFunc <func>
		//
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				msg_warning( "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", com_shadername );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				//depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				//depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				msg_warning( "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, com_shadername );
				continue;
			}
		}
		//
		// detail
		//
		else if ( !Q_stricmp( token, "detail" ) )
		{
			//stage->isDetail = qtrue;
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				msg_warning( "WARNING: missing parm for blendFunc in shader '%s'\n", com_shadername );
				continue;
			}
			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					msg_warning( "WARNING: missing parm for blendFunc in shader '%s'\n", com_shadername );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}
		}
		//
		// rgbGen
		//
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				msg_warning( "WARNING: missing parameters for rgbGen in shader '%s'\n", com_shadername );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				idVec3	color;

				ParseVector( text, 3, &color );
				stage->constantColor[0] = color[0];
				stage->constantColor[1] = color[1];
				stage->constantColor[2] = color[2];

				stage->rgbGen = CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = CGEN_VERTEX;
				if ( stage->alphaGen == 0 ) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				msg_warning( "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, com_shadername );
				continue;
			}
		}
		//
		// alphaGen 
		//
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				msg_warning( "WARNING: missing parameters for alphaGen in shader '%s'\n", com_shadername );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, qfalse );
				stage->constantColor[3] = 255 * atof( token );
				stage->alphaGen = AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
                /*
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					msg_warning( "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", com_shadername );
				}
				else
				{
					shader.portalRange = atof( token );
				}
                */
			}
			else
			{
				msg_warning( "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, com_shadername );
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				msg_warning( "WARNING: missing texgen parm in shader '%s'\n", com_shadername );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				stage->bundle.tcGen = TCGEN_ENVIRONMENT_MAPPED;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle.tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle.tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, &stage->bundle.tcGenVectors[0] );
				ParseVector( text, 3, &stage->bundle.tcGenVectors[1] );

				stage->bundle.tcGen = TCGEN_VECTOR;
			}
			else 
			{
				msg_warning( "WARNING: unknown texgen parm in shader '%s'\n", com_shadername );
			}
		}
		//
		// tcMod <type> <...>
		//
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
					break;
				strcat( buffer, token );
				strcat( buffer, " " );
			}

			ParseTexMod( buffer, stage );

			continue;
		}
		//
		// depthmask
		//
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			continue;
		}
		else
		{
			msg_warning( "WARNING: unknown parameter '%s' in shader '%s'\n", token, com_shadername );
			return qfalse;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if ( stage->rgbGen == CGEN_BAD ) {
		if ( blendSrcBits == 0 ||
			blendSrcBits == GLS_SRCBLEND_ONE || 
			blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = CGEN_IDENTITY;
		}
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && 
		 ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
	}

	// decide which agens we can skip
    /*
	if ( stage->alphaGen == CGEN_IDENTITY ) {
		if ( stage->rgbGen == CGEN_IDENTITY
			|| stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			stage->alphaGen = AGEN_SKIP;
		}
	}
    */

    stage->blendBits = ( blendDstBits << 5 ) | blendSrcBits;

	return qtrue;
}

bool Q3Material::ParseShader( const std::string &name ) {
    const char *textPtr, **text; 
    char *token;
    std::string contents;

    Q_strncpyz( com_shadername, name.c_str(), MAX_TOKEN_CHARS );
    com_lines = 0;

    BloatFile( name, contents );

    textPtr = name.c_str();
    text = &textPtr;

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		msg_warning( "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, com_shadername );
		return qfalse;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			msg_warning( "WARNING: no concluding '}' in shader %s\n", com_shadername );
			return qfalse;
		}

		// end of shader definition
		if ( token[0] == '}' )
		{
			break;
		}
		// stage definition
		else if ( token[0] == '{' )
		{
            shaderStage_t stage;
			if ( !ParseStage( &stage, text ) )
			{
				return qfalse;
			}
            m_shader.push_back( stage );
			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// sun parms
		else if ( !Q_stricmp( token, "q3map_sun" ) ) {
			float	a, b;

			token = COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );

		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
            /* TODO
      if (token[0]) {
        shader.clampTime = atof(token);
      }
      */
    }
		// skip stuff that only the q3map needs
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			continue;
		}
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) ) 
		{
			if ( !ParseVector( text, 3, shader.fogParms.color ) ) {
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) 
			{
				msg_warning( "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", com_shadername );
				continue;
			}

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// portal
		else if ( !Q_stricmp(token, "portal") )
		{
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp(token, "light") ) 
		{
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull") ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				msg_warning( "WARNING: missing cull parms in shader '%s'\n", com_shadername );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{

			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{

			}
			else
			{
				msg_warning( "WARNING: invalid cull parm '%s' in shader '%s'\n", token, com_shadername );
			}
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		else
		{
			msg_warning( "WARNING: unknown general shader parameter '%s' in '%s'\n", token, com_shadername );
			return qfalse;
		}
	}

	return qtrue;
}

bool Q3Material::Init( const std::string &name ) {
    if ( !ParseShader( name ) ) {
        return false;
    }

    /* precache textures */
    for ( int i = 0; i < m_shader.m_stages.size(); ++i ) {
        textureBundle_t &bundle = m_shader.m_stages[i].bundle;
        if ( !bundle.numImageAnimations ) {
            glTextureCache.Get( bundle.image[0] );
        } else {
            for ( int j = 0; j < bundle.numImageAnimations; ++j ) {
                glTextureCache.Get( bundle.image[j] );
            }
        }
    }

    std::vector<std::string> shaderList;
    shaderList.push_back( "shaders/q3.vs.glsl" );
    shaderList.push_back( "shaders/q3.ps.glsl" );

    if ( !m_program.Init( shaderList ) 
            || !m_shaderProgram.IsOk() ) {
        return false;
    }

    if ( !InitUniformVariables() ) {
        return false;
    }

    m_loadOk = true;
    return true;
}

bool Q3Material::InitUniformVariables( void ) {
    m_program.CreateUniformBuffer( "StagesBlock" );
    m_stagesBlock.Init( m_shader.m_stages );
    return true;
}

void Q3Material::BindTextures( const CommonMaterialParams &params ) {
    for ( int i = 0; i < m_shader.m_stages.size(); ++i ) {
        textureBundle_t &bundle = m_shader.m_stages[i].bundle;

        int bindIndex = 0;
        
        /* if animation */
        if ( bundle.numImageAnimations > 1 ) {
            bindIndex = GetAnimationIndexFromBundle( bundle, params.m_time );
        }

        boost::shared_ptr<GLTexture> 
            texPtr = glTextureCache.Get( bundle.image[bindIndex] );

        if ( !texPtr ) {
            msg_failure( "no texture with name `%s'\n", 
                    bundle.image[bindIndex].c_str() );
        }

        texPtr->Bind( i );
    }
}

void Q3StagesBlock::Pack( byte *pBuffer, const std::vector<GLint> &offsets ) const {
    byte *p = pBuffer;

    m_texIndex = 0;

    Std140GLSLPacker::Pack( pBuffer + offsets.at( 0 ), m_stages.size() );

    p = pBuffer + offsets.at( 1 );
    for ( int i = 0; i < m_stages.size(); ++i ) {
        p = PackInternal( p, m_stages[i] );
    }
}

template <>
struct Std140OpenglAlignmentHelper::AHelper<waveForm_t, 0> 
{
    static const int alignment = ROUNDUP(
        typename Std140OpenglAlignmentHelper::AHelper<GLint>::alignment, 16);

    static const int sizeUnaligned
        = typename Std140OpenglAlignmentHelper::AHelper<GLint>::size
            + 4 * typename Std140OpenglAlignmentHelper::AHelper<float>::size;

    static const int size = ROUNDUP(sizeUnaligned, alignment);
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<texModInfo_t, 0> 
{
    static const int alignment = ROUNDUP(
        typename Std140OpenglAlignmentHelper::AHelper<waveForm_t>::alignment, 16);

    static const int sizeUnaligned
        = typename Std140OpenglAlignmentHelper::AHelper<GLint>::size
            + typename Std140OpenglAlignmentHelper::AHelper<waveForm_t>::size
            + typename Std140OpenglAlignmentHelper::AHelper<idMat2>::size
            + 3 * typename Std140OpenglAlignmentHelper::AHelper<idVec2>::size
            + typename Std140OpenglAlignmentHelper::AHelper<float>::size;

    static const int size = ROUNDUP(sizeUnaligned, alignment);
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<textureBundle_t, 0> 
{
    static const int alignment = ROUNDUP(
        typename Std140OpenglAlignmentHelper::AHelper<texModInfo_t, 4>
        ::alignment, 16);

    static const int sizeUnaligned
        = 3 * typename Std140OpenglAlignmentHelper::AHelper<GLint>::size
        + typename Std140OpenglAlignmentHelper::AHelper<texModInfo_t, 4>::size;

    static const int size = ROUNDUP(sizeUnaligned, alignment);
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<shaderStage_t, 0> 
{
    static const int alignment = ROUNDUP(
        typename Std140OpenglAlignmentHelper::AHelper<textureBundle_t, 0>
        ::alignment, 16);

    static const int sizeUnaligned
        = typename Std140OpenglAlignmentHelper::AHelper<waveForm_t>::size
        + typename Std140OpenglAlignmentHelper::AHelper<GLint>::size
        + typename Std140OpenglAlignmentHelper::AHelper<waveForm_t>::size
        + typename Std140OpenglAlignmentHelper::AHelper<GLint>::size
        + typename Std140OpenglAlignmentHelper::AHelper<idVec4>::size
        + typename Std140OpenglAlignmentHelper::AHelper<textureBundle_t>::size
        + typename Std140OpenglAlignmentHelper::AHelper<GLint>::size;

    static const int size = ROUNDUP(sizeUnaligned, alignment);
};

byte *Q3StagesBlock::PackInternal( byte *pBuffer, 
        const waveForm_t &waveForm )
{
    byte *p = pBuffer;
    int alignValue = Std140OpenglAlignmentHelper::GetAlignment<waveForm_t>();

    p = AlignUp( p, alignValue );

    p = Std140GLSLPacker::Pack( p, (int)waveForm.func );
    p = Std140GLSLPacker::Pack( p, waveForm.base );
    p = Std140GLSLPacker::Pack( p, waveForm.amplitude );
    p = Std140GLSLPacker::Pack( p, waveForm.phase );
    p = Std140GLSLPacker::Pack( p, waveForm.frequency );

    return AlignUp( p, alignValue );
}

byte *Q3StagesBlock::PackInternal( byte *pBuffer, 
        const texModInfo_t &texMod )
{
    byte *p = pBuffer;
    int alignValue = Std140OpenglAlignmentHelper::GetAlignment<texModInfo_t>();

    p = AlignUp( p, alignValue );

    p = Std140GLSLPacker::Pack( p, (int)texMod.type );
    p = PackInternal( p, texMod.wave );
    p = Std140GLSLPacker::Pack( p, texMod.matrix );
    p = Std140GLSLPacker::Pack( p, texMod.translate );
    p = Std140GLSLPacker::Pack( p, texMod.scale );
    p = Std140GLSLPacker::Pack( p, texMod.scroll );
    p = Std140GLSLPacker::Pack( p, texMod.rotateSpeed );

    return AlignUp( p, alignValue );
}

byte *Q3StagesBlock::PackInternal( byte *pBuffer, 
        const textureBundle_t &texBundle )
{
    byte *p = pBuffer, *pp;
    int alignValue 
        = Std140OpenglAlignmentHelper::GetAlignment<textureBundle_t>();

    p = AlignUp( p, alignValue );

    p = Std140GLSLPacker::Pack( p, (int)m_texIndex++ );
    p = Std140GLSLPacker::Pack( p, (int)texBundle.tcGen );
    p = Std140GLSLPacker::Pack( p, (int)texBundle.numTexMods );

    p = AlignUp( p, 
        Std140OpenglAlignmentHelper::GetAlignment<texModInfo_t, 4>() );

    pp = p + Std140OpenglAlignmentHelper::GetSize<texModInfo_t, 4>();

    for ( int i = 0; i < texBundle.numTexMods; ++i ) {
        p = PackInternal( p, texBundle.m_texMods[i] );
    }

    return AlignUp( pp, alignValue );
}

byte *Q3StagesBlock::PackInternal( byte *pBuffer, 
        const shaderStage_t &stage )
{
    byte *p = pBuffer;
    int alignValue 
        = Std140OpenglAlignmentHelper::GetAlignment<shaderStage_t>();

    p = AlignUp( p, alignValue );

    p = PackInternal( p, stage.rgbWave );
    p = Std140GLSLPacker::Pack( p, (int)stage.rgbGen );

    p = PackInternal( p, stage.alphaWave );
    p = Std140GLSLPacker::Pack( p, (int)stage.alphaGen );

    p = Std140GLSLPacker::Pack( p, stage.m_constantColor );

    p = PackInternal( p, stage.bundle );

    p = Std140GLSLPacker::Pack( p, stage.blendBits );

    return AlignUp( p, alignValue );
}

void Q3Material::Bind( const CommonMaterialParams &params ) {
    assert( IsOk() );
    MaterialBase::Bind( params );
    BindTextures( params );
    m_program.Bind( "StagesBlock", m_stagesBlock );
}

void Q3Material::Unbind( void ) {
}

#undef qtrue
#undef qfalse
