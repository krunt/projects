
#include "Q3Map.h"
#include "Utils.h"
#include "Camera.h"
#include "Render.h"

void Q3Map::Spawn( void ) {
    MyEntity::Spawn();

    if ( m_map.find("map") == m_map.end() ) {
        msg_warning0( "no map parameter in Q3Map options" );
        return;
    }

    RE_LoadWorldMap( m_map["map"].c_str() );

    m_viewCount = 0;
}

bool Q3Map::RE_LoadWorldMap( const char *name ) {
	int			i;
	dheader_t	*header;
	byte		*buffer;
    std::string contents;

    BloatFile( name, contents );

    if ( contents.empty() ) {
        msg_warning( "can't find map file `%s'\n", name );
        return false;
    }

    m_fileBase = buffer = (byte*)&contents[0];

    memset( &m_world, 0, sizeof( m_world ) );

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if ( i != 46 ) {
		msg_warning( "RE_LoadWorldMap: %s has wrong version number (%i should be %i)", 
			name, i, 46);
        return false;
	}

	// swap all the lumps
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++) {
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);
	}

    /*
	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS] );
	R_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES] );
	R_LoadMarksurfaces (&header->lumps[LUMP_LEAFSURFACES]);
	R_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	R_LoadEntities( &header->lumps[LUMP_ENTITIES] );
	R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );
    */

	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	R_LoadPlanes (&header->lumps[LUMP_PLANES]);
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], 
            &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES] );
    R_LoadMarksurfaces (&header->lumps[LUMP_LEAFSURFACES]);
	R_LoadNodesAndLeafs (&header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS]);
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );

    return true;
}

template <typename T>
static void ClearBounds(T &mins, T &maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

template <typename T, typename U>
static void AddPointToBounds(const T &v, U &mins, U &maxs)
{
	int		i;
	float	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

void Q3Map::ParseTriSurf( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf, int *indexes ) {
	srfTriangles_t	*tri;
	int				i, j;
	int				numVerts, numIndexes;

    /*
	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader
	surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}
    */

    /* for now, stub it */
    surf->shader = NULL;

	numVerts = LittleLong( ds->numVerts );
	numIndexes = LittleLong( ds->numIndexes );

	tri = (srfTriangles_t *)malloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] ) 
		+ numIndexes * sizeof( tri->indexes[0] ) );
	tri->surfaceType = SF_TRIANGLES;
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (q3drawVert_t *)(tri + 1);
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

        FromQ3( tri->verts[i].xyz, tri->verts[i].xyz );
        FromQ3( tri->verts[i].normal, tri->verts[i].normal );

		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );
			tri->verts[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}
	}

    FromQ3( tri->bounds[0].ToFloatPtr(), tri->bounds[0].ToFloatPtr() );
    FromQ3( tri->bounds[1].ToFloatPtr(), tri->bounds[1].ToFloatPtr() );

	// copy indexes
	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = LittleLong( indexes[i] );
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			msg_failure0( "Bad index in triangle surface" );
		}
	}
}

#define MAX_FACE_POINTS 64
void Q3Map::ParseFace( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf, int *indexes  ) {
	int			i, j;
	srfSurfaceFace_t	*cv;
	int			numPoints, numIndexes;
	int			lightmapNum;
	int			sfaceSize, ofsIndexes;

	lightmapNum = LittleLong( ds->lightmapNum );

    /*
	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;

	// get shader value
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}
    */

    /* stub it */
    surf->shader = NULL;

	numPoints = LittleLong( ds->numVerts );
	if (numPoints > MAX_FACE_POINTS) {
		msg_warning0( "WARNING: MAX_FACE_POINTS exceeded: \n" );

        numPoints = MAX_FACE_POINTS;
        //surf->shader = tr.defaultShader;
	}

	numIndexes = LittleLong( ds->numIndexes );

	// create the srfSurfaceFace_t
	sfaceSize = ( int ) &((srfSurfaceFace_t *)0)->points[numPoints];
	ofsIndexes = sfaceSize;
	sfaceSize += sizeof( int ) * numIndexes;

	cv = (srfSurfaceFace_t *)malloc( sfaceSize );
	cv->surfaceType = SF_FACE;
	cv->numPoints = numPoints;
	cv->numIndices = numIndexes;
	cv->ofsIndices = ofsIndexes;

	verts += LittleLong( ds->firstVert );
	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			cv->points[i][j] = LittleFloat( verts[i].xyz[j] );
		}
        FromQ3( &cv->points[i][0], &cv->points[i][0] );
		for ( j = 0 ; j < 2 ; j++ ) {
			cv->points[i][3+j] = LittleFloat( verts[i].st[j] );
			cv->points[i][5+j] = LittleFloat( verts[i].lightmap[j] );
		}
		//R_ColorShiftLightingBytes( verts[i].color, (byte *)&cv->points[i][7] );
	}

	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		((int *)((byte *)cv + cv->ofsIndices ))[i] = LittleLong( indexes[ i ] );
	}

	// take the plane information from the lightmap vector
	for ( i = 0 ; i < 3 ; i++ ) {
		cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
	cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );

    FromQ3Plane( cv->plane );

	surf->data = (surfaceType_t *)cv;

    /*
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );
    */
}

mnode_t *Q3Map::FindLeaf( const idVec3 &pos ) {
    assert( m_world.numnodes > 0 );

    int index;
    mnode_t *node = &m_world.nodes[0];

    while ( node && node->contents == -1 ) {
        float distance = pos * node->plane->normal;

        if ( distance > node->plane->dist ) {
            index = 0;
        } else {
            index = 1;
        }
        node = node->children[index];
    }

    return node;
}

byte *Q3Map::R_ClusterPVS( int cluster ) const {
    if ( !m_world.vis || cluster < 0 || cluster >= m_world.numClusters ) {
        return m_world.novis;
    }
    return m_world.vis + cluster * m_world.clusterBytes;
}

void Q3Map::MarkSurfaces( const idVec3 &pos ) {
    int i, cluster;
    mnode_t *parent, *leaf = FindLeaf( pos );
    byte *vis;

    if ( !leaf ) {
        msg_warning0( "didn't find mark-leaf" );
        return;
    }

    cluster = leaf->cluster;
    m_viewCount++;

    vis = R_ClusterPVS( cluster );

    for ( i = 0, leaf = m_world.nodes; i < m_world.numnodes; ++i, ++leaf ) {
        cluster = leaf->cluster;
        if ( cluster < 0 || cluster >= m_world.numClusters )
            continue;

        if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
            continue;
        }

        parent = leaf;
        do {
            if ( parent->visframe == m_viewCount )
                break;
            parent->visframe = m_viewCount;
            parent = parent->parent;
        } while (parent);
    }
}

void Q3Map::R_RecursiveWorldNode( const mnode_t *node ) {
    do {
        if ( node->visframe != m_viewCount )
            return;

        if ( node->contents != -1 )
            break;

        R_RecursiveWorldNode( node->children[0] );

        node = node->children[1];
    } while ( 1 );

    {
        int c;
        msurface_t *surf, **mark;

        mark = node->firstmarksurface;
        c = node->nummarksurfaces;

        while (c--) {
            surf = *mark;
            RenderSurface( surf );
            mark++;
        }

    }
}

void Q3Map::UncacheSurfaces( void ) {
    for ( int i = 0; i < m_surfs.size(); ++i ) {
        gl_render.UncacheSurface( m_surfs[i] );
    }
    m_surfs.clear();
    m_batch.clear();
}

void Q3Map::RenderTriangleSoup( msurface_t *s ) {
    surf_t surf;
    cached_surf_t cached_surf;
    srfTriangles_t *tri = (srfTriangles_t *)s->data;

    //fprintf( stderr, "RenderTriangleSoup(%d,%d)\n", tri->numIndexes, tri->numVerts );

    surf.m_matName = "white";
    //surf.m_indices.resize( tri->numIndexes );
    //memcpy( surf.m_indices.data(), tri->indexes, tri->numIndexes * sizeof(int) );
    for ( int i = 0; i < tri->numIndexes; ++i ) {
        surf.m_indices.push_back( tri->indexes[i] );
    }

    for ( int i = 0; i < tri->numVerts; ++i ) {
        q3drawVert_t *qv = &tri->verts[ i ];

        drawVert_t v;

        v.m_pos[0] = qv->xyz[0];
        v.m_pos[1] = qv->xyz[1];
        v.m_pos[2] = qv->xyz[2];

        v.m_tex[0] = qv->st[0];
        v.m_tex[1] = qv->st[1];

        v.m_normal[0] = qv->normal[0];
        v.m_normal[1] = qv->normal[1];
        v.m_normal[2] = qv->normal[2];

        surf.m_verts.push_back( v );
    }

    //gl_render.CacheSurface( surf, cached_surf );
    //m_surfs.push_back( cached_surf );

    BatchElement &b = m_batch[ s->shader ];
    b.m_indices.insert( b.m_indices.end(), surf.m_indices.begin(), surf.m_indices.end() );
    b.m_verts.insert( b.m_verts.end(), surf.m_verts.begin(), surf.m_verts.end() );
}

void Q3Map::RenderFace( msurface_t *s ) {
    surf_t surf;
    //cached_surf_t cached_surf;
    unsigned *indices;
    float *point;
    srfSurfaceFace_t *face = (srfSurfaceFace_t *)s->data;

    //fprintf( stderr, "RenderTriangleSoup(%d,%d)\n", tri->numIndexes, tri->numVerts );

    surf.m_matName = "white";

    point = face->points[0];

    indices = (unsigned *)( ( ( char * ) point ) + face->ofsIndices );

    for ( int i = 0; i < face->numIndices; ++i ) {
        surf.m_indices.push_back( indices[i] );
    }

    for ( int i = 0; i < face->numPoints; ++i, point += 8 ) {

        drawVert_t v;
        v.m_pos[0] = point[0];
        v.m_pos[1] = point[1];
        v.m_pos[2] = point[2];

        v.m_tex[0] = point[3];
        v.m_tex[1] = point[4];

        v.m_normal[0] = face->plane.normal[0];
        v.m_normal[1] = face->plane.normal[1];
        v.m_normal[2] = face->plane.normal[2];

        surf.m_verts.push_back( v );
    }

    BatchElement &b = m_batch[ s->shader ];
    b.m_indices.insert( b.m_indices.end(), surf.m_indices.begin(), surf.m_indices.end() );
    b.m_verts.insert( b.m_verts.end(), surf.m_verts.begin(), surf.m_verts.end() );

    //gl_render.CacheSurface( surf, cached_surf );
    //m_surfs.push_back( cached_surf );
}

void Q3Map::CacheBatches() {
    surf_t surf;
    cached_surf_t cached_surf;

    m_surfs.clear();

    for ( std::map<qshader_t *, BatchElement>::iterator
            it = m_batch.begin(); it != m_batch.end(); ++it )
    {
        /* for now continue */
        if ( it->first ) {
            continue;
        }

        surf.m_matName = "images/checkerboard.tga";
        //surf.m_matName = "q3shaders/base_wall.q3a";
        surf.m_indices.swap( it->second.m_indices );
        surf.m_verts.swap( it->second.m_verts );

        gl_render.CacheSurface( surf, cached_surf );
        m_surfs.push_back( cached_surf );
    }
}

void Q3Map::RenderSurface( msurface_t *surf ) {
    surfaceType_t surfaceType;

    if ( !surf->data ) {
        m_numUndefined++;
        return;
    }

    surfaceType = *((surfaceType_t *)surf->data);
    switch ( surfaceType ) {
    case SF_TRIANGLES:
        RenderTriangleSoup( surf );
        m_numTrisurfs++;
        break;

    case SF_FACE:
        RenderFace( surf );
        m_numFaces++;
        break;

    case SF_GRID:
        m_numMeshes++;
        break;

    case SF_FLARE:
        m_numFlares++;
        break;

    default:
        break;
    };
}

void Q3Map::R_LoadSurfaces( lump_t *surfs, lump_t *verts, lump_t *indexLump ) {
	dsurface_t	*in;
	msurface_t	*out;
	q3drawVert_t	*dv;
	int			*indexes;
	int			count;
	int			numFaces, numMeshes, numTriSurfs, numFlares;
	int			i;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;

	in = (dsurface_t *)(m_fileBase + surfs->fileofs);
	if (surfs->filelen % sizeof(*in)) {
		msg_warning0( "LoadMap: funny lump size in\n" );
    }
	count = surfs->filelen / sizeof(*in);

	dv = (q3drawVert_t *)(m_fileBase + verts->fileofs);
	if (verts->filelen % sizeof(*dv)) {
		msg_warning0 ("LoadMap: funny lump size in\n");
    }

	indexes = (int *)(m_fileBase + indexLump->fileofs);
	if ( indexLump->filelen % sizeof(*indexes)) {
		msg_warning0( "LoadMap: funny lump size in\n" );
    }

	out = (msurface_t *)malloc ( count * sizeof(*out) );

	m_world.surfaces = out;
	m_world.numsurfaces = count;

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
        out->viewCount = -1;
        out->data = NULL;

		switch ( LittleLong( in->surfaceType ) ) {
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv, out, indexes );
			numTriSurfs++;
			break;

		case MST_PATCH:
            out->data = new surfaceType_t( SF_GRID );
			numMeshes++;
            break;

		case MST_PLANAR:
			ParseFace( in, dv, out, indexes );
			numFaces++;
			break;

		case MST_FLARE:
            out->data = new surfaceType_t( SF_FLARE );
			numFlares++;
			break;

		default:
            assert(0);
            break;
		};
	}

    /*
#ifdef PATCH_STITCHING
	R_StitchAllPatches();
#endif

	R_FixSharedVertexLodError();

#ifdef PATCH_STITCHING
	R_MovePatchSurfacesToHunk();
#endif
    */

	fprintf( stderr, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n", 
		numFaces, numMeshes, numTriSurfs, numFlares );
}

void Q3Map::R_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	int		*in;
	msurface_t **out;
	
	in = (int *)(m_fileBase + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		msg_warning0( "LoadMap: funny lump size in" );
    }
	count = l->filelen / sizeof(*in);
	out = (msurface_t **)malloc ( count*sizeof(*out) );

	m_world.marksurfaces = out;
	m_world.nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong(in[i]);
		out[i] = m_world.surfaces + j;
	}
}

void Q3Map::R_LoadShaders( lump_t *l ) {
	int		i, count;
	dshader_t	*in, *out;
	
	in = (dshader_t *)(m_fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		msg_warning0( "LoadMap: funny lump size in \n" );
	count = l->filelen / sizeof(*in);
	out = (dshader_t *)malloc( count*sizeof(*out) );

	m_world.shaders = out;
	m_world.numShaders = count;

	memcpy( out, in, count*sizeof(*out) );

	for ( i=0 ; i<count ; i++ ) {
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}

void Q3Map::R_LoadPlanes( lump_t *l ) {
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (dplane_t *)(m_fileBase + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		msg_warning0( "LoadMap: funny lump size in " );
        return;
    }

	count = l->filelen / sizeof(*in);
	out = (cplane_t *)malloc ( count*2*sizeof(*out) );
	
	m_world.planes = out;
	m_world.numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++) {
		bits = 0;
		for (j=0 ; j<3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1<<j;
			}
		}

		out->dist = LittleFloat (in->dist);

        FromQ3Plane( *out );

        /*
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
        */
	}
}

void Q3Map::R_LoadVisibility( lump_t *l ) {
	int		len;
	byte	*buf;

    len = l->filelen;
	if ( !len ) {
		return;
	}
	buf = m_fileBase + l->fileofs;

	m_world.numClusters = LittleLong( ((int *)buf)[0] );
    m_world.clusterBytes = LittleLong( ((int *)buf)[1] );

    len = m_world.numClusters * m_world.clusterBytes;

	byte	*dest;
	dest = (byte *)malloc( len );
	memcpy( dest, buf + 8, len - 8 );
	m_world.vis = dest;

    m_world.novis = (byte*)malloc( m_world.clusterBytes );
    memset( m_world.novis, 0xff, m_world.clusterBytes );
}

void Q3Map::R_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	R_SetParent (node->children[0], node);
	R_SetParent (node->children[1], node);
}


void Q3Map::R_LoadNodesAndLeafs(lump_t *nodeLump, lump_t *leafLump) {
	int			i, j, p;
	dnode_t		*in;
	dleaf_t		*inLeaf;
	mnode_t 	*out;
	int			numNodes, numLeafs;

	in = (dnode_t *)(m_fileBase + nodeLump->fileofs);
	if (nodeLump->filelen % sizeof(dnode_t) ||
		leafLump->filelen % sizeof(dleaf_t) ) {
		msg_warning0("LoadMap: funny lump size in");
	}
	numNodes = nodeLump->filelen / sizeof(dnode_t);
	numLeafs = leafLump->filelen / sizeof(dleaf_t);

	out = (mnode_t*)malloc ( (numNodes + numLeafs) * sizeof(*out) );

	m_world.nodes = out;
	m_world.numnodes = numNodes + numLeafs;
	m_world.numDecisionNodes = numNodes;

	// load nodes
	for ( i=0 ; i<numNodes; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (in->mins[j]);
			out->maxs[j] = LittleLong (in->maxs[j]);
		}

        FromQ3( out->mins.ToFloatPtr(), out->mins.ToFloatPtr() );
        FromQ3( out->maxs.ToFloatPtr(), out->maxs.ToFloatPtr() );
	
		p = LittleLong(in->planeNum);
		out->plane = m_world.planes + p;
        out->cluster = -1;
		out->contents = -1;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = m_world.nodes + p;
			else
				out->children[j] = m_world.nodes + numNodes + (-1 - p);
		}
	}
	
	// load leafs
	inLeaf = (dleaf_t *)(m_fileBase + leafLump->fileofs);
	for ( i=0 ; i<numLeafs ; i++, inLeaf++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (inLeaf->mins[j]);
			out->maxs[j] = LittleLong (inLeaf->maxs[j]);
		}

        FromQ3( out->mins.ToFloatPtr(), out->mins.ToFloatPtr() );
        FromQ3( out->maxs.ToFloatPtr(), out->maxs.ToFloatPtr() );

		out->cluster = LittleLong(inLeaf->cluster);
		out->area = LittleLong(inLeaf->area);
        out->contents = 0;

		if ( out->cluster >= m_world.numClusters ) {
			m_world.numClusters = out->cluster + 1;
		}

		out->firstmarksurface = m_world.marksurfaces +
			LittleLong(inLeaf->firstLeafSurface);
		out->nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);
	}	

	// chain decendants
	R_SetParent (m_world.nodes, NULL);
}

#define SCALE_FACTOR 1.0f

void Q3Map::FromQ3( float in[3], float out[3] ) const {
    float tmp[3];

    tmp[0] = in[0]; tmp[1] = in[1]; tmp[2] = in[2];

    out[0] = tmp[0] * SCALE_FACTOR;
    out[1] = -tmp[1] * SCALE_FACTOR;
    out[2] = tmp[2] * SCALE_FACTOR;
}

void Q3Map::FromQ3Plane( cplane_t &p ) const {
    cplane_t res;

    res.normal = p.normal;

    idVec3 orig = p.dist * p.normal;

    FromQ3( res.normal.ToFloatPtr(), res.normal.ToFloatPtr() );
    FromQ3( orig.ToFloatPtr(), orig.ToFloatPtr() );

    res.normal.Normalize();
    res.dist = orig * res.normal;

    p = res;
}

void Q3Map::Think( int ms ) {
}

void Q3Map::Render( void ) {
    m_numFaces = m_numMeshes = m_numTrisurfs = m_numFlares = m_numUndefined = 0;
    idVec3 pos( gl_camera.GetPlayerView().m_pos );

    UncacheSurfaces();
    MarkSurfaces( pos );
    R_RecursiveWorldNode( m_world.nodes );
    CacheBatches();

	fprintf( stderr, "q3map: (%d,%d,%d) added %d faces, "
            "%i meshes, %i trisurfs, %i flares %i undefined\n", 
        (int)pos[0], (int)pos[1], (int)pos[2], m_numFaces, 
        m_numMeshes, m_numTrisurfs, m_numFlares, m_numUndefined );

    for ( int i = 0; i < m_surfs.size(); ++i ) {
        glsurf_t gl;
        gl.m_modelMatrix = idMat4( m_axis, m_pos );
        gl.m_surf = m_surfs[i];

        gl_render.AddSurface( gl );
    }
}
