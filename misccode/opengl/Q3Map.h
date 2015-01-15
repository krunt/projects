#ifndef Q3MAP__DEF_H
#define Q3MAP__DEF_H

#include "Common.h"
#include "q3_map_common.h"
#include "aas_common.h"
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

    void ConvertToQ2( const std::string &q2file );
    bool MoveToPosition( const idVec3 &from, const idVec3 &to );

private:
    void FromQ3( float in[3], float out[3] ) const;
    void FromQ3Plane( cplane_t &p ) const;
    void ToQ3( float in[3], float out[3] ) const;
    void ToQ3Plane( cplane_t &p ) const;

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
    void ConstructFrustum( const playerView_t &view );
    bool CullSurfaceAgainstFrustum( const idBounds &bounds ) const;

    /* Q2-Part */
    typedef std::vector<std::string> LumpsType;
    void Q2StoreVisibility( LumpsType &lumps );
    void Q2StoreSurfaces( LumpsType &lumps );
    void Q2StoreNodesAndLeafs( LumpsType &lumps );

    struct q2_texinfo_s;
    void Q3ToQ2Texture( int shader, char *texname );

    void PushEdge( LumpsType &lumps,
        std::map<unsigned int, int> &m, unsigned int v1, unsigned int v2 );

    /* AAS-Part */
    bool AASLoadFile( const std::string &filename );
    aas_moveresult_t AASTravel_Walk(aas_movestate_t *ms,
        aas_reachability_t *reach);
    int AASGetReachabilityToGoal(
        vec3_t origin, int areanum, int lastgoalareanum, int lastareanum,
    	int *avoidreach, float *avoidreachtimes, int *avoidreachtries,
        aas_goal_t *goal, int travelflags, int movetravelflags,
    	void *avoidspots, int numavoidspots, int *flags );
    int AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, 
            int goalareanum, int travelflags);
    int AAS_AreaRouteToGoalArea(int areanum, vec3_t origin, 
            int goalareanum, int travelflags, int *traveltime, int *reachnum);
    unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end);
    int AAS_ClusterAreaNum(int clusternum, int areanum);
    int AAS_NextAreaReachability(int areanum, int reachnum);
    void AASMoveToGoal( aas_goal_t *goal );
    int AASReachabilityTime(aas_reachability_t *reach);
    int AAS_Time() const;
    aas_moveresult_t AASMoveInGoalArea(aas_movestate_t *ms, aas_goal_t *goal);
    void AASClearMoveResult(aas_moveresult_t *result);
    int AASFuzzyPointReachabilityArea(vec3_t origin);
    int AAS_TraceAreas(vec3_t start, vec3_t end, 
        int *areas, vec3_t *points, int maxareas);
    int AAS_PointAreaNum(vec3_t point);
    int AAS_AreaReachability(int areanum);
    void AAS_ReachabilityFromNum(int num, struct aas_reachability_s *reach);
    void AASResetMoveState();
    void AASInitMoveState( aas_initmove_t *state );
    byte *AAS_LoadAASLump(int offset, int length, int *lastoffset, int size);
    void AAS_LinkCache(aas_routingcache_t *cache);
    void AAS_UnlinkCache(aas_routingcache_t *cache);
    aas_routingcache_t *AAS_AllocRoutingCache(int numtraveltimes);
    aas_routingcache_t *AAS_GetAreaRoutingCache(int clusternum, int areanum, int travelflags);
    void AAS_UpdatePortalRoutingCache(aas_routingcache_t *portalcache);
    aas_routingcache_t *AAS_GetPortalRoutingCache(int clusternum, 
            int areanum, int travelflags);
    void AAS_EA_Move(vec3_t dir, float speed);
    void AAS_UpdateAreaRoutingCache(aas_routingcache_t *cache);


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

    idPlane m_frustumPlanes[6];

    std::map<msurface_t*, int> surfaceMap; // value is offset

    aas_t m_aasworld;
    aas_movestate_t m_movestate;
    aas_moveresult_t m_moveresult;
};

#endif 
