
#include "Q3Map.h"
#include "Utils.h"
#include "Camera.h"
#include "Render.h"
#include "Q3Material.h"
#include <fstream>

#include "q2_map_common.h"

#define qtrue true
#define qfalse false
#define qboolean bool

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

    surf->shader = LittleLong( ds->shaderNum );

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

    surf->shader = LittleLong( ds->shaderNum );

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

static void Transpose( int width, int height, q3drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j;
	q3drawVert_t	temp;

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

static void LerpDrawVert( q3drawVert_t *a, q3drawVert_t *b, q3drawVert_t *out ) {
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

static void PutPointsOnCurve( q3drawVert_t	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], 
							 int width, int height ) {
	int			i, j;
	q3drawVert_t	prev, next;

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

static void MakeMeshNormals( int width, int height, 
        q3drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) 
{
	int		i, j, k, dist;
	vec3_t	normal;
	vec3_t	sum;
	int		count;
	vec3_t	base;
	vec3_t	delta;
	int		x, y;
	q3drawVert_t	*dv;
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
			if ( count == 0 ) {
				count = 1;
			}
			VectorNormalize2( sum, dv->normal );
		}
	}
}

srfGridMesh_t *Q3Map::R_CreateSurfaceGridMesh(int width, int height,
		q3drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], float errorTable[2][MAX_GRID_SIZE] ) 
{
	int i, j, size;
	q3drawVert_t	*vert;
	idVec3		tmpVec;
	srfGridMesh_t *grid;

	// copy the results out to a grid
	size = (width * height - 1) * sizeof( q3drawVert_t ) + sizeof( *grid );

	grid = (srfGridMesh_t *)malloc( size );
	memset(grid, 0, size);

	grid->widthLodError = (float*)malloc( width * 4 );
	memcpy( grid->widthLodError, errorTable[0], width * 4 );

	grid->heightLodError = (float*)malloc( height * 4 );
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
    grid->localOrigin = grid->meshBounds[0] + grid->meshBounds[1];
	grid->localOrigin *= 0.5f;
    tmpVec = grid->meshBounds[0] - grid->localOrigin;
	grid->meshRadius = tmpVec.Length();

    grid->lodOrigin = grid->localOrigin;
	grid->lodRadius = grid->meshRadius;

	return grid;
}

srfGridMesh_t *Q3Map::R_SubdividePatchToGrid( int width, int height, q3drawVert_t *points ) {
	int			i, j, k, l;
	q3drawVert_t	prev, next, mid;
	float		len, maxLen;
	int			dir;
	int			t;
	q3drawVert_t	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE];
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
			// check subdivided midpoints against control points

			// FIXME: also check midpoints of adjacent patches against the control points
			// this would basically stitch all patches in the same LOD group together.

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

				// see how far off the line it is
				// using dist-from-line will not account for internal
				// texture warping, but it gives a lot less polygons than
				// dist-from-midpoint
				VectorSubtract( midxyz, ctrl[i][j].xyz, midxyz );
				VectorSubtract( ctrl[i][j+2].xyz, ctrl[i][j].xyz, dir );
				VectorNormalize( dir );

				d = DotProduct( midxyz, dir );
				VectorScale( dir, d, projected );
				VectorSubtract( midxyz, projected, midxyz2);
				len = VectorLengthSquared( midxyz2 );			// we will do the sqrt later
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

			if ( maxLen <= 4 ) {
				errorTable[dir][j+1] = 1.0f/maxLen;
				continue;	// didn't need subdivision
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


void Q3Map::ParseGrid( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf ) {
	srfGridMesh_t	*grid;
	int				i, j;
	int				width, height, numPoints;
	q3drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	int				lightmapNum;
	idVec3			bounds[2];
	idVec3			tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	//lightmapNum = LittleLong( ds->lightmapNum );

	// get fog volume
	//surf->fogIndex = LittleLong( ds->fogNum ) + 1;

    surf->shader = ds->shaderNum;

	// get shader value
	//surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );
	//if ( r_singleShader->integer && !surf->shader->isSky ) {
		//surf->shader = tr.defaultShader;
	//}

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( m_world.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & 0x80 ) {
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
		//R_ColorShiftLightingBytes( verts[i].color, points[i].color );
	}

    FromQ3( points[i].xyz, points[i].xyz );
    FromQ3( points[i].normal, points[i].normal );

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
    bounds[1] += bounds[0];
    grid->lodOrigin = bounds[1] * 0.5f;
    tmpVec = bounds[0] - grid->lodOrigin;
	grid->lodRadius = tmpVec.Length();

    FromQ3( grid->lodOrigin.ToFloatPtr(), grid->lodOrigin.ToFloatPtr() );
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

    //surf.m_matName = "white";
    //surf.m_indices.resize( tri->numIndexes );
    //memcpy( surf.m_indices.data(), tri->indexes, tri->numIndexes * sizeof(int) );

    BatchElement &b = m_batch[ s->shader ];
    int bob = b.m_verts.size();

    for ( int i = 0; i < tri->numIndexes; ++i ) {
        surf.m_indices.push_back( bob + tri->indexes[i] );
    }

    idBounds bounds;
    for ( int i = 0; i < tri->numVerts; ++i ) {
        q3drawVert_t *qv = &tri->verts[ i ];

        drawVert_t v;

        v.m_pos[0] = qv->xyz[0];
        v.m_pos[1] = qv->xyz[1];
        v.m_pos[2] = qv->xyz[2];

        bounds.AddPoint( idVec3( v.m_pos[0], v.m_pos[1], v.m_pos[2] ) );

        v.m_tex[0] = qv->st[0];
        v.m_tex[1] = qv->st[1];

        v.m_normal[0] = qv->normal[0];
        v.m_normal[1] = qv->normal[1];
        v.m_normal[2] = qv->normal[2];

        surf.m_verts.push_back( v );
    }

    //gl_render.CacheSurface( surf, cached_surf );
    //m_surfs.push_back( cached_surf );

    if ( !CullSurfaceAgainstFrustum( bounds ) ) {
        b.m_indices.insert( b.m_indices.end(), surf.m_indices.begin(), surf.m_indices.end() );
        b.m_verts.insert( b.m_verts.end(), surf.m_verts.begin(), surf.m_verts.end() );
    }
}

void Q3Map::RenderFace( msurface_t *s ) {
    surf_t surf;
    //cached_surf_t cached_surf;
    unsigned *indices;
    float *point;
    srfSurfaceFace_t *face = (srfSurfaceFace_t *)s->data;

    //fprintf( stderr, "RenderTriangleSoup(%d,%d)\n", tri->numIndexes, tri->numVerts );
    //surf.m_matName = "white";

    indices = (unsigned *)( ( ( char * ) face ) + face->ofsIndices );

    BatchElement &b = m_batch[ s->shader ];
    int bob = b.m_verts.size();

    for ( int i = 0; i < face->numIndices; ++i ) {
        surf.m_indices.push_back( bob + indices[i] );
    }

    point = face->points[0];

    idBounds bounds;
    for ( int i = 0; i < face->numPoints; ++i, point += 8 ) {

        drawVert_t v;
        v.m_pos[0] = point[0];
        v.m_pos[1] = point[1];
        v.m_pos[2] = point[2];

        bounds.AddPoint( idVec3( v.m_pos[0], v.m_pos[1], v.m_pos[2] ) );

        v.m_tex[0] = point[3];
        v.m_tex[1] = point[4];

        v.m_normal[0] = face->plane.normal[0];
        v.m_normal[1] = face->plane.normal[1];
        v.m_normal[2] = face->plane.normal[2];

        surf.m_verts.push_back( v );
    }

    if ( !CullSurfaceAgainstFrustum( bounds ) ) {
        b.m_indices.insert( b.m_indices.end(), surf.m_indices.begin(), surf.m_indices.end() );
        b.m_verts.insert( b.m_verts.end(), surf.m_verts.begin(), surf.m_verts.end() );
    }

    //gl_render.CacheSurface( surf, cached_surf );
    //m_surfs.push_back( cached_surf );
}

void Q3Map::RB_SubmitBatch( int shader ) {
    surf_t surf;
    cached_surf_t cached_surf;
    BatchElement &b = m_batch[ shader ];

    const char *p = (const char *)m_world.shaders[ shader ].shader;

    /*
    if ( m_world.shaders[ shader ].surfaceFlags & ( 0x80 | 0x200 | 0x4 ) ) {
        b.m_indices.clear();
        b.m_verts.clear();
        return;
    }
    */

    if ( b.m_indices.empty() ) {
        return;
    }

    std::string pp = std::string( p );

    if ( glMaterialCache.Get( pp + ".l3a" ) != NULL ) {
        //fprintf( stderr, "checkerboard=%s\n", surf.m_matName.c_str() );
        surf.m_matName = pp + ".l3a";

    } else if ( glMaterialCache.Get( pp + ".tga" ) != NULL ) {
        surf.m_matName = pp + ".tga";

    } else {
        surf.m_matName = "images/checkerboard.tga";
    }

    if ( glMaterialCache.Get( surf.m_matName ) != NULL ) {
        surf.m_indices.swap( b.m_indices );
        surf.m_verts.swap( b.m_verts );

        gl_render.CacheSurface( surf, cached_surf );
        m_surfs.push_back( cached_surf );
    }

    b.m_indices.clear();
    b.m_verts.clear();
}

void Q3Map::CacheBatches() {
    surf_t surf;
    cached_surf_t cached_surf;

    for ( std::map<int, BatchElement>::iterator
            it = m_batch.begin(); it != m_batch.end(); ++it )
    {
        RB_SubmitBatch( it->first );
    }

    m_batch.clear();
}

static float	LodErrorForVolume( vec3_t local, float radius ) {
    return 0;
}

void Q3Map::RenderGrid( msurface_t *surf ) {
	int		i, j;
	float	*xyz;
	float	*texCoords;
	float	*normal;
	unsigned char *color;
	q3drawVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes = 0;
	int		dlightBits;
	int		*vDlightBits;
	qboolean	needsNormal;
    srfGridMesh_t *cv = (srfGridMesh_t *)surf->data;
    const int SHADER_MAX_VERTEXES = 1000;
    const int SHADER_MAX_INDEXES = 6*1000;

    BatchElement &batch = m_batch[ surf->shader ];

	// determine the allowable discrepance
	//lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );
    ////LodErrorForVolume( cv->lodOrigin, cv->lodRadius );
	lodError = 20; 

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;


	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - (int)batch.m_verts.size() ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - (int)batch.m_indices.size() ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				RB_SubmitBatch( surf->shader );
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

        numVertexes = batch.m_verts.size();

        drawVert_t vert;
		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				vert.m_pos[0] = dv->xyz[0];
				vert.m_pos[1] = dv->xyz[1];
				vert.m_pos[2] = dv->xyz[2];
                vert.m_tex[0] = dv->st[0];
                vert.m_tex[1] = dv->st[1];
                vert.m_normal[0] = dv->normal[0];
                vert.m_normal[1] = dv->normal[1];
                vert.m_normal[2] = dv->normal[2];

                batch.m_verts.push_back( vert );
			}
		}


		// add the indexes
		{
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

                    batch.m_indices.push_back( v2 );
                    batch.m_indices.push_back( v3 );
                    batch.m_indices.push_back( v1 );

                    batch.m_indices.push_back( v1 );
                    batch.m_indices.push_back( v3 );
                    batch.m_indices.push_back( v4 );
				}
			}
		}

		used += rows - 1;
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
        RenderGrid( surf );
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
            ParseGrid( in, dv, out );
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


/* made next procedures dumb for now */

#define SCALE_FACTOR 1.0f

void Q3Map::FromQ3( float in[3], float out[3] ) const {
    out[0] = in[0]; out[1] = in[1]; out[2] = in[2];

    /*
    float tmp[3];

    tmp[0] = in[0]; tmp[1] = in[1]; tmp[2] = in[2];

    out[0] = tmp[0] * SCALE_FACTOR;
    out[1] = -tmp[1] * SCALE_FACTOR;
    out[2] = tmp[2] * SCALE_FACTOR;
    */
}

void Q3Map::FromQ3Plane( cplane_t &p ) const {
    /*
    cplane_t res;

    res.normal = p.normal;

    idVec3 orig = p.dist * p.normal;

    FromQ3( res.normal.ToFloatPtr(), res.normal.ToFloatPtr() );
    FromQ3( orig.ToFloatPtr(), orig.ToFloatPtr() );

    res.normal.Normalize();
    res.dist = orig * res.normal;

    p = res;
    */
}

void Q3Map::ToQ3( float in[3], float out[3] ) const {
    out[0] = in[0]; out[1] = in[1]; out[2] = in[2];
}

void Q3Map::ToQ3Plane( cplane_t &p ) const {
}

void Q3Map::Think( int ms ) {
}

void Q3Map::Render( void ) {
    m_numFaces = m_numMeshes = m_numTrisurfs = m_numFlares = m_numUndefined = 0;
    const playerView_t &view = gl_camera.GetPlayerView();
    idVec3 pos( view.m_pos );

    ConstructFrustum( view );

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

void Q3Map::ConstructFrustum( const playerView_t &view ) {
    m_frustumPlanes[0].SetNormal( view.m_axis[0] );
    m_frustumPlanes[0].FitThroughPoint( view.m_pos );
    m_frustumPlanes[0].SetDist( m_frustumPlanes[0].Dist() + view.m_znear );

    m_frustumPlanes[1].SetNormal( view.m_axis[0] );
    m_frustumPlanes[1].FitThroughPoint( view.m_pos );
    m_frustumPlanes[1].SetDist( m_frustumPlanes[1].Dist() + view.m_zfar );
    m_frustumPlanes[1].SetNormal( -view.m_axis[0] );

    m_frustumPlanes[2].SetNormal( view.m_axis[0] 
            * idQuat( view.m_axis[2], DEG2RAD( 90 - view.m_fovx / 2 ) ) );
    m_frustumPlanes[2].FitThroughPoint( view.m_pos );

    m_frustumPlanes[3].SetNormal( view.m_axis[0] 
            * idQuat( view.m_axis[2], -DEG2RAD( 90 - view.m_fovx / 2 ) ) );
    m_frustumPlanes[3].FitThroughPoint( view.m_pos );

    m_frustumPlanes[4].SetNormal( view.m_axis[0] 
            * idQuat( view.m_axis[1], DEG2RAD( 90 - view.m_fovy / 2 ) ) );
    m_frustumPlanes[4].FitThroughPoint( view.m_pos );

    m_frustumPlanes[5].SetNormal( view.m_axis[0] 
            * idQuat( view.m_axis[1], -DEG2RAD( 90 - view.m_fovy / 2 ) ) );
    m_frustumPlanes[5].FitThroughPoint( view.m_pos );
}

bool Q3Map::CullSurfaceAgainstFrustum( const idBounds &bounds ) const {
    idVec3 p[8];
    bounds.ToPoints( p );

    for ( int i = 0; i < 6; ++i ) {
        const idPlane &plane = m_frustumPlanes[i];

        for ( int j = 0; j < 8; ++j ) {
            if ( plane.Side( p[j] ) != PLANESIDE_BACK ) {
                return false;
            }
        }
    }

    return true;
}

void Q3Map::Q2StoreVisibility( LumpsType &lumps ) {
    std::string &s = lumps[Q2_LUMP_VISIBILITY];

    s.resize(m_world.numClusters * m_world.clusterBytes
        + sizeof(int) + 2 * sizeof(int) * m_world.numClusters);

    q2_dvis_t *vis = (q2_dvis_t *)s.data();
    vis->numclusters = m_world.numClusters;

    byte *origout, *out;
    origout = out = (byte *)&s[ sizeof(int) + 2 * sizeof(int) * m_world.numClusters ];
    for ( int i = 0; i < m_world.numClusters; ++i ) {
        byte *in = (byte*)m_world.vis + i * m_world.clusterBytes;

        vis->bitofs[i][Q2_DVIS_PVS] = out - origout;
        vis->bitofs[i][Q2_DVIS_PHS] = 0;

        byte *in_end = in + m_world.clusterBytes;

        /* compression */
        while ( in < in_end ) {
            if ( *in ) {
                *out++ = *in++;
            } else {
                byte *tin = in;
                while ( !*tin && ( tin - in ) < 255 ) {
                    ++tin;
                }

                *out++ = 0;
                *out++ = tin - in;

                in = tin;
            }
        }
    }

    s.resize( ( out - origout ) + sizeof(int) + 2 * sizeof(int) * m_world.numClusters );
}

void Q3Map::Q3ToQ2Texture( int shader, char *texname ) {
    const char *p = (const char *)m_world.shaders[ shader ].shader;

    std::string pp = std::string( p );
    std::string imgName;

    if ( glMaterialCache.Get( pp + ".l3a" ) != NULL ) {
        Q3LightMaterial *m = static_cast<Q3LightMaterial*>( 
                 glMaterialCache.Get( pp + ".l3a" ).get() );
        imgName = m->GetImageName();

    } else if ( glMaterialCache.Get( pp + ".tga" ) != NULL ) {
        imgName = pp + ".tga";

    } else {
        imgName = "images/checkerboard.tga";
    }

    std::string origImgName = imgName;

    /* compressing */
    if ( imgName.rfind( '/' ) != -1 ) {
        imgName = "t/" + imgName.substr( imgName.rfind( '/' ) );
    }

    fprintf( stderr, "`%s' -> `%s'\n", origImgName.c_str(), imgName.c_str() );

    if ( imgName.length() > 32 ) {
        msg_failure( "`%s' is longer than 32 symbols\n", imgName.c_str() );
    }

    memcpy( texname, imgName.c_str(), imgName.size() );
}

/* return offset in bytes */
template <typename T>
static int PushItem(std::string &m, const T &v) { 
    int sz = m.size();
    m.resize(sz + sizeof(T));
    T *vt = (T*)&m[sz];
    *vt = v;
    return sz;
}

/* == 0 - not found, offset in bytes in edges array */
static int FindEdgeInMap( std::map<unsigned int, int> &m, 
        unsigned int v1, unsigned int v2 ) 
{
    unsigned int ind = v1 << 16 + v2;
    if ( m.find( ind ) != m.end() ) {
        assert( m[ind] > 0 );
        return m[ind];
    }

    ind = v2 << 16 + v1;
    if ( m.find( ind ) != m.end() ) {
        assert( m[ind] > 0 );
        return -m[ind];
    }

    return 0;
}

void Q3Map::PushEdge( LumpsType &lumps,
        std::map<unsigned int, int> &m, unsigned int v1, unsigned int v2 )
{
    int offset = FindEdgeInMap( m, v1, v2 );

    if ( !offset ) {
        q2_dedge_t edge;
        edge.v[0] = v1;
        edge.v[1] = v2;
        offset = PushItem( lumps[Q2_LUMP_EDGES], edge );
        m.insert( std::make_pair( v1 << 16 + v2, offset ) );
    }

    assert( offset != 0 );
    PushItem( lumps[Q2_LUMP_SURFEDGES], offset / sizeof(q2_dedge_t) );
}

void Q3Map::Q2StoreSurfaces( LumpsType &lumps ) {
    surfaceMap.clear();

    /* fake edge */
    PushItem( lumps[Q2_LUMP_EDGES], q2_dedge_t() );

    for ( int i = 0; i < m_world.numsurfaces; ++i ) {
        msurface_t *m = m_world.surfaces + i;
        surfaceType_t *type = (surfaceType_t *)m->data;

        if ( *type != SF_FACE ) {
            continue;
        }

        // key (idx1<<16 + idx2, value - edge-offset-int)
        std::map<unsigned int, int> edgesMap; 

        srfSurfaceFace_t *face = (srfSurfaceFace_t *)m->data;

        q2_dvertex_t q2vertex;

        int voffset = lumps[Q2_LUMP_VERTEXES].size() / sizeof(q2_dvertex_t);
        int eoffset = lumps[Q2_LUMP_EDGES].size() / sizeof(q2_dedge_t);

        float *point = face->points[0];
        for ( int j = 0; j < face->numPoints; ++j, point += 8 ) {
            VectorCopy( point, q2vertex.point );
            PushItem( lumps[Q2_LUMP_VERTEXES], q2vertex );
        }

        /* generating edges */
        unsigned int *indices = (unsigned int *)( ( ( char * ) face ) + face->ofsIndices );

        for ( int j = 0; j < face->numIndices; j += 3 ) {
            assert( voffset+indices[j+0] < 65536 );
            assert( voffset+indices[j+1] < 65536 );
            assert( voffset+indices[j+2] < 65536 );

            PushEdge( lumps, edgesMap, voffset+indices[j+0], 
                    voffset+indices[j+1] );
            PushEdge( lumps, edgesMap, voffset+indices[j+1], 
                    voffset+indices[j+2] );
            PushEdge( lumps, edgesMap, voffset+indices[j+2], 
                    voffset+indices[j+0] );
        }

        int neweoffset = lumps[Q2_LUMP_EDGES].size() / sizeof(q2_dedge_t);

        q2_dface_t q2face = {0};

        q2_dplane_t plane;
        VectorCopy( face->plane.normal, plane.normal );
        plane.dist = face->plane.dist;
        plane.type = 3;
        
        q2face.planenum = PushItem( lumps[Q2_LUMP_PLANES], plane ) / sizeof(plane);
        q2face.side = 0;
        q2face.firstedge = eoffset;
        q2face.numedges = neweoffset - eoffset;

        {
            /* texinfo */
            assert( face->numIndices >= 3 );

            point = face->points[0];

            vec3_t d1, d2, sdir, tdir, ndir;
            vec3_t s0, s1, s2, ds, dt;

            VectorSubtract( &point[8*indices[1]], &point[8*indices[0]], d1 );
            VectorSubtract( &point[8*indices[2]], &point[8*indices[0]], d2 );
            VectorCopy( &point[8*indices[0]+5], ndir );
            VectorCopy( &point[8*indices[0]+3], s0 );
            VectorCopy( &point[8*indices[1]+3], s1 );
            VectorCopy( &point[8*indices[2]+3], s2 );
            VectorSubtract( s1, s0, ds );
            VectorSubtract( s2, s0, dt );

            float detSt = ( dt[1] * ds[0] - ds[1] * dt[0] );
            assert( detSt != 0 );

            sdir[0] = ( dt[1] * d1[0] - ds[1] * d2[0] ) / detSt;
            sdir[1] = ( dt[1] * d1[1] - ds[1] * d2[1] ) / detSt;
            sdir[2] = ( dt[1] * d1[2] - ds[1] * d2[2] ) / detSt;

            tdir[0] = ( -dt[0] * d1[0] + ds[0] * d2[0] ) / detSt;
            tdir[1] = ( -dt[0] * d1[1] + ds[0] * d2[2] ) / detSt;
            tdir[2] = ( -dt[0] * d1[2] + ds[0] * d2[2] ) / detSt;

            VectorNormalize( sdir );
            VectorNormalize( tdir );

            /* orthonormalize ndir and sdir */
            float pd = DotProduct( sdir, ndir );
            VectorMA( sdir, -pd, ndir, sdir );
            pd = DotProduct( tdir, ndir );
            VectorMA( tdir, -pd, ndir, tdir );
            pd = DotProduct( tdir, sdir );
            VectorMA( tdir, -pd, sdir, tdir );

            q2_texinfo_t tex;
            VectorCopy( sdir, tex.vecs[0] );
            VectorCopy( tdir, tex.vecs[1] );

            tex.vecs[0][3] = tex.vecs[1][3] = 0;
            tex.flags = 0;
            tex.value = 0;
            Q3ToQ2Texture( m->shader, tex.texture );
            tex.nexttexinfo = -1;

            q2face.texinfo = PushItem( 
                    lumps[Q2_LUMP_TEXINFO], tex ) / sizeof(tex);
        }

        surfaceMap.insert( std::make_pair( m,
            PushItem( lumps[Q2_LUMP_FACES], q2face ) / sizeof(q2_dface_t) ) );
    }
}

void Q3Map::Q2StoreNodesAndLeafs( LumpsType &lumps ) {
    std::string &nodesLump = lumps[ Q2_LUMP_NODES ];
    std::string &leafsLump = lumps[ Q2_LUMP_LEAFS ];
    std::string &markLump = lumps[ Q2_LUMP_LEAFFACES ];

    /* leaves first */
    for ( int i = 0; i < m_world.numnodes; ++i ) {
        mnode_t *n = &m_world.nodes[i];

        if ( n->contents == -1 ) { 
            continue;
        }

        q2_dleaf_t leaf;
        leaf.contents = 0;
        leaf.cluster = n->cluster;
        leaf.area = n->area;

        leaf.mins[0] = n->mins[0];
        leaf.mins[1] = n->mins[1];
        leaf.mins[2] = n->mins[2];

        leaf.maxs[0] = n->maxs[0];
        leaf.maxs[1] = n->maxs[1];
        leaf.maxs[2] = n->maxs[2];

        leaf.firstleafface = markLump.size() / sizeof(short);
        leaf.numleaffaces = 0;

        int c;
        msurface_t *surf, **mark;

        mark = n->firstmarksurface;
        c = n->nummarksurfaces;


        while (c--) {
            surf = *mark;

            if ( surfaceMap.find( surf ) == surfaceMap.end() ) {
                continue;
            }

            PushItem( lumps[ Q2_LUMP_LEAFFACES ], (short)surfaceMap[surf] );

            leaf.numleaffaces++;
            mark++;
        }

        PushItem( lumps[ Q2_LUMP_LEAFS ], leaf );
    }

    /* nodes are next */
    for ( int i = 0; i < m_world.numnodes; ++i ) {
        mnode_t *n = &m_world.nodes[i];

        if ( n->contents != -1 ) { 
            continue;
        }


        q2_dnode_t dnode;

        q2_dplane_t plane;
        VectorCopy( n->plane->normal, plane.normal );
        plane.dist = n->plane->dist;
        plane.type = 3;

        dnode.planenum = PushItem( lumps[Q2_LUMP_PLANES], plane )
            / sizeof(q2_dplane_t);

        if ( n->children[0] >= &m_world.nodes[m_world.numDecisionNodes]) {
            dnode.children[0] = -((n->children[0] - &m_world.nodes[m_world.numDecisionNodes]) + 1);
        } else {
            dnode.children[0] = n->children[0] - m_world.nodes;
        }

        if ( n->children[1] >= &m_world.nodes[m_world.numDecisionNodes]) {
            dnode.children[1] = -((n->children[1] - &m_world.nodes[m_world.numDecisionNodes]) + 1);
        } else {
            dnode.children[1] = n->children[1] - m_world.nodes;
        }

        dnode.mins[0] = n->mins[0];
        dnode.mins[1] = n->mins[1];
        dnode.mins[2] = n->mins[2];

        dnode.maxs[0] = n->maxs[0];
        dnode.maxs[1] = n->maxs[1];
        dnode.maxs[2] = n->maxs[2];

        dnode.firstface = 0; /* ??? is correct */
        dnode.numfaces = 0;

        PushItem( lumps[ Q2_LUMP_NODES ], dnode );
    }
}

void Q3Map::ConvertToQ2( const std::string &q2file ) {
    LumpsType lumps;
    q2_dheader_t header = {0};
    header.ident = Q2_IDBSPHEADER;
    header.version = Q2_BSPVERSION;

    lumps.resize(Q2_HEADER_LUMPS);

    Q2StoreVisibility(lumps);
    Q2StoreSurfaces(lumps);
    Q2StoreNodesAndLeafs(lumps);

    std::fstream ofs( q2file.c_str(), 
            std::ios_base::out | std::ios_base::binary );

    for ( int i = 0, offs = sizeof(header); i < Q2_HEADER_LUMPS; 
            offs += lumps[i].size(), ++i ) 
    {
        header.lumps[i].fileofs = offs;
        header.lumps[i].filelen = lumps[i].size();
    }

    ofs.write( (const char *)&header, sizeof( header ));

    for ( int i = 0; i < Q2_HEADER_LUMPS; ++i ) {
        if ( !lumps[i].empty() ) {
            ofs.write( lumps[i].data(), lumps[i].size() );
        }
    }
}

byte *Q3Map::AAS_LoadAASLump(int offset, int length, 
        int *lastoffset, int size)
{
	byte *buf;
	if (!length) {
        return (byte*)malloc(size+1);
	}
	buf = (byte *) malloc(length+1);
	if (length) {
        memcpy( buf, m_fileBase + offset, length );
		*lastoffset += length;
	}
	return buf;
}

static void AAS_DData(unsigned char *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
	{
		data[i] ^= (unsigned char) i * 119;
	} //end for
}

static void AAS_SwapAASData(aas_t &aasworld)
{
	int i, j;
	//bounding boxes
	for (i = 0; i < aasworld.numbboxes; i++)
	{
		aasworld.bboxes[i].presencetype = LittleLong(aasworld.bboxes[i].presencetype);
		aasworld.bboxes[i].flags = LittleLong(aasworld.bboxes[i].flags);
		for (j = 0; j < 3; j++)
		{
			aasworld.bboxes[i].mins[j] = LittleLong(aasworld.bboxes[i].mins[j]);
			aasworld.bboxes[i].maxs[j] = LittleLong(aasworld.bboxes[i].maxs[j]);
		} //end for
	} //end for
	//vertexes
	for (i = 0; i < aasworld.numvertexes; i++)
	{
		for (j = 0; j < 3; j++)
			aasworld.vertexes[i][j] = LittleFloat(aasworld.vertexes[i][j]);
	} //end for
	//planes
	for (i = 0; i < aasworld.numplanes; i++)
	{
		for (j = 0; j < 3; j++)
			aasworld.planes[i].normal[j] = LittleFloat(aasworld.planes[i].normal[j]);
		aasworld.planes[i].dist = LittleFloat(aasworld.planes[i].dist);
		aasworld.planes[i].type = LittleLong(aasworld.planes[i].type);
	} //end for
	//edges
	for (i = 0; i < aasworld.numedges; i++)
	{
		aasworld.edges[i].v[0] = LittleLong(aasworld.edges[i].v[0]);
		aasworld.edges[i].v[1] = LittleLong(aasworld.edges[i].v[1]);
	} //end for
	//edgeindex
	for (i = 0; i < aasworld.edgeindexsize; i++)
	{
		aasworld.edgeindex[i] = LittleLong(aasworld.edgeindex[i]);
	} //end for
	//faces
	for (i = 0; i < aasworld.numfaces; i++)
	{
		aasworld.faces[i].planenum = LittleLong(aasworld.faces[i].planenum);
		aasworld.faces[i].faceflags = LittleLong(aasworld.faces[i].faceflags);
		aasworld.faces[i].numedges = LittleLong(aasworld.faces[i].numedges);
		aasworld.faces[i].firstedge = LittleLong(aasworld.faces[i].firstedge);
		aasworld.faces[i].frontarea = LittleLong(aasworld.faces[i].frontarea);
		aasworld.faces[i].backarea = LittleLong(aasworld.faces[i].backarea);
	} //end for
	//face index
	for (i = 0; i < aasworld.faceindexsize; i++)
	{
		aasworld.faceindex[i] = LittleLong(aasworld.faceindex[i]);
	} //end for
	//convex areas
	for (i = 0; i < aasworld.numareas; i++)
	{
		aasworld.areas[i].areanum = LittleLong(aasworld.areas[i].areanum);
		aasworld.areas[i].numfaces = LittleLong(aasworld.areas[i].numfaces);
		aasworld.areas[i].firstface = LittleLong(aasworld.areas[i].firstface);
		for (j = 0; j < 3; j++)
		{
			aasworld.areas[i].mins[j] = LittleFloat(aasworld.areas[i].mins[j]);
			aasworld.areas[i].maxs[j] = LittleFloat(aasworld.areas[i].maxs[j]);
			aasworld.areas[i].center[j] = LittleFloat(aasworld.areas[i].center[j]);
		} //end for
	} //end for
	//area settings
	for (i = 0; i < aasworld.numareasettings; i++)
	{
		aasworld.areasettings[i].contents = LittleLong(aasworld.areasettings[i].contents);
		aasworld.areasettings[i].areaflags = LittleLong(aasworld.areasettings[i].areaflags);
		aasworld.areasettings[i].presencetype = LittleLong(aasworld.areasettings[i].presencetype);
		aasworld.areasettings[i].cluster = LittleLong(aasworld.areasettings[i].cluster);
		aasworld.areasettings[i].clusterareanum = LittleLong(aasworld.areasettings[i].clusterareanum);
		aasworld.areasettings[i].numreachableareas = LittleLong(aasworld.areasettings[i].numreachableareas);
		aasworld.areasettings[i].firstreachablearea = LittleLong(aasworld.areasettings[i].firstreachablearea);
	} //end for
	//area reachability
	for (i = 0; i < aasworld.reachabilitysize; i++)
	{
		aasworld.reachability[i].areanum = LittleLong(aasworld.reachability[i].areanum);
		aasworld.reachability[i].facenum = LittleLong(aasworld.reachability[i].facenum);
		aasworld.reachability[i].edgenum = LittleLong(aasworld.reachability[i].edgenum);
		for (j = 0; j < 3; j++)
		{
			aasworld.reachability[i].start[j] = LittleFloat(aasworld.reachability[i].start[j]);
			aasworld.reachability[i].end[j] = LittleFloat(aasworld.reachability[i].end[j]);
		} //end for
		aasworld.reachability[i].traveltype = LittleLong(aasworld.reachability[i].traveltype);
		aasworld.reachability[i].traveltime = LittleShort(aasworld.reachability[i].traveltime);
	} //end for
	//nodes
	for (i = 0; i < aasworld.numnodes; i++)
	{
		aasworld.nodes[i].planenum = LittleLong(aasworld.nodes[i].planenum);
		aasworld.nodes[i].children[0] = LittleLong(aasworld.nodes[i].children[0]);
		aasworld.nodes[i].children[1] = LittleLong(aasworld.nodes[i].children[1]);
	} //end for
	//cluster portals
	for (i = 0; i < aasworld.numportals; i++)
	{
		aasworld.portals[i].areanum = LittleLong(aasworld.portals[i].areanum);
		aasworld.portals[i].frontcluster = LittleLong(aasworld.portals[i].frontcluster);
		aasworld.portals[i].backcluster = LittleLong(aasworld.portals[i].backcluster);
		aasworld.portals[i].clusterareanum[0] = LittleLong(aasworld.portals[i].clusterareanum[0]);
		aasworld.portals[i].clusterareanum[1] = LittleLong(aasworld.portals[i].clusterareanum[1]);
	} //end for
	//cluster portal index
	for (i = 0; i < aasworld.portalindexsize; i++)
	{
		aasworld.portalindex[i] = LittleLong(aasworld.portalindex[i]);
	} //end for
	//cluster
	for (i = 0; i < aasworld.numclusters; i++)
	{
		aasworld.clusters[i].numareas = LittleLong(aasworld.clusters[i].numareas);
		aasworld.clusters[i].numreachabilityareas = LittleLong(aasworld.clusters[i].numreachabilityareas);
		aasworld.clusters[i].numportals = LittleLong(aasworld.clusters[i].numportals);
		aasworld.clusters[i].firstportal = LittleLong(aasworld.clusters[i].firstportal);
	} //end for
}

bool Q3Map::AASLoadFile( const std::string &aasname ) {
	aas_header_t *header;
    byte *buffer;
    int lastoffset, offset, length;

    memset( &m_aasworld, 0, sizeof( m_aasworld ) );

    std::string contents;
    BloatFile( aasname, contents );

    if ( contents.empty() ) {
        msg_warning( "can't find aas file `%s'\n", aasname.c_str() );
        return false;
    }

    m_fileBase = buffer = (byte*)&contents[0];
    lastoffset = sizeof(aas_header_t);

    header = (aas_header_t *)buffer;

    if ( LittleLong( header->ident ) != AASID ) {
        msg_warning0( "aas-ident doesn't match\n" );
        return false;
    }

    if ( LittleLong( header->version ) != AASVERSION 
            && LittleLong( header->version ) != AASVERSION_OLD ) 
    {
        msg_warning0( "aas-version mismatch\n" );
        return false;
    }

	if ( LittleLong( header->version ) == AASVERSION)
	{
		AAS_DData((byte *) header + 8, sizeof(aas_header_t) - 8);
	}

    aas_t &aasworld = m_aasworld;

	offset = LittleLong(header->lumps[AASLUMP_BBOXES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_BBOXES].filelen);
	aasworld.bboxes = (aas_bbox_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_bbox_t));
	aasworld.numbboxes = length / sizeof(aas_bbox_t);
	if (aasworld.numbboxes && !aasworld.bboxes) return false;
	//vertexes
	offset = LittleLong(header->lumps[AASLUMP_VERTEXES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_VERTEXES].filelen);
	aasworld.vertexes = (aas_vertex_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_vertex_t));
	aasworld.numvertexes = length / sizeof(aas_vertex_t);
	if (aasworld.numvertexes && !aasworld.vertexes) return false;
	//planes
	offset = LittleLong(header->lumps[AASLUMP_PLANES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_PLANES].filelen);
	aasworld.planes = (aas_plane_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_plane_t));
	aasworld.numplanes = length / sizeof(aas_plane_t);
	if (aasworld.numplanes && !aasworld.planes) return false;
	//edges
	offset = LittleLong(header->lumps[AASLUMP_EDGES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_EDGES].filelen);
	aasworld.edges = (aas_edge_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_edge_t));
	aasworld.numedges = length / sizeof(aas_edge_t);
	if (aasworld.numedges && !aasworld.edges) return false;
	//edgeindex
	offset = LittleLong(header->lumps[AASLUMP_EDGEINDEX].fileofs);
	length = LittleLong(header->lumps[AASLUMP_EDGEINDEX].filelen);
	aasworld.edgeindex = (aas_edgeindex_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_edgeindex_t));
	aasworld.edgeindexsize = length / sizeof(aas_edgeindex_t);
	if (aasworld.edgeindexsize && !aasworld.edgeindex) return false;
	//faces
	offset = LittleLong(header->lumps[AASLUMP_FACES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_FACES].filelen);
	aasworld.faces = (aas_face_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_face_t));
	aasworld.numfaces = length / sizeof(aas_face_t);
	if (aasworld.numfaces && !aasworld.faces) return false;
	//faceindex
	offset = LittleLong(header->lumps[AASLUMP_FACEINDEX].fileofs);
	length = LittleLong(header->lumps[AASLUMP_FACEINDEX].filelen);
	aasworld.faceindex = (aas_faceindex_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_faceindex_t));
	aasworld.faceindexsize = length / sizeof(aas_faceindex_t);
	if (aasworld.faceindexsize && !aasworld.faceindex) return false;
	//convex areas
	offset = LittleLong(header->lumps[AASLUMP_AREAS].fileofs);
	length = LittleLong(header->lumps[AASLUMP_AREAS].filelen);
	aasworld.areas = (aas_area_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_area_t));
	aasworld.numareas = length / sizeof(aas_area_t);
	if (aasworld.numareas && !aasworld.areas) return false;
	//area settings
	offset = LittleLong(header->lumps[AASLUMP_AREASETTINGS].fileofs);
	length = LittleLong(header->lumps[AASLUMP_AREASETTINGS].filelen);
	aasworld.areasettings = (aas_areasettings_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_areasettings_t));
	aasworld.numareasettings = length / sizeof(aas_areasettings_t);
	if (aasworld.numareasettings && !aasworld.areasettings) return false;
	//reachability list
	offset = LittleLong(header->lumps[AASLUMP_REACHABILITY].fileofs);
	length = LittleLong(header->lumps[AASLUMP_REACHABILITY].filelen);
	aasworld.reachability = (aas_reachability_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_reachability_t));
	aasworld.reachabilitysize = length / sizeof(aas_reachability_t);
	if (aasworld.reachabilitysize && !aasworld.reachability) return false;
	//nodes
	offset = LittleLong(header->lumps[AASLUMP_NODES].fileofs);
	length = LittleLong(header->lumps[AASLUMP_NODES].filelen);
	aasworld.nodes = (aas_node_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_node_t));
	aasworld.numnodes = length / sizeof(aas_node_t);
	if (aasworld.numnodes && !aasworld.nodes) return false;
	//cluster portals
	offset = LittleLong(header->lumps[AASLUMP_PORTALS].fileofs);
	length = LittleLong(header->lumps[AASLUMP_PORTALS].filelen);
	aasworld.portals = (aas_portal_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_portal_t));
	aasworld.numportals = length / sizeof(aas_portal_t);
	if (aasworld.numportals && !aasworld.portals) return false;
	//cluster portal index
	offset = LittleLong(header->lumps[AASLUMP_PORTALINDEX].fileofs);
	length = LittleLong(header->lumps[AASLUMP_PORTALINDEX].filelen);
	aasworld.portalindex = (aas_portalindex_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_portalindex_t));
	aasworld.portalindexsize = length / sizeof(aas_portalindex_t);
	if (aasworld.portalindexsize && !aasworld.portalindex) return false;
	//clusters
	offset = LittleLong(header->lumps[AASLUMP_CLUSTERS].fileofs);
	length = LittleLong(header->lumps[AASLUMP_CLUSTERS].filelen);
	aasworld.clusters = (aas_cluster_t *) AAS_LoadAASLump(offset, length, &lastoffset, sizeof(aas_cluster_t));
	aasworld.numclusters = length / sizeof(aas_cluster_t);
	if (aasworld.numclusters && !aasworld.clusters) return false;
	//swap everything
	AAS_SwapAASData(m_aasworld);
	//aas file is loaded
	aasworld.loaded = qtrue;

    return true;
}

void Q3Map::AASResetMoveState() {
    memset( &m_movestate, 0, sizeof( m_movestate ) );
}

void Q3Map::AASInitMoveState( aas_initmove_t *initmove ) {
    AASResetMoveState();

    aas_movestate_t *ms = &m_movestate;

	VectorCopy(initmove->origin, ms->origin);
	VectorCopy(initmove->velocity, ms->velocity);
	VectorCopy(initmove->viewoffset, ms->viewoffset);
	ms->entitynum = initmove->entitynum;
	ms->client = initmove->client;
	ms->thinktime = initmove->thinktime;
	ms->presencetype = initmove->presencetype;
	VectorCopy(initmove->viewangles, ms->viewangles);
}

void Q3Map::AAS_ReachabilityFromNum(int num, struct aas_reachability_s *reach)
{
	if (!m_aasworld.initialized)
	{
		memset(reach, 0, sizeof(aas_reachability_t));
		return;
	} //end if
	if (num < 0 || num >= m_aasworld.reachabilitysize)
	{
		memset(reach, 0, sizeof(aas_reachability_t));
		return;
	}
	memcpy(reach, &m_aasworld.reachability[num], sizeof(aas_reachability_t));;
}

int Q3Map::AAS_AreaReachability(int areanum)
{
	if (areanum < 0 || areanum >= m_aasworld.numareas)
	{
		msg_failure("AAS_AreaReachability: areanum %d out of range", areanum);
		return 0;
	} //end if
	return m_aasworld.areasettings[areanum].numreachableareas;
}

int Q3Map::AAS_PointAreaNum(vec3_t point) {
	int nodenum;
	float	dist;
	aas_node_t *node;
	aas_plane_t *plane;

	if (!m_aasworld.loaded)
	{
		msg_warning0( "AAS_PointAreaNum: aas not loaded\n" );
		return 0;
	} //end if

	//start with node 1 because node zero is a dummy used for solid leafs
	nodenum = 1;
	while (nodenum > 0)
	{
		node = &m_aasworld.nodes[nodenum];
		plane = &m_aasworld.planes[node->planenum];
		float dist = DotProduct(point, plane->normal) - plane->dist;
		if (dist > 0) nodenum = node->children[0];
		else nodenum = node->children[1];
	} //end while
	if (!nodenum)
	{
		return 0;
	} //end if
	return -nodenum;
}

typedef struct aas_tracestack_s
{
	vec3_t start;		//start point of the piece of line to trace
	vec3_t end;			//end point of the piece of line to trace
	int planenum;		//last plane used as splitter
	int nodenum;		//node found after splitting with planenum
} aas_tracestack_t;

int Q3Map::AAS_TraceAreas(vec3_t start, vec3_t end, 
        int *areas, vec3_t *points, int maxareas) 
{
	int side, nodenum, tmpplanenum;
	int numareas;
	float front, back, frac;
	vec3_t cur_start, cur_end, cur_mid;
	aas_tracestack_t tracestack[127];
	aas_tracestack_t *tstack_p;
	aas_node_t *aasnode;
	aas_plane_t *plane;

	numareas = 0;
	areas[0] = 0;
	if (!m_aasworld.loaded) return numareas;

	tstack_p = tracestack;
	//we start with the whole line on the stack
	VectorCopy(start, tstack_p->start);
	VectorCopy(end, tstack_p->end);
	tstack_p->planenum = 0;
	//start with node 1 because node zero is a dummy for a solid leaf
	tstack_p->nodenum = 1;		//starting at the root of the tree
	tstack_p++;

	while (1)
	{
		//pop up the stack
		tstack_p--;
		//if the trace stack is empty (ended up with a piece of the
		//line to be traced in an area)
		if (tstack_p < tracestack)
		{
			return numareas;
		} //end if
		//number of the current node to test the line against
		nodenum = tstack_p->nodenum;
		//if it is an area
		if (nodenum < 0)
		{
			areas[numareas] = -nodenum;
			if (points) VectorCopy(tstack_p->start, points[numareas]);
			numareas++;
			if (numareas >= maxareas) return numareas;
			continue;
		} //end if
		//if it is a solid leaf
		if (!nodenum)
		{
			continue;
		} //end if
		//the node to test against
		aasnode = &m_aasworld.nodes[nodenum];
		//start point of current line to test against node
		VectorCopy(tstack_p->start, cur_start);
		//end point of the current line to test against node
		VectorCopy(tstack_p->end, cur_end);
		//the current node plane
		plane = &m_aasworld.planes[aasnode->planenum];

		switch(plane->type)
		{
			default: //gee it's not an axial plane
			{
				front = DotProduct(cur_start, plane->normal) - plane->dist;
				back = DotProduct(cur_end, plane->normal) - plane->dist;
				break;
			} //end default
		} //end switch

		//if the whole to be traced line is totally at the front of this node
		//only go down the tree with the front child
		if (front > 0 && back > 0)
		{
			//keep the current start and end point on the stack
			//and go down the tree with the front child
			tstack_p->nodenum = aasnode->children[0];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				msg_failure0( "AAS_TraceAreas: stack overflow\n" );
				return numareas;
			} //end if
		} //end if
		//if the whole to be traced line is totally at the back of this node
		//only go down the tree with the back child
		else if (front <= 0 && back <= 0)
		{
			//keep the current start and end point on the stack
			//and go down the tree with the back child
			tstack_p->nodenum = aasnode->children[1];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				msg_failure0( "AAS_TraceAreas: stack overflow\n");
				return numareas;
			} //end if
		} //end if
		//go down the tree both at the front and back of the node
		else
		{
			tmpplanenum = tstack_p->planenum;
			//calculate the hitpoint with the node (split point of the line)
			//put the crosspoint TRACEPLANE_EPSILON pixels on the near side
			if (front < 0) frac = (front)/(front-back);
			else frac = (front)/(front-back);
			if (frac < 0) frac = 0;
			else if (frac > 1) frac = 1;
			//frac = front / (front-back);
			//
			cur_mid[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac;
			cur_mid[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac;
			cur_mid[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac;

			//side the front part of the line is on
			side = front < 0;
			//first put the end part of the line on the stack (back side)
			VectorCopy(cur_mid, tstack_p->start);
			//not necesary to store because still on stack
			//VectorCopy(cur_end, tstack_p->end);
			tstack_p->planenum = aasnode->planenum;
			tstack_p->nodenum = aasnode->children[!side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				msg_failure0("AAS_TraceAreas: stack overflow\n");
				return numareas;
			} //end if
			//now put the part near the start of the line on the stack so we will
			//continue with thats part first. This way we'll find the first
			//hit of the bbox
			VectorCopy(cur_start, tstack_p->start);
			VectorCopy(cur_mid, tstack_p->end);
			tstack_p->planenum = tmpplanenum;
			tstack_p->nodenum = aasnode->children[side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				msg_failure0("AAS_TraceAreas: stack overflow\n");
				return numareas;
			} //end if
		} //end else
	} //end while
	return numareas;
}

static double VectorLength(vec3_t v)
{
	int		i;
	double	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

int Q3Map::AASFuzzyPointReachabilityArea(vec3_t origin) {
	int firstareanum, j, x, y, z;
	int areas[10], numareas, areanum, bestareanum;
	float dist, bestdist;
	vec3_t points[10], v, end;

	firstareanum = 0;
	areanum = AAS_PointAreaNum(origin);
	if (areanum)
	{
		firstareanum = areanum;
		if (AAS_AreaReachability(areanum)) return areanum;
	} //end if
	VectorCopy(origin, end);
	end[2] += 4;
	numareas = AAS_TraceAreas(origin, end, areas, points, 10);
	for (j = 0; j < numareas; j++)
	{
		if (AAS_AreaReachability(areas[j])) return areas[j];
	} //end for
	bestdist = 999999;
	bestareanum = 0;
	for (z = 1; z >= -1; z -= 1)
	{
		for (x = 1; x >= -1; x -= 1)
		{
			for (y = 1; y >= -1; y -= 1)
			{
				VectorCopy(origin, end);
				end[0] += x * 8;
				end[1] += y * 8;
				end[2] += z * 12;
				numareas = AAS_TraceAreas(origin, end, areas, points, 10);
				for (j = 0; j < numareas; j++)
				{
					if (AAS_AreaReachability(areas[j]))
					{
						VectorSubtract(points[j], origin, v);
						dist = VectorLength(v);
						if (dist < bestdist)
						{
							bestareanum = areas[j];
							bestdist = dist;
						} //end if
					} //end if
					if (!firstareanum) firstareanum = areas[j];
				} //end for
			} //end for
		} //end for
		if (bestareanum) return bestareanum;
	} //end for
	return firstareanum;
}

void Q3Map::AASClearMoveResult(aas_moveresult_t *result) {
    memset(result, 0, sizeof(*result));
}

void Q3Map::AAS_EA_Move(vec3_t dir, float speed) {
    aas_movestate_t *ms = &m_movestate;

    VectorMA( ms->origin, speed / 10, dir, ms->origin );

    /* here is your logic to move */
}

aas_moveresult_t Q3Map::AASMoveInGoalArea(aas_movestate_t *ms, aas_goal_t *goal)
{
	aas_moveresult_t result;
	vec3_t dir;
	float dist, speed;

	AASClearMoveResult(&result);
	//walk straight to the goal origin
	dir[0] = goal->origin[0] - ms->origin[0];
	dir[1] = goal->origin[1] - ms->origin[1];
	dir[2] = 0;
    result.traveltype = TRAVEL_WALK;

	dist = VectorNormalize(dir);
	if (dist > 100) dist = 100;
	speed = 4 * dist;
	if (speed < 10) speed = 0;
	//
	//elemantary action move in direction
	AAS_EA_Move(dir, speed);
	VectorCopy(dir, result.movedir);
	ms->lastreachnum = 0;
	ms->lastareanum = 0;
	ms->lastgoalareanum = goal->areanum;
	VectorCopy(ms->origin, ms->lastorigin);
	return result;
}

int Q3Map::AAS_Time() const { return (float)idLib::Milliseconds() / 1000; }

int Q3Map::AASReachabilityTime(aas_reachability_t *reach)
{
	switch(reach->traveltype & TRAVELTYPE_MASK)
	{
		case TRAVEL_WALK: return 5;
		case TRAVEL_CROUCH: return 5;
		case TRAVEL_BARRIERJUMP: return 5;
		case TRAVEL_LADDER: return 6;
		case TRAVEL_WALKOFFLEDGE: return 5;
		case TRAVEL_JUMP: return 5;
		case TRAVEL_SWIM: return 5;
		case TRAVEL_WATERJUMP: return 5;
		case TRAVEL_TELEPORT: return 5;
		case TRAVEL_ELEVATOR: return 10;
		case TRAVEL_GRAPPLEHOOK: return 8;
		case TRAVEL_ROCKETJUMP: return 6;
		case TRAVEL_BFGJUMP: return 6;
		case TRAVEL_JUMPPAD: return 10;
		case TRAVEL_FUNCBOB: return 10;
		default:
		{
			msg_warning("travel type %d not implemented yet\n", reach->traveltype);
			return 8;
		} //end case
	} //end switch
}

bool Q3Map::MoveToPosition( const idVec3 &from, const idVec3 &to ) {
    aas_initmove_t initMove = {0};
    VectorCopy( from.ToFloatPtr(), initMove.origin );

    AASInitMoveState( &initMove );

    aas_goal_t goal = {0};
    VectorCopy( to.ToFloatPtr(), goal.origin );

    goal.areanum = AASFuzzyPointReachabilityArea( goal.origin );

    if ( goal.areanum == 0 ) {
        msg_warning0( "no area found in to origin" );
        return false;
    }

    goal.maxs[0] = goal.maxs[1] = goal.maxs[2] = 5;

    aas_movestate_t *ms = &m_movestate;
    aas_moveresult_t *result = &m_moveresult;
    AASClearMoveResult( result );

    while ( 1 ) {
        AASMoveToGoal( &goal );

        if ( result->failure ) {
            return false;
        }

        vec3_t sub;
        VectorSubtract( goal.origin, ms->origin, sub );

        if ( VectorLength( sub ) < 10.0f ) {
            break;
        }
    }

    return true;
}

void Q3Map::AASMoveToGoal( aas_goal_t *goal ) {
    aas_reachability_t reach;
    int travelflags = MFL_WALK;
    aas_movestate_t *ms = &m_movestate;
    aas_moveresult_t *result = &m_moveresult;

    ms->areanum = AASFuzzyPointReachabilityArea(ms->origin);

    if ( !ms->areanum ) {
		result->failure = qtrue;
		result->blocked = qtrue;
		result->blockentity = 0;
		result->type = RESULTTYPE_INSOLIDAREA;
        return;
    }

    if ( ms->areanum == goal->areanum ) {
        *result = AASMoveInGoalArea(ms, goal);
        return;
    }

    int reachnum = ms->lastreachnum;
    if (reachnum)
    {
        AAS_ReachabilityFromNum(reachnum, &reach);
        //check if the reachability is still valid
        {
            //if the goal area changed or the reachability timed out
            //or the area changed
            if (ms->lastgoalareanum != goal->areanum ||
                    ms->reachability_time < AAS_Time() ||
                    ms->lastareanum != ms->areanum)
            {
                reachnum = 0;
            } //end else if
        } //end else
    }

    int resultflags = 0;
    if (!reachnum)
    {
        //if the area has no reachability links
        if (!AAS_AreaReachability(ms->areanum))
        {
            msg_warning("area %d no reachability\n", ms->areanum);
        } //end if
        //get a new reachability leading towards the goal
        reachnum = AASGetReachabilityToGoal(ms->origin, ms->areanum,
            ms->lastgoalareanum, ms->lastareanum,
            ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries,
            goal, travelflags, travelflags, NULL, 
            ms->numavoidspots, &resultflags);
        //the area number the reachability starts in
        ms->reachareanum = ms->areanum;
        //reset some state variables
        ms->jumpreach = 0;                        //for TRAVEL_JUMP
        ms->moveflags &= ~MFL_GRAPPLERESET;    //for TRAVEL_GRAPPLEHOOK
        //if there is a reachability to the goal
        if (reachnum)
        {
            AAS_ReachabilityFromNum(reachnum, &reach);
            //set a timeout for this reachability
            ms->reachability_time = AAS_Time() + AASReachabilityTime(&reach);
            //
        } //end if
    }

    ms->lastreachnum = reachnum;
    ms->lastgoalareanum = goal->areanum;
    ms->lastareanum = ms->areanum;

    if (reachnum)
    {
        //get the reachability from the number
        AAS_ReachabilityFromNum(reachnum, &reach);
        result->traveltype = reach.traveltype;
        //
        switch(reach.traveltype & TRAVELTYPE_MASK)
        {
            case TRAVEL_WALK: *result = AASTravel_Walk(ms, &reach); break;
            default:
            {
                msg_warning( "travel type %d not implemented yet\n", (reach.traveltype & TRAVELTYPE_MASK));
                break;
            } //end case
        } //end switch
        result->traveltype = reach.traveltype;
        result->flags |= resultflags;
    } //end if
	else
    {
        result->failure = qtrue;
        result->flags |= resultflags;
        memset(&reach, 0, sizeof(aas_reachability_t));
    }

    VectorCopy( ms->origin, ms->lastorigin );
}

int Q3Map::AAS_NextAreaReachability(int areanum, int reachnum)
{
	aas_areasettings_t *settings;

	if (!m_aasworld.initialized) return 0;

	if (areanum <= 0 || areanum >= m_aasworld.numareas)
	{
		msg_warning( "AAS_NextAreaReachability: areanum %d out of range\n", 
                areanum);
		return 0;
	} //end if

	settings = &m_aasworld.areasettings[areanum];
	if (!reachnum)
	{
		return settings->firstreachablearea;
	} //end if
	if (reachnum < settings->firstreachablearea)
	{
		msg_warning0( "AAS_NextAreaReachability:" 
                "reachnum < settings->firstreachableara");
		return 0;
	} //end if
	reachnum++;
	if (reachnum >= settings->firstreachablearea + settings->numreachableareas)
	{
		return 0;
	} //end if
	return reachnum;
}

int Q3Map::AAS_ClusterAreaNum(int clusternum, int areanum) {
	int side, areacluster;

	areacluster = m_aasworld.areasettings[areanum].cluster;
	if (areacluster > 0) return m_aasworld.areasettings[areanum].clusterareanum;
	else
	{
		side = m_aasworld.portals[-areacluster].frontcluster != clusternum;
		return m_aasworld.portals[-areacluster].clusterareanum[side];
	}
}

#define DISTANCEFACTOR_WALK			0.33f	//walk speed = 300
unsigned short int Q3Map::AAS_AreaTravelTime(int areanum, 
        vec3_t start, vec3_t end)
{
	int intdist;
	float dist;
	vec3_t dir;

	VectorSubtract(start, end, dir);
	dist = VectorLength(dir);
	//normal walk area
	dist *= DISTANCEFACTOR_WALK;
	//
	intdist = (int) dist;
	//make sure the distance isn't zero
	if (intdist <= 0) intdist = 1;
	return intdist;
}

aas_routingcache_t *Q3Map::AAS_AllocRoutingCache(int numtraveltimes)
{
	aas_routingcache_t *cache;
	int size;

	//
	size = sizeof(aas_routingcache_t)
						+ numtraveltimes * sizeof(unsigned short int)
						+ numtraveltimes * sizeof(unsigned char);
	//
	cache = (aas_routingcache_t *) malloc(size);
    memset((byte*)cache, 0, size);
	cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t)
								+ numtraveltimes * sizeof(unsigned short int);
	cache->size = size;
	return cache;
}

void Q3Map::AAS_UnlinkCache(aas_routingcache_t *cache) {
	if (cache->time_next) cache->time_next->time_prev = cache->time_prev;
	else m_aasworld.newestcache = cache->time_prev;
	if (cache->time_prev) cache->time_prev->time_next = cache->time_next;
	else m_aasworld.oldestcache = cache->time_next;
	cache->time_next = NULL;
	cache->time_prev = NULL;
}

void Q3Map::AAS_LinkCache(aas_routingcache_t *cache)
{
	if (m_aasworld.newestcache)
	{
		m_aasworld.newestcache->time_next = cache;
		cache->time_prev = m_aasworld.newestcache;
	} //end if
	else
	{
		m_aasworld.oldestcache = cache;
		cache->time_prev = NULL;
	} //end else
	cache->time_next = NULL;
	m_aasworld.newestcache = cache;
}

void Q3Map::AAS_UpdateAreaRoutingCache(aas_routingcache_t *cache) {
	int i, nextareanum, cluster, badtravelflags, clusterareanum, linknum;
	int numreachabilityareas;
	unsigned short int t, startareatraveltimes[128]; //NOTE: not more than 128 reachabilities per area allowed
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	aas_reversedreachability_t *revreach;
	aas_reversedlink_t *revlink;

	//number of reachability areas within this cluster
	numreachabilityareas = m_aasworld.clusters[cache->cluster].numreachabilityareas;
	//
	m_aasworld.frameroutingupdates++;
	//
	badtravelflags = ~cache->travelflags;
	//
	clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
	if (clusterareanum >= numreachabilityareas) return;
	//
	memset(startareatraveltimes, 0, sizeof(startareatraveltimes));
	//
	curupdate = &m_aasworld.areaupdate[clusterareanum];
	curupdate->areanum = cache->areanum;
	//VectorCopy(areacache->origin, curupdate->start);
	curupdate->areatraveltimes = startareatraveltimes;
	curupdate->tmptraveltime = cache->starttraveltime;
	//
	cache->traveltimes[clusterareanum] = cache->starttraveltime;
	//put the area to start with in the current read list
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		//
		curupdate->inlist = qfalse;
		//check all reversed reachability links
		revreach = &m_aasworld.reversedreachability[curupdate->areanum];
		//
		for (i = 0, revlink = revreach->first; 
                revlink; revlink = revlink->next, i++)
		{
			linknum = revlink->linknum;
			reach = &m_aasworld.reachability[linknum];
			//if not allowed to enter the next area
			if (m_aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED) continue;
			//number of the area the reversed reachability leads to
			nextareanum = revlink->areanum;
			//get the cluster number of the area
			cluster = m_aasworld.areasettings[nextareanum].cluster;
			//don't leave the cluster
			if (cluster > 0 && cluster != cache->cluster) continue;
			//get the number of the area in the cluster
			clusterareanum = AAS_ClusterAreaNum(cache->cluster, nextareanum);
			if (clusterareanum >= numreachabilityareas) continue;
			//time already travelled plus the traveltime through
			//the current area plus the travel time from the reachability
			t = curupdate->tmptraveltime 
                + curupdate->areatraveltimes[i] + reach->traveltime;

			if (!cache->traveltimes[clusterareanum] ||
					cache->traveltimes[clusterareanum] > t)
			{
				cache->traveltimes[clusterareanum] = t;
				cache->reachabilities[clusterareanum] = 
                    linknum - m_aasworld.areasettings[nextareanum].firstreachablearea;
				nextupdate = &m_aasworld.areaupdate[clusterareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				nextupdate->areatraveltimes 
                    = m_aasworld.areatraveltimes[nextareanum][linknum -
						m_aasworld.areasettings[nextareanum].firstreachablearea];
				if (!nextupdate->inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
}

aas_routingcache_t *Q3Map::AAS_GetAreaRoutingCache(int clusternum, int areanum, int travelflags) 
{
	int clusterareanum;
	aas_routingcache_t *cache, *clustercache;

	//number of the area in the cluster
	clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
	//pointer to the cache for the area in the cluster
	clustercache = m_aasworld.clusterareacache[clusternum][clusterareanum];
	//find the cache without undesired travel flags
	for (cache = clustercache; cache; cache = cache->next)
	{
		//if there aren't used any undesired travel types for the cache
		if (cache->travelflags == travelflags) break;
	} //end for
	//if there was no cache
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(m_aasworld.clusters[
                clusternum].numreachabilityareas);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(m_aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		cache->prev = NULL;
		cache->next = clustercache;
		if (clustercache) clustercache->prev = cache;
		m_aasworld.clusterareacache[clusternum][clusterareanum] = cache;
		AAS_UpdateAreaRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_Time();
	cache->type = CACHETYPE_AREA;
	AAS_LinkCache(cache);
	return cache;
}

void Q3Map::AAS_UpdatePortalRoutingCache(aas_routingcache_t *portalcache) {
	int i, portalnum, clusterareanum, clusternum;
	unsigned short int t;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *cache;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;

	//
	curupdate = &m_aasworld.portalupdate[m_aasworld.numportals];
	curupdate->cluster = portalcache->cluster;
	curupdate->areanum = portalcache->areanum;
	curupdate->tmptraveltime = portalcache->starttraveltime;
	//if the start area is a cluster portal, store the travel time for that portal
	clusternum = m_aasworld.areasettings[portalcache->areanum].cluster;
	if (clusternum < 0)
	{
		portalcache->traveltimes[-clusternum] = portalcache->starttraveltime;
	} //end if
	//put the area to start with in the current read list
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//remove the current update from the list
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		//current update is removed from the list
		curupdate->inlist = qfalse;
		//
		cluster = &m_aasworld.clusters[curupdate->cluster];
		//
		cache = AAS_GetAreaRoutingCache(curupdate->cluster,
								curupdate->areanum, portalcache->travelflags);
		//take all portals of the cluster
		for (i = 0; i < cluster->numportals; i++)
		{
			portalnum = m_aasworld.portalindex[cluster->firstportal + i];
			portal = &m_aasworld.portals[portalnum];
			//if this is the portal of the current update continue
			if (portal->areanum == curupdate->areanum) continue;
			//
			clusterareanum = AAS_ClusterAreaNum(curupdate->cluster, portal->areanum);
			if (clusterareanum >= cluster->numreachabilityareas) continue;
			//
			t = cache->traveltimes[clusterareanum];
			if (!t) continue;
			t += curupdate->tmptraveltime;
			//
			if (!portalcache->traveltimes[portalnum] ||
					portalcache->traveltimes[portalnum] > t)
			{
				portalcache->traveltimes[portalnum] = t;
				nextupdate = &m_aasworld.portalupdate[portalnum];
				if (portal->frontcluster == curupdate->cluster)
				{
					nextupdate->cluster = portal->backcluster;
				} //end if
				else
				{
					nextupdate->cluster = portal->frontcluster;
				} //end else
				nextupdate->areanum = portal->areanum;
				//add travel time through the actual portal area for the next update
				nextupdate->tmptraveltime = t 
                    + m_aasworld.portalmaxtraveltimes[portalnum];
				if (!nextupdate->inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
}

aas_routingcache_t *Q3Map::AAS_GetPortalRoutingCache(int clusternum, int areanum, int travelflags)
{
	aas_routingcache_t *cache;

	//find the cached portal routing if existing
	for (cache = m_aasworld.portalcache[areanum]; cache; cache = cache->next)
	{
		if (cache->travelflags == travelflags) break;
	} //end for
	//if the portal routing isn't cached
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(m_aasworld.numportals);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(m_aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		//add the cache to the cache list
		cache->prev = NULL;
		cache->next = m_aasworld.portalcache[areanum];
		if (m_aasworld.portalcache[areanum]) 
            m_aasworld.portalcache[areanum]->prev = cache;
		m_aasworld.portalcache[areanum] = cache;
		//update the cache
		AAS_UpdatePortalRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_Time();
	cache->type = CACHETYPE_PORTAL;
	AAS_LinkCache(cache);
	return cache;
}

int Q3Map::AAS_AreaRouteToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags, int *traveltime, int *reachnum)
{
	int clusternum, goalclusternum, portalnum, i, clusterareanum, bestreachnum;
	unsigned short int t, besttime;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *areacache, *portalcache;
	aas_reachability_t *reach;

	if (!m_aasworld.initialized) return qfalse;

	if (areanum == goalareanum)
	{
		*traveltime = 1;
		*reachnum = 0;
		return qtrue;
	}
	//
	if (areanum <= 0 || areanum >= m_aasworld.numareas)
	{
		msg_warning( "AAS_AreaTravelTimeToGoalArea: areanum %d out of range\n", 
                areanum);
		return qfalse;
	} //end if
	if (goalareanum <= 0 || goalareanum >= m_aasworld.numareas)
	{
		msg_warning( "AAS_AreaTravelTimeToGoalArea: goalareanum %d out of range\n",
            goalareanum);
		return qfalse;
	} //end if
	// make sure the routing cache doesn't grow to large
    /*
	while(AAS_AvailableMemory() < 10 * 1024 * 1024) {
		if (!AAS_FreeOldestCache()) break;
	} */
	//
	clusternum = m_aasworld.areasettings[areanum].cluster;
	goalclusternum = m_aasworld.areasettings[goalareanum].cluster;
	//check if the area is a portal of the goal area cluster
	if (clusternum < 0 && goalclusternum > 0)
	{
		portal = &m_aasworld.portals[-clusternum];
		if (portal->frontcluster == goalclusternum ||
				portal->backcluster == goalclusternum)
		{
			clusternum = goalclusternum;
		} //end if
	} //end if
	//check if the goalarea is a portal of the area cluster
	else if (clusternum > 0 && goalclusternum < 0)
	{
		portal = &m_aasworld.portals[-goalclusternum];
		if (portal->frontcluster == clusternum ||
				portal->backcluster == clusternum)
		{
			goalclusternum = clusternum;
		} //end if
	} //end if
	//if both areas are in the same cluster
	//NOTE: there might be a shorter route via another cluster!!! but we don't care
	if (clusternum > 0 && goalclusternum > 0 && clusternum == goalclusternum)
	{
		//
		areacache = AAS_GetAreaRoutingCache(clusternum, goalareanum, travelflags);
		//the number of the area in the cluster
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//the cluster the area is in
		cluster = &m_aasworld.clusters[clusternum];
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster->numreachabilityareas) return 0;
		//if it is possible to travel to the goal area through this cluster
		if (areacache->traveltimes[clusterareanum] != 0)
		{
			*reachnum = m_aasworld.areasettings[areanum].firstreachablearea +
							areacache->reachabilities[clusterareanum];
			if (!origin) {
				*traveltime = areacache->traveltimes[clusterareanum];
				return qtrue;
			}
			reach = &m_aasworld.reachability[*reachnum];
			*traveltime = areacache->traveltimes[clusterareanum] +
							AAS_AreaTravelTime(areanum, origin, reach->start);
			//
			return qtrue;
		} //end if
	} //end if
	//
	clusternum = m_aasworld.areasettings[areanum].cluster;
	goalclusternum = m_aasworld.areasettings[goalareanum].cluster;
	//if the goal area is a portal
	if (goalclusternum < 0)
	{
		//just assume the goal area is part of the front cluster
		portal = &m_aasworld.portals[-goalclusternum];
		goalclusternum = portal->frontcluster;
	} //end if
	//get the portal routing cache
	portalcache = AAS_GetPortalRoutingCache(goalclusternum, goalareanum, travelflags);
	//if the area is a cluster portal, read directly from the portal cache
	if (clusternum < 0)
	{
		*traveltime = portalcache->traveltimes[-clusternum];
		*reachnum = m_aasworld.areasettings[areanum].firstreachablearea +
						portalcache->reachabilities[-clusternum];
		return qtrue;
	} //end if
	//
	besttime = 0;
	bestreachnum = -1;
	//the cluster the area is in
	cluster = &m_aasworld.clusters[clusternum];
	//find the portal of the area cluster leading towards the goal area
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = m_aasworld.portalindex[cluster->firstportal + i];
		//if the goal area isn't reachable from the portal
		if (!portalcache->traveltimes[portalnum]) continue;
		//
		portal = &m_aasworld.portals[portalnum];
		//get the cache of the portal area
		areacache = AAS_GetAreaRoutingCache(clusternum, portal->areanum, travelflags);
		//current area inside the current cluster
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster->numreachabilityareas) continue;
		//if the portal is NOT reachable from this area
		if (!areacache->traveltimes[clusterareanum]) continue;
		//total travel time is the travel time the portal area is from
		//the goal area plus the travel time towards the portal area
		t = portalcache->traveltimes[portalnum] + areacache->traveltimes[clusterareanum];
		t += m_aasworld.portalmaxtraveltimes[portalnum];
		//
		if (origin)
		{
			*reachnum = m_aasworld.areasettings[areanum].firstreachablearea 
                + areacache->reachabilities[clusterareanum];
			reach = m_aasworld.reachability + *reachnum;
			t += AAS_AreaTravelTime(areanum, origin, reach->start);
		} //end if
		//if the time is better than the one already found
		if (!besttime || t < besttime)
		{
			bestreachnum = *reachnum;
			besttime = t;
		} //end if
	} //end for
	if (bestreachnum < 0) {
		return qfalse;
	}
	*reachnum = bestreachnum;
	*traveltime = besttime;
	return qtrue;
}

int Q3Map::AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, &traveltime, &reachnum))
	{
		return traveltime;
	}
	return 0;
}

int Q3Map::AASGetReachabilityToGoal(
    vec3_t origin, int areanum, int lastgoalareanum, int lastareanum,
	int *avoidreach, float *avoidreachtimes, int *avoidreachtries,
    aas_goal_t *goal, int travelflags, int movetravelflags,
	void *avoidspots, int numavoidspots, int *flags ) 
{
	int i, t, besttime, bestreachnum, reachnum;
	aas_reachability_t reach;

	if (!areanum) return 0;

	besttime = 0;
	bestreachnum = 0;

    for (reachnum = AAS_NextAreaReachability(areanum, 0); reachnum;
		reachnum = AAS_NextAreaReachability(areanum, reachnum))
	{
		//get the reachability from the number
		AAS_ReachabilityFromNum(reachnum, &reach);
		//NOTE: do not go back to the previous area if the goal didn't change
		//NOTE: is this actually avoidance of local routing minima between two areas???
		if (lastgoalareanum == goal->areanum && reach.areanum == lastareanum) continue;
		//get the travel time
		t = AAS_AreaTravelTimeToGoalArea(reach.areanum, reach.end, 
                goal->areanum, travelflags);
		//if the goal area isn't reachable from the reachable area
		if (!t) continue;
		//if the bot should not use this reachability to avoid bad spots
		
		//add the travel time towards the area
		t += reach.traveltime;// + AAS_AreaTravelTime(areanum, origin, reach.start);
		//if the travel time is better than the ones already found
		if (!besttime || t < besttime)
		{
			besttime = t;
			bestreachnum = reachnum;
		} //end if
	}
    return bestreachnum;
}

aas_moveresult_t Q3Map::AASTravel_Walk(aas_movestate_t *ms, 
        aas_reachability_t *reach)
{
	float dist, speed;
	vec3_t hordir;
	aas_moveresult_t result;

	AASClearMoveResult(&result);
	//first walk straight to the reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	//
	if (dist < 10)
	{
		//walk straight to the reachability end
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		dist = VectorNormalize(hordir);
	} //end if
    AAS_EA_Move(hordir, speed);
	VectorCopy(hordir, result.movedir);
	//
	return result;
}


#undef qtrue
#undef qfalse
#undef qboolean
