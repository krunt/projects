#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#define LittleShort(x) x
#define LittleLong(x) x
#define LittleFloat(x) x

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif

typedef unsigned char 		byte;
typedef enum {qfalse, qtrue}	qboolean;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))

static vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static 
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

static
vec_t VectorNormalize( vec3_t v ) {
	// NOTE: TTimo - Apple G4 altivec source uses double?
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if ( length ) {
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1/(float)sqrt (length);
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;
}

static
vec_t VectorNormalize2( const vec3_t v, vec3_t out) {
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if (length)
	{
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1/(float)sqrt (length);
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	} else {
		VectorClear( out );
	}
		
	return length;
}


#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
#define	PLANE_NON_AXIAL	3

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : \
        (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )

typedef struct {
	float		normal[3];
	float		dist;
} dplane_t;

typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;
	byte	signbits;
	byte	pad[2];
} cplane_t;

#define	MAX_QPATH 64
typedef struct {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contentFlags;
} dshader_t;

typedef enum {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	SF_MD4,
	SF_MDR,
	SF_IQM,
	SF_FLARE,
	SF_ENTITY,
	SF_DISPLAY_LIST,

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff			// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
} srfFlare_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE
} mapSurfaceType_t;

typedef struct msurface_s {
	int					viewCount;		// if == tr.viewCount, already added
	struct shader_s		*shader;
	int					fogIndex;

	surfaceType_t		*data;			// any of srf*_t
} msurface_t;

#define	VERTEXSIZE	8
typedef struct {
	surfaceType_t	surfaceType;
	cplane_t	plane;

	// dynamic lighting information
	int			dlightBits;

	// triangle definitions (no normals at points)
	int			numPoints;
	int			numIndices;
	int			ofsIndices;
	float		points[1][VERTEXSIZE];	// variable sized
} srfSurfaceFace_t;


typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[2];
	vec3_t		normal;
	byte		color[4];
} drawVert_t;

typedef struct srfGridMesh_s {
	surfaceType_t	surfaceType;

	// dynamic lighting information
	int				dlightBits;

	// culling information
	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;
	int				lodFixed;
	int				lodStitched;

	// vertexes
	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
	drawVert_t		verts[1];		// variable sized
} srfGridMesh_t;

typedef struct {
	surfaceType_t	surfaceType;

	// dynamic lighting information
	int				dlightBits;

	// culling information (FIXME: use this!)
	vec3_t			bounds[2];
	vec3_t			localOrigin;
	float			radius;

	// triangle definitions
	int				numIndexes;
	int				*indexes;

	int				numVerts;
	drawVert_t		*verts;
} srfTriangles_t;

typedef struct {
	int			planeNum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	int			mins[3];		// for frustom culling
	int			maxs[3];
} dnode_t;

typedef struct {
	int			cluster;	// -1 = opaque cluster (do I still store these?)
	int			area;

	int			mins[3];	// for frustum culling
	int			maxs[3];

	int			firstLeafSurface;
	int			numLeafSurfaces;

	int			firstLeafBrush;
	int			numLeafBrushes;
} dleaf_t;

#define	CONTENTS_NODE		-1
typedef struct mnode_s {
	// common with leaf and node
	int			contents;		// -1 for nodes, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	vec3_t		mins, maxs;		// for bounding box culling
	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];	

	// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mnode_t;

typedef struct {
	float		mins[3], maxs[3];
	int			firstSurface, numSurfaces;
	int			firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	vec3_t		bounds[2];		// for culling
	msurface_t	*firstSurface;
	int			numSurfaces;
} bmodel_t;

typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MD4,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	// model name

	int			flags;

	int			numFrames;
	int			numTags;			
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;			// offset for first frame
	int			ofsTags;			// numFrames * numTags
	int			ofsSurfaces;		// first surface, others follow

	int			ofsEnd;				// end of file
} md3Header_t;

#define MD3_MAX_LODS		3
typedef struct model_s {
	char		name[MAX_QPATH];
	modtype_t	type;
	int			index;		// model = tr.models[model->index]

	int			dataSize;	// just for listing purposes
	bmodel_t	*bmodel;		// only if type == MOD_BRUSH
	md3Header_t	*md3[MD3_MAX_LODS];	// only if type == MOD_MESH
	void	*modelData;			// only if type == (MOD_MD4 | MOD_MDR | MOD_IQM)

	int			 numLods;
} model_t;

typedef struct {
	int			shaderNum;
	int			fogNum;
	int			surfaceType;

	int			firstVert;
	int			numVerts;

	int			firstIndex;
	int			numIndexes;

	int			lightmapNum;
	int			lightmapX, lightmapY;
	int			lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];	// for patches, [0] and [1] are lodbounds

	int			patchWidth;
	int			patchHeight;
} dsurface_t;

typedef enum
{
	IMGTYPE_COLORALPHA, // for color, lightmap, diffuse, and specular
	IMGTYPE_NORMAL,
	IMGTYPE_NORMALHEIGHT,
	IMGTYPE_DELUXE, // normals are swizzled, deluxe are not
} imgType_t;

typedef enum
{
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CUBEMAP        = 0x0004,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_CLAMPTOEDGE    = 0x0040,
	IMGFLAG_SRGB           = 0x0080,
	IMGFLAG_GENNORMALMAP   = 0x0100,
} imgFlags_t;

typedef struct image_s {
	char		imgName[MAX_QPATH];		// game path, including extension
	int			width, height;				// source image
	int			uploadWidth, uploadHeight;
	int		    texnum;				

	int			frameUsed;			// for texture usage in frame statistics

	int			internalFormat;
	int			TMU;				// only needed for voodoo2

	imgType_t   type;
	imgFlags_t  flags;

	struct image_s*	next;
} image_t;

typedef struct shader_s {
	char		name[MAX_QPATH];		// game path, including extension
	int			lightmapIndex;			// for a shader to match, both name and lightmapIndex must match

	int			index;					// this shader == tr.shaders[index]
	int			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]

	float		sort;					// lower numbered shaders draw before higher numbered

	qboolean	defaultShader;			// we want to return index 0 if the shader failed to
										// load for some reason, but R_FindShader should
										// still keep a name allocated for it, so if
										// something calls RE_RegisterShader again with
										// the same name, we don't try looking for it again

	qboolean	explicitlyDefined;		// found in a .shader file

	int			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
	int			contentFlags;

	qboolean	entityMergable;			// merge across entites optimizable (smoke, blood)

	qboolean	isSky;

	float		portalRange;			// distance to fog out at

	int			multitextureEnv;		// 0, GL_MODULATE, GL_ADD (FIXME: put in stage)

	qboolean	polygonOffset;			// set for decals and other items that must be offset 
	qboolean	noMipMaps;				// for console fonts, 2D elements, etc.
	qboolean	noPicMip;				// for images that must always be full resolution

	qboolean	needsNormal;			// not all shaders will need all data to be gathered
	qboolean	needsST1;
	qboolean	needsST2;
	qboolean	needsColor;

	void		(*optimalStageIteratorFunc)( void );

  float clampTime;                                  // time this shader is clamped to
  float timeOffset;                                 // current time offset for this shader

  int numStates;                                    // if non-zero this is a state shader
  struct shader_s *currentShader;                   // current state if this is a state shader
  struct shader_s *parentShader;                    // current state if this is a state shader
  int currentState;                                 // current state index for cycle purposes
  long expireTime;                                  // time in milliseconds this expires

  struct shader_s *remappedShader;                  // current shader this one is remapped too

	struct	shader_s	*next;
} shader_t;

typedef struct {
	char		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp

	int			numShaders;
	dshader_t	*shaders;

	bmodel_t	*bmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numnodes;		// includes leafs
	int			numDecisionNodes;
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];
	byte		*lightGridData;

	int			numClusters;
	int			clusterBytes;
	const byte	*vis;			

	byte		*novis;			// clusterBytes of 0xff
} world_t;

typedef struct {
	int						visCount;		// incremented every time a new vis cluster is entered
	int						frameCount;		// incremented every frame
	int						sceneCount;		// incremented every scene
	int						viewCount;		// incremented every view (twice a scene if portaled)
											// and every R_MarkFragments call

	int						frameSceneNum;	// zeroed at RE_BeginFrame

	qboolean				worldMapLoaded;
	world_t					*world;

	const byte				*externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t					*defaultImage;
	image_t					*scratchImage[32];
	image_t					*fogImage;
	image_t					*dlightImage;	// inverse-quare highlight for projective adding
	image_t					*flareImage;
	image_t					*whiteImage;			// full of 0xff
	image_t					*identityLightImage;	// full of tr.identityLightByte

	shader_t				*defaultShader;
	shader_t				*shadowShader;
	shader_t				*projectionShadowShader;

	shader_t				*flareShader;
	shader_t				*sunShader;

	int						numLightmaps;
	image_t					**lightmaps;

	int						currentEntityNum;
	int						shiftedEntityNum;	// currentEntityNum << QSORT_REFENTITYNUM_SHIFT
	model_t					*currentModel;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	int						identityLightByte;	// identityLight * 255
	int						overbrightBits;		// r_overbrightBits->integer, but set to 0 if no hw gamma

	int						viewCluster;

	vec3_t					sunLight;			// from the sky shader for this level
	vec3_t					sunDirection;

	int						frontEndMsec;		// not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
#define	MAX_MOD_KNOWN	1024
	model_t					*models[MAX_MOD_KNOWN];
	int						numModels;
#define	MAX_DRAWIMAGES			2048
	int						numImages;
	image_t					*images[MAX_DRAWIMAGES];

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
#define SHADERNUM_BITS	14
#define MAX_SHADERS		(1<<SHADERNUM_BITS)
	int						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];

#define	MAX_DRAWIMAGES			2048
#define	MAX_SKINS				1024
//	int						numSkins;
//	skin_t					*skins[MAX_SKINS];

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)
	float					sinTable[FUNCTABLE_SIZE];
	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];
	float					fogTable[FOG_TABLE_SIZE];
} trGlobals_t;

static trGlobals_t tr;


static world_t world;
static char *fileBase;

#define	MAX_GRID_SIZE	129
static void Transpose( int width, int height, drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j;
	drawVert_t	temp;

	if ( width > height ) {
		for ( i = 0 ; i < height ; i++ ) {
			for ( j = i + 1 ; j < width ; j++ ) {
				if ( j < height ) {
					// swap the value
					temp = ctrl[j][i];
					ctrl[j][i] = ctrl[i][j];
					ctrl[i][j] = temp;
				} else {
					// just copy
					ctrl[j][i] = ctrl[i][j];
				}
			}
		}
	} else {
		for ( i = 0 ; i < width ; i++ ) {
			for ( j = i + 1 ; j < height ; j++ ) {
				if ( j < width ) {
					// swap the value
					temp = ctrl[i][j];
					ctrl[i][j] = ctrl[j][i];
					ctrl[j][i] = temp;
				} else {
					// just copy
					ctrl[i][j] = ctrl[j][i];
				}
			}
		}
	}
}

static void LerpDrawVert( drawVert_t *a, drawVert_t *b, drawVert_t *out ) {
	out->xyz[0] = 0.5f * (a->xyz[0] + b->xyz[0]);
	out->xyz[1] = 0.5f * (a->xyz[1] + b->xyz[1]);
	out->xyz[2] = 0.5f * (a->xyz[2] + b->xyz[2]);

	out->st[0] = 0.5f * (a->st[0] + b->st[0]);
	out->st[1] = 0.5f * (a->st[1] + b->st[1]);

	out->lightmap[0] = 0.5f * (a->lightmap[0] + b->lightmap[0]);
	out->lightmap[1] = 0.5f * (a->lightmap[1] + b->lightmap[1]);

	out->color[0] = (a->color[0] + b->color[0]) >> 1;
	out->color[1] = (a->color[1] + b->color[1]) >> 1;
	out->color[2] = (a->color[2] + b->color[2]) >> 1;
	out->color[3] = (a->color[3] + b->color[3]) >> 1;
}

static void PutPointsOnCurve( drawVert_t	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], 
							 int width, int height ) {
	int			i, j;
	drawVert_t	prev, next;

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 1 ; j < height ; j += 2 ) {
			LerpDrawVert( &ctrl[j][i], &ctrl[j+1][i], &prev );
			LerpDrawVert( &ctrl[j][i], &ctrl[j-1][i], &next );
			LerpDrawVert( &prev, &next, &ctrl[j][i] );
		}
	}


	for ( j = 0 ; j < height ; j++ ) {
		for ( i = 1 ; i < width ; i += 2 ) {
			LerpDrawVert( &ctrl[j][i], &ctrl[j][i+1], &prev );
			LerpDrawVert( &ctrl[j][i], &ctrl[j][i-1], &next );
			LerpDrawVert( &prev, &next, &ctrl[j][i] );
		}
	}
}

typedef struct {
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_SHADERS		1
#define	LUMP_PLANES			2
#define	LUMP_NODES			3
#define	LUMP_LEAFS			4
#define	LUMP_LEAFSURFACES	5
#define	LUMP_LEAFBRUSHES	6
#define	LUMP_MODELS			7
#define	LUMP_BRUSHES		8
#define	LUMP_BRUSHSIDES		9
#define	LUMP_DRAWVERTS		10
#define	LUMP_DRAWINDEXES	11
#define	LUMP_FOGS			12
#define	LUMP_SURFACES		13
#define	LUMP_LIGHTMAPS		14
#define	LUMP_LIGHTGRID		15
#define	LUMP_VISIBILITY		16
#define	HEADER_LUMPS		17

typedef struct {
	int			ident;
	int			version;

	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

#define BSP_IDENT   (('P'<<24)+('S'<<16)+('B'<<8)+'I')
#define BSP_VERSION 46

void *HunkAlloc(int size) {
    return (void*)malloc(size);
}
void HunkFree(void *p) {
    free(p);
}
void *BloatFile(const char *name) {
    int size; void *res;
    FILE *fd = fopen(name, "rb");
    if (!fd) { return NULL; }
    fseek(fd, 0, SEEK_END);
    size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    res = HunkAlloc(size);
    fread(res, 1, size, fd);
    return res;
}
void Ferror(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static	void R_LoadPlanes( lump_t *l ) {
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Ferror("LoadMap: funny lump size in %s", world.name);

	count = l->filelen / sizeof(*in);
	out = HunkAlloc ( count*2*sizeof(*out) );
	
	world.planes = out;
	world.numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++) {
		bits = 0;
		for (j=0 ; j<3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1<<j;
			}
		}

		out->dist = LittleFloat (in->dist);
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

static	void R_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	R_SetParent (node->children[0], node);
	R_SetParent (node->children[1], node);
}

static	void R_LoadNodesAndLeafs (lump_t *nodeLump, lump_t *leafLump) {
	int			i, j, p;
	dnode_t		*in;
	dleaf_t		*inLeaf;
	mnode_t 	*out;
	int			numNodes, numLeafs;

	in = (void *)(fileBase + nodeLump->fileofs);
	if (nodeLump->filelen % sizeof(dnode_t) ||
		leafLump->filelen % sizeof(dleaf_t) ) {
		Ferror("LoadMap: funny lump size in %s",world.name);
	}
	numNodes = nodeLump->filelen / sizeof(dnode_t);
	numLeafs = leafLump->filelen / sizeof(dleaf_t);

	out = HunkAlloc( (numNodes + numLeafs) * sizeof(*out) );

	world.nodes = out;
	world.numnodes = numNodes + numLeafs;
	world.numDecisionNodes = numNodes;

	// load nodes
	for ( i=0 ; i<numNodes; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (in->mins[j]);
			out->maxs[j] = LittleLong (in->maxs[j]);
		}
	
		p = LittleLong(in->planeNum);
		out->plane = world.planes + p;

		out->contents = CONTENTS_NODE;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = world.nodes + p;
			else
				out->children[j] = world.nodes + numNodes + (-1 - p);
		}
	}
	
	// load leafs
	inLeaf = (void *)(fileBase + leafLump->fileofs);
	for ( i=0 ; i<numLeafs ; i++, inLeaf++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (inLeaf->mins[j]);
			out->maxs[j] = LittleLong (inLeaf->maxs[j]);
		}

		out->cluster = LittleLong(inLeaf->cluster);
		out->area = LittleLong(inLeaf->area);

		if ( out->cluster >= world.numClusters ) {
			world.numClusters = out->cluster + 1;
		}

		out->firstmarksurface = world.marksurfaces +
			LittleLong(inLeaf->firstLeafSurface);
		out->nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);
	}	

	// chain decendants
	R_SetParent (world.nodes, NULL);
}

static	void R_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	int		*in;
	msurface_t **out;
	
	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Ferror("LoadMap: funny lump size in %s",world.name);

	count = l->filelen / sizeof(*in);
	out = HunkAlloc( count*sizeof(*out) );

	world.marksurfaces = out;
	world.nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong(in[i]);
		out[i] = world.surfaces + j;
	}
}

static	void R_LoadVisibility( lump_t *l ) {
	int		len;
	byte	*buf;
	byte	*dest;

	len = ( world.numClusters + 63 ) & ~63;
	world.novis = HunkAlloc( len );
	memset( world.novis, 0xff, len );

    len = l->filelen;
	if ( !len ) {
		return;
	}
	buf = fileBase + l->fileofs;

	world.numClusters = LittleLong( ((int *)buf)[0] );
	world.clusterBytes = LittleLong( ((int *)buf)[1] );

    dest = HunkAlloc( len - 8 );
	memcpy( dest, buf + 8, len - 8 );
	world.vis = dest;
}

static	void R_LoadShaders( lump_t *l ) {
	int		i, count;
	dshader_t	*in, *out;
	
	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Ferror("LoadMap: funny lump size in %s",world.name);
	count = l->filelen / sizeof(*in);
	out = HunkAlloc( count*sizeof(*out));

	world.shaders = out;
	world.numShaders = count;

	memcpy( out, in, count*sizeof(*out) );

	for ( i=0 ; i<count ; i++ ) {
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}

static model_t *R_AllocModel( void ) {
	model_t		*mod;

	if ( tr.numModels == MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = HunkAlloc( sizeof( *tr.models[tr.numModels] ) );
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

static	void R_LoadSubmodels( lump_t *l ) {
	dmodel_t	*in;
	bmodel_t	*out;
	int			i, j, count;

	in = (void *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Ferror("LoadMap: funny lump size in %s",world.name);
	count = l->filelen / sizeof(*in);

	world.bmodels = out = HunkAlloc( count * sizeof(*out) );

	for ( i=0 ; i<count ; i++, in++, out++ ) {
		model_t *model;

		model = R_AllocModel();

		assert( model != NULL );			// this should never happen
		if ( model == NULL ) {
			Ferror("R_LoadSubmodels: R_AllocModel() failed");
		}

		model->type = MOD_BRUSH;
		model->bmodel = out;
		snprintf( model->name, sizeof( model->name ), "*%d", i );

		for (j=0 ; j<3 ; j++) {
			out->bounds[0][j] = LittleFloat (in->mins[j]);
			out->bounds[1][j] = LittleFloat (in->maxs[j]);
		}

		out->firstSurface = world.surfaces + LittleLong( in->firstSurface );
		out->numSurfaces = LittleLong( in->numSurfaces );
	}
}

void R_LoadLightGrid( lump_t *l ) {
	int		i;
	vec3_t	maxs;
	int		numGridPoints;
	world_t	*w;
	float	*wMins, *wMaxs;

	w = &world;

	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;
	w->lightGridInverseSize[0] = 1.0f / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0f / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0f / w->lightGridSize[2];

	wMins = w->bmodels[0].bounds[0];
	wMaxs = w->bmodels[0].bounds[1];

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridOrigin[i] 
            = w->lightGridSize[i] * ceil( wMins[i] / w->lightGridSize[i] );
		maxs[i] = w->lightGridSize[i] * floor( wMaxs[i] / w->lightGridSize[i] );
		w->lightGridBounds[i] = 
            (maxs[i] - w->lightGridOrigin[i])/w->lightGridSize[i] + 1;
	}

	numGridPoints = w->lightGridBounds[0] 
        * w->lightGridBounds[1] * w->lightGridBounds[2];

	if ( l->filelen != numGridPoints * 8 ) {
		Ferror("WARNING: light grid mismatch\n");
	}

	w->lightGridData = HunkAlloc( l->filelen );
	memcpy( w->lightGridData, (void *)(fileBase + l->fileofs), l->filelen );
}

static void ClearBounds( vec3_t mins, vec3_t maxs ) {
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}
static void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	if ( v[0] < mins[0] ) {
		mins[0] = v[0];
	}
	if ( v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if ( v[1] < mins[1] ) {
		mins[1] = v[1];
	}
	if ( v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if ( v[2] < mins[2] ) {
		mins[2] = v[2];
	}
	if ( v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}

static void ParseTriSurf( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes ) {
	srfTriangles_t	*tri;
	int				i, j;
	int				numVerts, numIndexes;

	// get fog volume
	//surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader
	//surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );

	numVerts = LittleLong( ds->numVerts );
	numIndexes = LittleLong( ds->numIndexes );

	tri = HunkAlloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] ) 
		+ numIndexes * sizeof( tri->indexes[0] ));
	tri->surfaceType = SF_TRIANGLES;
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (drawVert_t *)(tri + 1);
	tri->indexes = (int *)(tri->verts + tri->numVerts );

	surf->data = (surfaceType_t *)tri;

	// copy vertexes
	ClearBounds( tri->bounds[0], tri->bounds[1] );
	verts += LittleLong( ds->firstVert );
	for ( i = 0 ; i < numVerts ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tri->verts[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			tri->verts[i].normal[j] = LittleFloat( verts[i].normal[j] );
		}
		AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );
			tri->verts[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}
	}

	// copy indexes
	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = LittleLong( indexes[i] );
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			Ferror("Bad index in triangle surface" );
		}
	}
}

static void MakeMeshNormals( int width, int height, drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j, k, dist;
	vec3_t	normal;
	vec3_t	sum;
	int		count = 0;
	vec3_t	base;
	vec3_t	delta;
	int		x, y;
	drawVert_t	*dv;
	vec3_t		around[8], temp;
	qboolean	good[8];
	qboolean	wrapWidth, wrapHeight;
	float		len;
static	int	neighbors[8][2] = {
	{0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}
	};

	wrapWidth = qfalse;
	for ( i = 0 ; i < height ; i++ ) {
		VectorSubtract( ctrl[i][0].xyz, ctrl[i][width-1].xyz, delta );
		len = VectorLengthSquared( delta );
		if ( len > 1.0 ) {
			break;
		}
	}
	if ( i == height ) {
		wrapWidth = qtrue;
	}

	wrapHeight = qfalse;
	for ( i = 0 ; i < width ; i++ ) {
		VectorSubtract( ctrl[0][i].xyz, ctrl[height-1][i].xyz, delta );
		len = VectorLengthSquared( delta );
		if ( len > 1.0 ) {
			break;
		}
	}
	if ( i == width) {
		wrapHeight = qtrue;
	}


	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			count = 0;
			dv = &ctrl[j][i];
			VectorCopy( dv->xyz, base );
			for ( k = 0 ; k < 8 ; k++ ) {
				VectorClear( around[k] );
				good[k] = qfalse;

				for ( dist = 1 ; dist <= 3 ; dist++ ) {
					x = i + neighbors[k][0] * dist;
					y = j + neighbors[k][1] * dist;
					if ( wrapWidth ) {
						if ( x < 0 ) {
							x = width - 1 + x;
						} else if ( x >= width ) {
							x = 1 + x - width;
						}
					}
					if ( wrapHeight ) {
						if ( y < 0 ) {
							y = height - 1 + y;
						} else if ( y >= height ) {
							y = 1 + y - height;
						}
					}

					if ( x < 0 || x >= width || y < 0 || y >= height ) {
						break;					// edge of patch
					}
					VectorSubtract( ctrl[y][x].xyz, base, temp );
					if ( VectorNormalize2( temp, temp ) == 0 ) {
						continue;				// degenerate edge, get more dist
					} else {
						good[k] = qtrue;
						VectorCopy( temp, around[k] );
						break;					// good edge
					}
				}
			}

			VectorClear( sum );
			for ( k = 0 ; k < 8 ; k++ ) {
				if ( !good[k] || !good[(k+1)&7] ) {
					continue;	// didn't get two points
				}
				CrossProduct( around[(k+1)&7], around[k], normal );
				if ( VectorNormalize2( normal, normal ) == 0 ) {
					continue;
				}
				VectorAdd( normal, sum, sum );
				count++;
			}
			//if ( count == 0 ) {
			//	printf("bad normal\n");
			//}
			VectorNormalize2( sum, dv->normal );
		}
	}
}

static
srfGridMesh_t *R_CreateSurfaceGridMesh(int width, int height,
								drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], float errorTable[2][MAX_GRID_SIZE] ) {
	int i, j, size;
	drawVert_t	*vert;
	vec3_t		tmpVec;
	srfGridMesh_t *grid;

	// copy the results out to a grid
	size = (width * height - 1) * sizeof( drawVert_t ) + sizeof( *grid );

	grid = HunkAlloc( size );
	memset(grid, 0, size);

	grid->widthLodError = HunkAlloc( width * 4 );
	memcpy( grid->widthLodError, errorTable[0], width * 4 );

	grid->heightLodError = HunkAlloc( height * 4 );
	memcpy( grid->heightLodError, errorTable[1], height * 4 );

	grid->width = width;
	grid->height = height;
	grid->surfaceType = SF_GRID;
	ClearBounds( grid->meshBounds[0], grid->meshBounds[1] );
	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			vert = &grid->verts[j*width+i];
			*vert = ctrl[j][i];
			AddPointToBounds( vert->xyz, grid->meshBounds[0], grid->meshBounds[1] );
		}
	}

	// compute local origin and bounds
	VectorAdd( grid->meshBounds[0], grid->meshBounds[1], grid->localOrigin );
	VectorScale( grid->localOrigin, 0.5f, grid->localOrigin );
	VectorSubtract( grid->meshBounds[0], grid->localOrigin, tmpVec );
	grid->meshRadius = VectorLength( tmpVec );

	VectorCopy( grid->localOrigin, grid->lodOrigin );
	grid->lodRadius = grid->meshRadius;
	//
	return grid;
}

#define drawVert_t_cleared(x) drawVert_t (x) = {{0, 0, 0}, {0, 0}, {0, 0}, {0, 0, 0}, {0, 0, 0, 0}}
#define	MAX_PATCH_SIZE		32	
#define	MAX_FACE_POINTS		64
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
static
srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] ) {
	int			i, j, k, l;
	drawVert_t_cleared( prev );
	drawVert_t_cleared( next );
	drawVert_t_cleared( mid );
	float		len, maxLen;
	int			dir;
	int			t;
	drawVert_t	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE];
	float		errorTable[2][MAX_GRID_SIZE];

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			ctrl[j][i] = points[j*width+i];
		}
	}

	for ( dir = 0 ; dir < 2 ; dir++ ) {

		for ( j = 0 ; j < MAX_GRID_SIZE ; j++ ) {
			errorTable[dir][j] = 0;
		}

		// horizontal subdivisions
		for ( j = 0 ; j + 2 < width ; j += 2 ) {

			maxLen = 0;
			for ( i = 0 ; i < height ; i++ ) {
				vec3_t		midxyz;
				vec3_t		midxyz2;
				vec3_t		dir;
				vec3_t		projected;
				float		d;

				// calculate the point on the curve
				for ( l = 0 ; l < 3 ; l++ ) {
					midxyz[l] = (ctrl[i][j].xyz[l] + ctrl[i][j+1].xyz[l] * 2
							+ ctrl[i][j+2].xyz[l] ) * 0.25f;
				}

				VectorSubtract( midxyz, ctrl[i][j].xyz, midxyz );
				VectorSubtract( ctrl[i][j+2].xyz, ctrl[i][j].xyz, dir );
				VectorNormalize( dir );

				d = DotProduct( midxyz, dir );
				VectorScale( dir, d, projected );
				VectorSubtract( midxyz, projected, midxyz2);
				len = VectorLengthSquared( midxyz2 );
				if ( len > maxLen ) {
					maxLen = len;
				}
			}

			maxLen = sqrt(maxLen);

			// if all the points are on the lines, remove the entire columns
			if ( maxLen < 0.1f ) {
				errorTable[dir][j+1] = 999;
				continue;
			}

			// see if we want to insert subdivided columns
			if ( width + 2 > MAX_GRID_SIZE ) {
				errorTable[dir][j+1] = 1.0f/maxLen;
				continue;	// can't subdivide any more
			}

			errorTable[dir][j+2] = 1.0f/maxLen;

			// insert two columns and replace the peak
			width += 2;
			for ( i = 0 ; i < height ; i++ ) {
				LerpDrawVert( &ctrl[i][j], &ctrl[i][j+1], &prev );
				LerpDrawVert( &ctrl[i][j+1], &ctrl[i][j+2], &next );
				LerpDrawVert( &prev, &next, &mid );

				for ( k = width - 1 ; k > j + 3 ; k-- ) {
					ctrl[i][k] = ctrl[i][k-2];
				}
				ctrl[i][j + 1] = prev;
				ctrl[i][j + 2] = mid;
				ctrl[i][j + 3] = next;
			}

			// back up and recheck this set again, it may need more subdivision
			j -= 2;

		}

		Transpose( width, height, ctrl );
		t = width;
		width = height;
		height = t;
	}


	// put all the aproximating points on the curve
	PutPointsOnCurve( ctrl, width, height );

	// cull out any rows or columns that are colinear
	for ( i = 1 ; i < width-1 ; i++ ) {
		if ( errorTable[0][i] != 999 ) {
			continue;
		}
		for ( j = i+1 ; j < width ; j++ ) {
			for ( k = 0 ; k < height ; k++ ) {
				ctrl[k][j-1] = ctrl[k][j];
			}
			errorTable[0][j-1] = errorTable[0][j];
		}
		width--;
	}

	for ( i = 1 ; i < height-1 ; i++ ) {
		if ( errorTable[1][i] != 999 ) {
			continue;
		}
		for ( j = i+1 ; j < height ; j++ ) {
			for ( k = 0 ; k < width ; k++ ) {
				ctrl[j-1][k] = ctrl[j][k];
			}
			errorTable[1][j-1] = errorTable[1][j];
		}
		height--;
	}

	// calculate normals
	MakeMeshNormals( width, height, ctrl );

	return R_CreateSurfaceGridMesh( width, height, ctrl, errorTable );
}

static void ParseMesh( dsurface_t *ds, drawVert_t *verts, msurface_t *surf ) {
	srfGridMesh_t	*grid;
	int				i, j;
	int				width, height, numPoints;
	drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	int				lightmapNum;
	vec3_t			bounds[2];
	vec3_t			tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	lightmapNum = LittleLong( ds->lightmapNum );

	// get fog volume
	//surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader value
	//surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( world.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	width = LittleLong( ds->patchWidth );
	height = LittleLong( ds->patchHeight );

	verts += LittleLong( ds->firstVert );
	numPoints = width * height;
	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			points[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			points[i].normal[j] = LittleFloat( verts[i].normal[j] );
		}
		for ( j = 0 ; j < 2 ; j++ ) {
			points[i].st[j] = LittleFloat( verts[i].st[j] );
			points[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}
	}

	// pre-tesseleate
	grid = R_SubdividePatchToGrid( width, height, points );
	surf->data = (surfaceType_t *)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	for ( i = 0 ; i < 3 ; i++ ) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );
}

static void SetPlaneSignbits (cplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}
static void ParseFace( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes  ) {
	int			i, j;
	srfSurfaceFace_t	*cv;
	int			numPoints, numIndexes;
	int			lightmapNum;
	int			sfaceSize, ofsIndexes;

	lightmapNum = LittleLong( ds->lightmapNum );

	// get fog volume
	//surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader value
	//surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );

	numPoints = LittleLong( ds->numVerts );
	if (numPoints > MAX_FACE_POINTS) {
		Ferror("WARNING: MAX_FACE_POINTS exceeded: %i\n", numPoints);
    }

	numIndexes = LittleLong( ds->numIndexes );

	// create the srfSurfaceFace_t
	sfaceSize = ( size_t ) &((srfSurfaceFace_t *)0)->points[numPoints];
	ofsIndexes = sfaceSize;
	sfaceSize += sizeof( int ) * numIndexes;

	cv = HunkAlloc( sfaceSize );
	cv->surfaceType = SF_FACE;
	cv->numPoints = numPoints;
	cv->numIndices = numIndexes;
	cv->ofsIndices = ofsIndexes;

	verts += LittleLong( ds->firstVert );
	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			cv->points[i][j] = LittleFloat( verts[i].xyz[j] );
		}
		for ( j = 0 ; j < 2 ; j++ ) {
			cv->points[i][3+j] = LittleFloat( verts[i].st[j] );
			cv->points[i][5+j] = LittleFloat( verts[i].lightmap[j] );
		}
	}

	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		((int *)((byte *)cv + cv->ofsIndices ))[i] = LittleLong( indexes[ i ] );
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
	cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	surf->data = (surfaceType_t *)cv;
}

static void ParseFlare( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes ) {
	srfFlare_t		*flare;
	int				i;

	// get fog volume
	//surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader
	//surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );

	flare = HunkAlloc( sizeof( *flare ) );
	flare->surfaceType = SF_FLARE;

	surf->data = (surfaceType_t *)flare;

	for ( i = 0 ; i < 3 ; i++ ) {
		flare->origin[i] = LittleFloat( ds->lightmapOrigin[i] );
		flare->color[i] = LittleFloat( ds->lightmapVecs[0][i] );
		flare->normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
}

static	void R_LoadSurfaces( lump_t *surfs, lump_t *verts, lump_t *indexLump ) {
	dsurface_t	*in;
	msurface_t	*out;
	drawVert_t	*dv;
	int			*indexes;
	int			count;
	int			numFaces, numMeshes, numTriSurfs, numFlares;
	int			i;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;

	in = (void *)(fileBase + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Ferror("LoadMap: funny lump size in %s",world.name);
	count = surfs->filelen / sizeof(*in);

	dv = (void *)(fileBase + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		Ferror("LoadMap: funny lump size in %s",world.name);

	indexes = (void *)(fileBase + indexLump->fileofs);
	if ( indexLump->filelen % sizeof(*indexes))
		Ferror("LoadMap: funny lump size in %s",world.name);

	out = HunkAlloc( count * sizeof(*out) );

	world.surfaces = out;
	world.numsurfaces = count;

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		switch ( LittleLong( in->surfaceType ) ) {
		case MST_PATCH:
			ParseMesh ( in, dv, out );
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv, out, indexes );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace( in, dv, out, indexes );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( in, dv, out, indexes );
			numFlares++;
			break;
		default:
			Ferror("Bad surfaceType");
		}
	}

	fprintf( stdout, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n", 
		numFaces, numMeshes, numTriSurfs, numFlares );
}



void RE_LoadWorldMap( const char *name ) {
	int			i;
	dheader_t	*header;
    char *bsp;

    bsp = BloatFile(name);
	if (!bsp) {
        Ferror("RE_LoadWorldMap: %s not found", name);
	}

    memset(&tr, 0, sizeof(tr));
    memset(&world, 0, sizeof(world));
    strncpy(world.name, name, sizeof(world.name));

	header = (dheader_t *)bsp;
	fileBase = (char *)header;

	i = header->version;
	if ( i != BSP_VERSION ) {
        Ferror("RE_LoadWorldMap: %s has wrong version number (%i should be %i)", 
			name, i, BSP_VERSION);
	}

	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	R_LoadPlanes (&header->lumps[LUMP_PLANES]);
	R_LoadNodesAndLeafs(&header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS]);
	R_LoadMarksurfaces (&header->lumps[LUMP_LEAFSURFACES]);
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	R_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES] );

    HunkFree(bsp);
}

static void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

static void R_RotateForViewer (void) 
{
	float	viewerMatrix[16];
	vec3_t	origin;

	Com_Memset (&tr.or, 0, sizeof(tr.or));
	tr.or.axis[0][0] = 1;
	tr.or.axis[1][1] = 1;
	tr.or.axis[2][2] = 1;
	VectorCopy (tr.viewParms.or.origin, tr.or.viewOrigin);

	// transform by the camera placement
	VectorCopy( tr.viewParms.or.origin, origin );

	viewerMatrix[0] = tr.viewParms.or.axis[0][0];
	viewerMatrix[4] = tr.viewParms.or.axis[0][1];
	viewerMatrix[8] = tr.viewParms.or.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.or.axis[1][0];
	viewerMatrix[5] = tr.viewParms.or.axis[1][1];
	viewerMatrix[9] = tr.viewParms.or.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.or.axis[2][0];
	viewerMatrix[6] = tr.viewParms.or.axis[2][1];
	viewerMatrix[10] = tr.viewParms.or.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.or.modelMatrix );
	tr.viewParms.world = tr.or;
}

static void R_SetupFrustum (viewParms_t *dest, float xmin, float xmax, float ymax, 
        float zProj, float stereoSep)
{
	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;
	
	if(stereoSep == 0 && xmin == -xmax)
	{
		// symmetric case can be simplified
		VectorCopy(dest->or.origin, ofsorigin);

		length = sqrt(xmax * xmax + zProj * zProj);
		oppleg = xmax / length;
		adjleg = zProj / length;

		VectorScale(dest->or.axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest->or.axis[1], dest->frustum[0].normal);

		VectorScale(dest->or.axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest->or.axis[1], dest->frustum[1].normal);
	}
	else
	{
		// In stereo rendering, due to the modification of the projection matrix, dest->or.origin is not the
		// actual origin that we're rendering so offset the tip of the view pyramid.
		VectorMA(dest->or.origin, stereoSep, dest->or.axis[1], ofsorigin);
	
		oppleg = xmax + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest->or.axis[1], dest->frustum[0].normal);

		oppleg = xmin + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], -oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -zProj / length, dest->or.axis[1], dest->frustum[1].normal);
	}

	length = sqrt(ymax * ymax + zProj * zProj);
	oppleg = ymax / length;
	adjleg = zProj / length;

	VectorScale(dest->or.axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest->or.axis[2], dest->frustum[2].normal);

	VectorScale(dest->or.axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, -adjleg, dest->or.axis[2], dest->frustum[3].normal);
	
	for (i=0 ; i<4 ; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct (ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits( &dest->frustum[i] );
	}
}

static void R_SetupProjection(viewParms_t *dest, float zProj, qboolean computeFrustum)
{
	float	xmin, xmax, ymin, ymax;
	float	width, height;

	ymax = zProj * tan(dest->fovY * M_PI / 360.0f);
	ymin = -ymax;

	xmax = zProj * tan(dest->fovX * M_PI / 360.0f);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	
	dest->projectionMatrix[0] = 2 * zProj / width;
	dest->projectionMatrix[4] = 0;
	dest->projectionMatrix[8] = (xmax + xmin + 2 * stereoSep) / width;
	dest->projectionMatrix[12] = 2 * zProj * stereoSep / width;

	dest->projectionMatrix[1] = 0;
	dest->projectionMatrix[5] = 2 * zProj / height;
	dest->projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	dest->projectionMatrix[13] = 0;

	dest->projectionMatrix[3] = 0;
	dest->projectionMatrix[7] = 0;
	dest->projectionMatrix[11] = -1;
	dest->projectionMatrix[15] = 0;
	
	if(computeFrustum)
		R_SetupFrustum(dest, xmin, xmax, ymax, zProj, stereoSep);
}

static void R_SetFarClip( void )
{
	float	farthestCornerDistance = 0;
	int		i;

	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	//
	// set far clipping planes dynamically
	//
	farthestCornerDistance = 0;
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		vec3_t vecTo;
		float distance;

		if ( i & 1 )
		{
			v[0] = tr.viewParms.visBounds[0][0];
		}
		else
		{
			v[0] = tr.viewParms.visBounds[1][0];
		}

		if ( i & 2 )
		{
			v[1] = tr.viewParms.visBounds[0][1];
		}
		else
		{
			v[1] = tr.viewParms.visBounds[1][1];
		}

		if ( i & 4 )
		{
			v[2] = tr.viewParms.visBounds[0][2];
		}
		else
		{
			v[2] = tr.viewParms.visBounds[1][2];
		}

		VectorSubtract( v, tr.viewParms.or.origin, vecTo );

		distance = vecTo[0] * vecTo[0] + vecTo[1] * vecTo[1] + vecTo[2] * vecTo[2];

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}
	tr.viewParms.zFar = sqrt( farthestCornerDistance );
}

static void R_SetupProjectionZ(viewParms_t *dest)
{
	float zNear, zFar, depth;
	
	zNear	= r_znear->value;
	zFar	= dest->zFar;	
	depth	= zFar - zNear;

	dest->projectionMatrix[2] = 0;
	dest->projectionMatrix[6] = 0;
	dest->projectionMatrix[10] = -( zFar + zNear ) / depth;
	dest->projectionMatrix[14] = -2 * zFar * zNear / depth;
}

static mnode_t *R_PointInLeaf( const vec3_t p ) {
	mnode_t		*node;
	float		d;
	cplane_t	*plane;
	
	if ( !tr.world ) {
		Ferror("R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while( 1 ) {
		if (node->contents != -1) {
			break;
		}
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}
	
	return node;
}

static const byte *R_ClusterPVS (int cluster) {
	if (!tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters ) {
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

static void R_MarkLeaves (void) {
	const byte	*vis;
	mnode_t	*leaf, *parent;
	int		i;
	int		cluster;

	// current viewcluster
	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	tr.visCount++;
	tr.viewCluster = cluster;

	vis = R_ClusterPVS (tr.viewCluster);
	
	for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		// check general pvs
		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		// check for door connection
		if ( (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7)) ) ) {
			continue;		// not visible
		}

		parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}

static
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist[2];
	int		sides, b, i;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	dist[0] = dist[1] = 0;
	if (p->signbits < 8) // >= 8: default case is original code (dist[0]=dist[1]=0)
	{
		for (i=0 ; i<3 ; i++)
		{
			b = (p->signbits >> i) & 1;
			dist[ b] += p->normal[i]*emaxs[i];
			dist[!b] += p->normal[i]*emins[i];
		}
	}

	sides = 0;
	if (dist[0] >= p->dist)
		sides = 1;
	if (dist[1] < p->dist)
		sides |= 2;

	return sides;
}

static qboolean	R_CullSurface( surfaceType_t *surface, shader_t *shader ) {
	srfSurfaceFace_t *sface;
	float			d;

	if ( *surface == SF_GRID ) {
		return R_CullGrid( (srfGridMesh_t *)surface );
	}

	if ( *surface == SF_TRIANGLES ) {
		return R_CullTriSurf( (srfTriangles_t *)surface );
	}

	if ( *surface != SF_FACE ) {
		return qfalse;
	}

	if ( shader->cullType == CT_TWO_SIDED ) {
		return qfalse;
	}

	sface = ( srfSurfaceFace_t * ) surface;
	d = DotProduct (tr.or.viewOrigin, sface->plane.normal);

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here 
	if ( shader->cullType == CT_FRONT_SIDED ) {
		if ( d < sface->plane.dist - 8 ) {
			return qtrue;
		}
	} else {
		if ( d > sface->plane.dist + 8 ) {
			return qtrue;
		}
	}

	return qfalse;
}

static
void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, 
				   int fogIndex, int dlightMap ) {
	int			index;

	// instead of checking for overflow, we just mask the index
	// so it wraps around
	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;
	// the sort data is packed into a single 32 bit value so it can be
	// compared quickly during the qsorting process
	tr.refdef.drawSurfs[index].sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
	tr.refdef.drawSurfs[index].surface = surface;
	tr.refdef.numDrawSurfs++;
}

static void R_AddWorldSurface( msurface_t *surf, int dlightBits ) {
	if ( surf->viewCount == tr.viewCount ) {
		return;		// already in this view
	}

	surf->viewCount = tr.viewCount;
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if ( R_CullSurface( surf->data, surf->shader ) ) {
		return;
	}

	// check for dlighting
	if ( dlightBits ) {
		dlightBits = R_DlightSurface( surf, dlightBits );
		dlightBits = ( dlightBits != 0 );
	}

	R_AddDrawSurf( surf->data, surf->shader, surf->fogIndex, dlightBits );
}

static void R_RecursiveWorldNode( mnode_t *node, int planeBits, int dlightBits ) {

	do {
		int			newDlights[2];

		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount) {
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if ( 1 ) {
			int		r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

		}

		if ( node->contents != -1 ) {
			break;
		}

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative

		// determine which dlights are needed
		newDlights[0] = 0;
		newDlights[1] = 0;
		if ( dlightBits ) {
			int	i;

			for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
				dlight_t	*dl;
				float		dist;

				if ( dlightBits & ( 1 << i ) ) {
					dl = &tr.refdef.dlights[i];
					dist = DotProduct( dl->origin, node->plane->normal ) - node->plane->dist;
					
					if ( dist > -dl->radius ) {
						newDlights[0] |= ( 1 << i );
					}
					if ( dist < dl->radius ) {
						newDlights[1] |= ( 1 << i );
					}
				}
			}
		}

		// recurse down the children, front side first
		R_RecursiveWorldNode (node->children[0], planeBits, newDlights[0] );

		// tail recurse
		node = node->children[1];
		dlightBits = newDlights[1];
	} while ( 1 );

	{
		// leaf node, so add mark surfaces
		int			c;
		msurface_t	*surf, **mark;

		tr.pc.c_leafs++;

		// add to z buffer bounds
		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface( surf, dlightBits );
			mark++;
		}
	}

}

void R_AddWorldSurfaces (void) {
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves ();

	// clear out the visible min/max
	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );

	// perform frustum culling and add all the potentially visible surfaces
	if ( tr.refdef.num_dlights > 32 ) {
		tr.refdef.num_dlights = 32 ;
	}
	R_RecursiveWorldNode( tr.world->nodes, 15, ( 1 << tr.refdef.num_dlights ) - 1 );
}

static void R_GenerateDrawSurfs( void ) {
	R_AddWorldSurfaces ();

	R_SetFarClip();

	R_SetupProjectionZ (&tr.viewParms);
}

void R_RenderView (viewParms_t *parms) {
	int		firstDrawSurf;

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;

	tr.viewCount++;

	// set viewParms.world
	R_RotateForViewer ();

	R_SetupProjection(&tr.viewParms, r_zproj->value, qtrue);

	R_GenerateDrawSurfs();

	R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf, tr.refdef.numDrawSurfs - firstDrawSurf );
}

int main(int argc, char **argv) {
    const char *bspfilename;

    if (argc != 2) {
        fprintf(stderr, "usage: <bsp-fname>\n");
        return 1;
    }

    bspfilename = argv[1];
    RE_LoadWorldMap(bspfilename);

    return 0;
}

