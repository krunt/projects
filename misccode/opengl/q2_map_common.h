#ifndef Q2_MAP_COMMON__DEF_H
#define Q2_MAP_COMMON__DEF_H

#define	Q2_LUMP_ENTITIES		0
#define	Q2_LUMP_PLANES			1
#define	Q2_LUMP_VERTEXES		2
#define	Q2_LUMP_VISIBILITY		3
#define	Q2_LUMP_NODES			4
#define	Q2_LUMP_TEXINFO		5
#define	Q2_LUMP_FACES			6
#define	Q2_LUMP_LIGHTING		7
#define	Q2_LUMP_LEAFS			8
#define	Q2_LUMP_LEAFFACES		9
#define	Q2_LUMP_LEAFBRUSHES	10
#define	Q2_LUMP_EDGES			11
#define	Q2_LUMP_SURFEDGES		12
#define	Q2_LUMP_MODELS			13
#define	Q2_LUMP_BRUSHES		14
#define	Q2_LUMP_BRUSHSIDES		15
#define	Q2_LUMP_POP			16
#define	Q2_LUMP_AREAS			17
#define	Q2_LUMP_AREAPORTALS	18
#define	Q2_HEADER_LUMPS		19

#define Q2_IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
#define Q2_BSPVERSION 38

typedef struct
{
	int			ident;
	int			version;	
	lump_t		lumps[Q2_HEADER_LUMPS];
} q2_dheader_t;

#define	Q2_DVIS_PVS	0
#define	Q2_DVIS_PHS	1
typedef struct
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} q2_dvis_t;

typedef struct
{
	unsigned short	v[2];		// vertex numbers
} q2_dedge_t;

typedef struct
{
	float	point[3];
} q2_dvertex_t;

#define	MAXLIGHTMAPS	4
typedef struct
{
	unsigned short	planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;	
	short		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} q2_dface_t;

typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} q2_dplane_t;

typedef struct q2_texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/*.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} q2_texinfo_t;

typedef struct
{
	int				contents;			// OR of all brushes (not needed?)

	short			cluster;
	short			area;

	short			mins[3];			// for frustum culling
	short			maxs[3];

	unsigned short	firstleafface;
	unsigned short	numleaffaces;

	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
} q2_dleaf_t;

typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for frustom culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} q2_dnode_t;

#endif /* Q2_MAP_COMMON__DEF_H */
