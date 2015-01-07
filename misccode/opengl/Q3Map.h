#ifndef Q3MAP__DEF_H
#define Q3MAP__DEF_H

#include "Common.h"
#include "q3_map_common.h"
#include "MyEntity.h"

#define MAX_FACE_POINTS 64
#define MAX_PATCH_SIZE 32
#define MAX_GRID_SIZE 65

class Q3Map: public MyEntity {
public:
    Q3Map( const Map &m = Map() )
        : MyEntity( m )
    {}

    virtual ~Q3Map() {}

    virtual void Spawn( void );
    virtual void Precache( void ) {}
    virtual void Think( int ms );
    virtual void Render( void );

private:
    void FromQ3( float in[3], float out[3] ) const;
    void FromQ3Plane( cplane_t &p ) const;

    bool RE_LoadWorldMap( const char *name );
    void R_LoadNodesAndLeafs(lump_t *nodeLump, lump_t *leafLump);
    void R_SetParent (mnode_t *node, mnode_t *parent);
    void R_LoadVisibility( lump_t *l );
    void R_LoadPlanes( lump_t *l );
    void R_LoadSurfaces( lump_t *surfs, lump_t *verts, lump_t *indexLump );
    void R_LoadMarksurfaces (lump_t *l);
    void ParseTriSurf( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf, int *indexes );
    void ParseFace( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf, int *indexes );
    mnode_t *FindLeaf( const idVec3 &pos );
    void MarkSurfaces( const idVec3 &pos );
    void RenderSurfaces( void );
    void RenderSurface( msurface_t *surf );
    void R_RecursiveWorldNode( const mnode_t *node );
    void RenderTriangleSoup( msurface_t *surf );
    void RenderFace( msurface_t *surf );
    byte *R_ClusterPVS( int ) const;
    void UncacheSurfaces( void );
    void R_LoadShaders( lump_t *l );
    void CacheBatches();
    void RenderGrid( msurface_t *surf );
    void ParseGrid( dsurface_t *ds, q3drawVert_t *verts, msurface_t *surf );
    srfGridMesh_t *R_SubdividePatchToGrid( int width, int height, q3drawVert_t *points );
    srfGridMesh_t *R_CreateSurfaceGridMesh(int width, int height,
		q3drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], float errorTable[2][MAX_GRID_SIZE] );
    void RB_SubmitBatch( int shader );

    q3world_t m_world;
    byte *m_fileBase;

    int m_viewCount;

    int m_numFaces, m_numMeshes, m_numTrisurfs, m_numFlares, m_numUndefined;

    struct BatchElement {
        std::vector<GLushort> m_indices;
        std::vector<drawVert_t> m_verts;
    };

    /* keyed by shader-keyindex */
    std::map<int, BatchElement> m_batch;

    std::vector<cached_surf_t> m_surfs;
};

#endif 
