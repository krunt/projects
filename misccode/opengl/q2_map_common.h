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

#endif /* Q2_MAP_COMMON__DEF_H */
