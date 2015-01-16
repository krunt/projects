#ifndef AAS_COMMON__DEF_H_
#define AAS_COMMON__DEF_H_

#define TRAVELTYPE_MASK				0xFFFFFF

#define AASID						(('S'<<24)+('A'<<16)+('A'<<8)+'E')
#define AASVERSION_OLD				4
#define AASVERSION					5

#define AAS_LUMPS					14
#define AASLUMP_BBOXES				0
#define AASLUMP_VERTEXES			1
#define AASLUMP_PLANES				2
#define AASLUMP_EDGES				3
#define AASLUMP_EDGEINDEX			4
#define AASLUMP_FACES				5
#define AASLUMP_FACEINDEX			6
#define AASLUMP_AREAS				7
#define AASLUMP_AREASETTINGS		8
#define AASLUMP_REACHABILITY		9
#define AASLUMP_NODES				10
#define AASLUMP_PORTALS				11
#define AASLUMP_PORTALINDEX			12
#define AASLUMP_CLUSTERS			13

#define RESULTTYPE_ELEVATORUP			1		//elevator is up
#define RESULTTYPE_WAITFORFUNCBOBBING	2		//waiting for func bobbing to arrive
#define RESULTTYPE_BADGRAPPLEPATH		4		//grapple path is obstructed
#define RESULTTYPE_INSOLIDAREA			8		//stuck in solid area, this is bad

//area flags
#define AREA_GROUNDED				1		//bot can stand on the ground
#define AREA_LADDER					2		//area contains one or more ladder faces
#define AREA_LIQUID					4		//area contains a liquid
#define AREA_DISABLED				8		//area is disabled for routing when set
#define AREA_BRIDGE					16		//area ontop of a bridge

//movement types
#define MOVE_WALK						1
#define MOVE_CROUCH						2
#define MOVE_JUMP						4
#define MOVE_GRAPPLE					8
#define MOVE_ROCKETJUMP					16
#define MOVE_BFGJUMP					32
//move flags
#define MFL_BARRIERJUMP					1		//bot is performing a barrier jump
#define MFL_ONGROUND					2		//bot is in the ground
#define MFL_SWIMMING					4		//bot is swimming
#define MFL_AGAINSTLADDER				8		//bot is against a ladder
#define MFL_WATERJUMP					16		//bot is waterjumping
#define MFL_TELEPORTED					32		//bot is being teleported
#define MFL_GRAPPLEPULL					64		//bot is being pulled by the grapple
#define MFL_ACTIVEGRAPPLE				128		//bot is using the grapple hook
#define MFL_GRAPPLERESET				256		//bot has reset the grapple
#define MFL_WALK						512		//bot should walk slowly

//travel types
#define MAX_TRAVELTYPES				32
#define TRAVEL_INVALID				1		//temporary not possible
#define TRAVEL_WALK					2		//walking
#define TRAVEL_CROUCH				3		//crouching
#define TRAVEL_BARRIERJUMP			4		//jumping onto a barrier
#define TRAVEL_JUMP					5		//jumping
#define TRAVEL_LADDER				6		//climbing a ladder
#define TRAVEL_WALKOFFLEDGE			7		//walking of a ledge
#define TRAVEL_SWIM					8		//swimming
#define TRAVEL_WATERJUMP			9		//jump out of the water
#define TRAVEL_TELEPORT				10		//teleportation
#define TRAVEL_ELEVATOR				11		//travel by elevator
#define TRAVEL_ROCKETJUMP			12		//rocket jumping required for travel
#define TRAVEL_BFGJUMP				13		//bfg jumping required for travel
#define TRAVEL_GRAPPLEHOOK			14		//grappling hook required for travel
#define TRAVEL_DOUBLEJUMP			15		//double jump
#define TRAVEL_RAMPJUMP				16		//ramp jump
#define TRAVEL_STRAFEJUMP			17		//strafe jump
#define TRAVEL_JUMPPAD				18		//jump pad
#define TRAVEL_FUNCBOB				19		//func bob

typedef struct
{
	int fileofs;
	int filelen;
} aas_lump_t;

typedef struct aas_header_s
{
	int ident;
	int version;
	int bspchecksum;
	//data entries
	aas_lump_t lumps[AAS_LUMPS];
} aas_header_t;

typedef struct aas_bbox_s
{
	int presencetype;
	int flags;
	vec3_t mins, maxs;
} aas_bbox_t;

typedef vec3_t aas_vertex_t;

typedef struct aas_plane_s
{
	vec3_t normal;						//normal vector of the plane
	float dist;							//distance of the plane (normal vector * distance = point in plane)
	int type;
} aas_plane_t;

typedef struct aas_edge_s
{
	int v[2];							//numbers of the vertexes of this edge
} aas_edge_t;

typedef int aas_edgeindex_t;

typedef struct aas_face_s
{
	int planenum;						//number of the plane this face is in
	int faceflags;						//face flags (no use to create face settings for just this field)
	int numedges;						//number of edges in the boundary of the face
	int firstedge;						//first edge in the edge index
	int frontarea;						//area at the front of this face
	int backarea;						//area at the back of this face
} aas_face_t;

typedef int aas_faceindex_t;

typedef struct aas_area_s
{
	int areanum;						//number of this area
	//3d definition
	int numfaces;						//number of faces used for the boundary of the area
	int firstface;						//first face in the face index used for the boundary of the area
	vec3_t mins;						//mins of the area
	vec3_t maxs;						//maxs of the area
	vec3_t center;						//'center' of the area
} aas_area_t;

typedef struct aas_areasettings_s
{
	//could also add all kind of statistic fields
	int contents;						//contents of the area
	int areaflags;						//several area flags
	int presencetype;					//how a bot can be present in this area
	int cluster;						//cluster the area belongs to, if negative it's a portal
	int clusterareanum;				//number of the area in the cluster
	int numreachableareas;			//number of reachable areas from this one
	int firstreachablearea;			//first reachable area in the reachable area index
} aas_areasettings_t;

typedef struct aas_reachability_s
{
	int areanum;						//number of the reachable area
	int facenum;						//number of the face towards the other area
	int edgenum;						//number of the edge towards the other area
	vec3_t start;						//start point of inter area movement
	vec3_t end;							//end point of inter area movement
	int traveltype;					//type of travel required to get to the area
	unsigned short int traveltime;//travel time of the inter area movement
} aas_reachability_t;

typedef struct aas_node_s
{
	int planenum;
	int children[2];					//child nodes of this node, or areas as leaves when negative
										//when a child is zero it's a solid leaf
} aas_node_t;

typedef struct aas_portal_s
{
	int areanum;						//area that is the actual portal
	int frontcluster;					//cluster at front of portal
	int backcluster;					//cluster at back of portal
	int clusterareanum[2];			//number of the area in the front and back cluster
} aas_portal_t;

typedef int aas_portalindex_t;

typedef struct aas_cluster_s
{
	int numareas;						//number of areas in the cluster
	int numreachabilityareas;			//number of areas with reachabilities
	int numportals;						//number of cluster portals
	int firstportal;					//first cluster portal in the index
} aas_cluster_t;

typedef struct aas_link_s
{
	int entnum;
	int areanum;
	struct aas_link_s *next_ent, *prev_ent;
	struct aas_link_s *next_area, *prev_area;
} aas_link_t;

#define CACHETYPE_PORTAL		0
#define CACHETYPE_AREA			1

//routing cache
typedef struct aas_routingcache_s
{
	byte type;									//portal or area cache
	float time;									//last time accessed or updated
	int size;									//size of the routing cache
	int cluster;								//cluster the cache is for
	int areanum;								//area the cache is created for
	vec3_t origin;								//origin within the area
	float starttraveltime;						//travel time to start with
	int travelflags;							//combinations of the travel flags
	struct aas_routingcache_s *prev, *next;
	struct aas_routingcache_s *time_prev, *time_next;
	unsigned char *reachabilities;				//reachabilities used for routing
	unsigned short int traveltimes[1];			//travel time for every area (variable sized)
} aas_routingcache_t;

//fields for the routing algorithm
typedef struct aas_routingupdate_s
{
	int cluster;
	int areanum;								//area number of the update
	vec3_t start;								//start point the area was entered
	unsigned short int tmptraveltime;			//temporary travel time
	unsigned short int *areatraveltimes;		//travel times within the area
	bool inlist;							//true if the update is in the list
	struct aas_routingupdate_s *next;
	struct aas_routingupdate_s *prev;
} aas_routingupdate_t;

//reversed reachability link
typedef struct aas_reversedlink_s
{
	int linknum;								//the aas_areareachability_t
	int areanum;								//reachable from this area
	struct aas_reversedlink_s *next;			//next link
} aas_reversedlink_t;

//reversed area reachability
typedef struct aas_reversedreachability_s
{
	int numlinks;
	aas_reversedlink_t *first;
} aas_reversedreachability_t;

//areas a reachability goes through
typedef struct aas_reachabilityareas_s
{
	int firstarea, numareas;
} aas_reachabilityareas_t;

typedef struct aas_settings_s
{
	vec3_t phys_gravitydirection;
	float phys_friction;
	float phys_stopspeed;
	float phys_gravity;
	float phys_waterfriction;
	float phys_watergravity;
	float phys_maxvelocity;
	float phys_maxwalkvelocity;
	float phys_maxcrouchvelocity;
	float phys_maxswimvelocity;
	float phys_walkaccelerate;
	float phys_airaccelerate;
	float phys_swimaccelerate;
	float phys_maxstep;
	float phys_maxsteepness;
	float phys_maxwaterjump;
	float phys_maxbarrier;
	float phys_jumpvel;
	float phys_falldelta5;
	float phys_falldelta10;
	float rs_waterjump;
	float rs_teleport;
	float rs_barrierjump;
	float rs_startcrouch;
	float rs_startgrapple;
	float rs_startwalkoffledge;
	float rs_startjump;
	float rs_rocketjump;
	float rs_bfgjump;
	float rs_jumppad;
	float rs_aircontrolledjumppad;
	float rs_funcbob;
	float rs_startelevator;
	float rs_falldamage5;
	float rs_falldamage10;
	float rs_maxfallheight;
	float rs_maxjumpfallheight;
} aas_settings_t;

typedef struct aas_moveresult_s
{
	int failure;				//true if movement failed all together
	int type;					//failure or blocked type
	int blocked;				//true if blocked by an entity
	int blockentity;			//entity blocking the bot
	int traveltype;				//last executed travel type
	int flags;					//result flags
	int weapon;					//weapon used for movement
	vec3_t movedir;				//movement direction
	vec3_t ideal_viewangles;	//ideal viewangles for the movement
} aas_moveresult_t;

typedef struct aas_goal_s
{
	vec3_t origin;				//origin of the goal
	int areanum;				//area number of the goal
	vec3_t mins, maxs;			//mins and maxs of the goal
	int entitynum;				//number of the goal entity
	int number;					//goal number
	int flags;					//goal flags
	int iteminfo;				//item information
} aas_goal_t;


#define MAX_AVOIDREACH					1
#define MAX_AVOIDSPOTS					32
#define	MAX_CONFIGSTRINGS	1024

typedef struct aas_initmove_s
{
	vec3_t origin;				//origin of the bot
	vec3_t velocity;			//velocity of the bot
	vec3_t viewoffset;			//view offset
	int entitynum;				//entity number of the bot
	int client;					//client number of the bot
	float thinktime;			//time the bot thinks
	int presencetype;			//presencetype of the bot
	vec3_t viewangles;			//view angles of the bot
	int or_moveflags;			//values ored to the movement flags
} aas_initmove_t;


typedef struct aas_movestate_s
{
	//input vars (all set outside the movement code)
	vec3_t origin;								//origin of the bot
	vec3_t velocity;							//velocity of the bot
	vec3_t viewoffset;							//view offset
	int entitynum;								//entity number of the bot
	int client;									//client number of the bot
	float thinktime;							//time the bot thinks
	int presencetype;							//presencetype of the bot
	vec3_t viewangles;							//view angles of the bot
	//state vars
	int areanum;								//area the bot is in
	int lastareanum;							//last area the bot was in
	int lastgoalareanum;						//last goal area number
	int lastreachnum;							//last reachability number
	vec3_t lastorigin;							//origin previous cycle
	int reachareanum;							//area number of the reachabilty
	int moveflags;								//movement flags
	int jumpreach;								//set when jumped
	float grapplevisible_time;					//last time the grapple was visible
	float lastgrappledist;						//last distance to the grapple end
	float reachability_time;					//time to use current reachability
	int avoidreach[MAX_AVOIDREACH];				//reachabilities to avoid
	float avoidreachtimes[MAX_AVOIDREACH];		//times to avoid the reachabilities
	int avoidreachtries[MAX_AVOIDREACH];		//number of tries before avoiding
	//
	void *avoidspots[MAX_AVOIDSPOTS];	//spots to avoid
	int numavoidspots;
} aas_movestate_t;




#define MAX_PATH 64

typedef struct aas_s
{
	int loaded;									//true when an AAS file is loaded
	int initialized;							//true when AAS has been initialized
	int savefile;								//set true when file should be saved
	int bspchecksum;
	//current time
	float time;
	int numframes;
	//name of the aas file
	char filename[MAX_PATH];
	char mapname[MAX_PATH];
	//bounding boxes
	int numbboxes;
	aas_bbox_t *bboxes;
	//vertexes
	int numvertexes;
	aas_vertex_t *vertexes;
	//planes
	int numplanes;
	aas_plane_t *planes;
	//edges
	int numedges;
	aas_edge_t *edges;
	//edge index
	int edgeindexsize;
	aas_edgeindex_t *edgeindex;
	//faces
	int numfaces;
	aas_face_t *faces;
	//face index
	int faceindexsize;
	aas_faceindex_t *faceindex;
	//convex areas
	int numareas;
	aas_area_t *areas;
	//convex area settings
	int numareasettings;
	aas_areasettings_t *areasettings;
	//reachablity list
	int reachabilitysize;
	aas_reachability_t *reachability;
	//nodes of the bsp tree
	int numnodes;
	aas_node_t *nodes;
	//cluster portals
	int numportals;
	aas_portal_t *portals;
	//cluster portal index
	int portalindexsize;
	aas_portalindex_t *portalindex;
	//clusters
	int numclusters;
	aas_cluster_t *clusters;
	//
	int numreachabilityareas;
	float reachabilitytime;
	//enities linked in the areas
	aas_link_t *linkheap;						//heap with link structures
	int linkheapsize;							//size of the link heap
	aas_link_t *freelinks;						//first free link
	aas_link_t **arealinkedentities;			//entities linked into areas
	//entities
	int maxentities;
	int maxclients;
    struct aas_entity_t;
	aas_entity_t *entities;
	//string indexes
	char *configstrings[MAX_CONFIGSTRINGS];
	int indexessetup;
	//index to retrieve travel flag for a travel type
	int travelflagfortype[MAX_TRAVELTYPES];
	//travel flags for each area based on contents
	int *areacontentstravelflags;
	//routing update
	aas_routingupdate_t *areaupdate;
	aas_routingupdate_t *portalupdate;
	//number of routing updates during a frame (reset every frame)
	int frameroutingupdates;
	//reversed reachability links
	aas_reversedreachability_t *reversedreachability;
	//travel times within the areas
	unsigned short ***areatraveltimes;
	//array of size numclusters with cluster cache
	aas_routingcache_t ***clusterareacache;
	aas_routingcache_t **portalcache;
	//cache list sorted on time
	aas_routingcache_t *oldestcache;		// start of cache list sorted on time
	aas_routingcache_t *newestcache;		// end of cache list sorted on time
	//maximum travel time through portal areas
	int *portalmaxtraveltimes;
	//areas the reachabilities go through
	int *reachabilityareaindex;
	aas_reachabilityareas_t *reachabilityareas;
} aas_t;

#endif /* AAS_COMMON__DEF_H_ */
