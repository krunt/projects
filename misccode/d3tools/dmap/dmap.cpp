/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "dmap.h"

dmapGlobals_t	dmapGlobals;
idBounds gBounds;

void SetupGBounds( bspface_t *faces ) {
    idBounds b;
    bspface_t *face;

    gBounds.Clear();
    for ( face = faces; face ; face = face->next ) {

        face->w->GetBounds( b );
        gBounds += b;
    }

    dmapGlobals.drawBounds = gBounds;
    dmapGlobals.drawBounds.ExpandSelf( gBounds.GetRadius( gBounds.GetCenter() ) * 0.2 );
}

void DrawFaceBSP_r( node_t *node, idWinding &centerWinding ) {
    if ( !node || node->planenum == PLANENUM_LEAF ) {
        return;
    }

    idWinding w;
    idPlane &plane = dmapGlobals.mapPlanes[node->planenum];
    idVec3  center = node->bounds.GetCenter();
    float r = node->bounds.GetRadius( center );
    idVec3 vx, vy;

    plane.Normal().NormalVectors( vx, vy );
    vx.Normalize(); vy.Normalize();

    w.AddPoint( center - vx * r - vy * r );
    w.AddPoint( center + vx * r - vy * r );
    w.AddPoint( center + vx * r + vy * r );
    w.AddPoint( center - vx * r + vy * r );

    centerWinding.AddPoint( center );

    BeginScene( gBounds, 1 );
    DrawWinding( &w, 0xFF0000 );
    DrawWinding( &centerWinding, 0x00FF00 );
    EndScene();

    DrawFaceBSP_r( node->children[0] );
    DrawFaceBSP_r( node->children[1] );
}

void DrawFaceBSP( tree_t *tree ) {
    if ( !dmapGlobals.drawflag ) {
        return;
    }

    //BeginScene(gBounds);
    idWinding centerWinding;
    DrawFaceBSP_r( tree->headnode, centerWinding );
    //EndScene();
}

void DrawTreePortals_r( node_t *node ) {
    int side;

    if ( !node ) {
        return;
    }

    if ( node->planenum == PLANENUM_LEAF ) {

        for ( uPortal_t *portal = node->portals; portal ; 
                portal = portal->next[side] )
        {
            if ( node == portal->nodes[0] ) {
                side = 0;
            } else {
                side = 1;
            }
    
            DrawWinding( portal->winding, 0x000000 );
        }

        return;
    }

    DrawTreePortals_r( node->children[0] );
    DrawTreePortals_r( node->children[1] );
}

void DrawTreePortals( tree_t *tree ) {
    if ( !dmapGlobals.drawflag ) {
        return;
    }

    BeginScene( gBounds, 2 );
    DrawTreePortals_r( tree->headnode );
    EndScene();
}

void DrawOccupiedBounds_r( node_t *node ) {
    if ( !node ) {
        return;
    }

    if ( node->planenum == PLANENUM_LEAF ) {
        DrawBounds( node->bounds, 
            node->occupied ? 0x000000 : 0xFF0000 );
        return;
    }

    DrawOccupiedBounds_r( node->children[0] );
    DrawOccupiedBounds_r( node->children[1] );
}

void DrawOccupiedBounds( tree_t *tree ) {
    if ( !dmapGlobals.drawflag ) {
        return;
    }

    BeginScene( gBounds, 3 );
    DrawOccupiedBounds_r( tree->headnode );
    EndScene();
}

void DrawSideWindings( uEntity_t *e ) {
    int i;
    primitive_t *prim;
    uBrush_t *b;

    if ( !dmapGlobals.drawflag ) {
        return;
    }

    BeginScene( gBounds, 4 );

    for ( prim = e->primitives ; prim ; prim = prim->next ) {
        b = prim->brush;
        if ( !b ) {
            continue;
        }

        for ( i = 0; i < b->numsides; ++i ) {
            side = &b->sides[i];
            if ( !side->winding ) 
                continue;
            DrawWinding( side->winding, 0x0000FF );
        }
    }

    EndScene();
}

void DrawSideVisibleHulls( uEntity_t *e ) {
    int i;
    primitive_t *prim;
    uBrush_t *b;

    if ( !dmapGlobals.drawflag ) {
        return;
    }

    BeginScene( gBounds, 5 );

    for ( prim = e->primitives ; prim ; prim = prim->next ) {
        b = prim->brush;
        if ( !b ) {
            continue;
        }

        for ( i = 0; i < b->numsides; ++i ) {
            side = &b->sides[i];
            if ( !side->visibleHull ) 
                continue;
            DrawWinding( side->visibleHull, 0x000000 );
        }
    }

    EndScene();
}

void DrawAreaOptimizedGroups_r( node_t *node ) {
    uArea_t *area;
    optimizeGroup_t *group;

    if ( !node ) {
        return;
    }

    if ( node->planenum == PLANENUM_LEAF ) {

        if ( node->area >= 0 && !node->opaque ) {
            area = &e->areas[node->area];
            for ( group = area->groups ; group ; group = group->nextGroup ) {
                DrawBounds( group->bounds, 0xFF00FF );
            }
        }

        return;
    }

    DrawAreaOptimizedGroups_r( node->children[0] );
    DrawAreaOptimizedGroups_r( node->children[1] );
}

void DrawAreaOptimizedGroups( tree_t *tree ) {
    if ( !dmapGlobals.drawFlag )
        return;

    DrawAreaOptimizedGroups_r( tree->headnode );
}

void DrawAreaTriLists_r( node_t *node ) {
    uArea_t *area;
    optimizeGroup_t *group;

    if ( !node ) {
        return;
    }

    if ( node->planenum == PLANENUM_LEAF ) {

        if ( node->area >= 0 && !node->opaque ) {
            area = &e->areas[node->area];
            for ( group = area->groups ; group ; group = group->nextGroup ) {
                for ( tri = group->triList ; tri ; tri = tri->next ) {
                    idWinding w;

                    w.AddPoint( tri->v[0].xyz );
                    w.AddPoint( tri->v[1].xyz );
                    w.AddPoint( tri->v[2].xyz );

                    DrawWinding( &w, 0xFF00FF );
                }
            }
        }

        return;
    }

    DrawAreaTriLists_r( node->children[0] );
    DrawAreaTriLists_r( node->children[1] );
}

void DrawAreaTriLists( tree_t *tree ) {
    if ( !dmapGlobals.drawFlag )
        return;

    BeginScene( gBounds, 6 );
    DrawAreaTriLists_r( tree->headnode );
    EndScene();
}


/*
============
ProcessModel
============
*/
bool ProcessModel( uEntity_t *e, bool floodFill ) {
	bspface_t	*faces;

	// build a bsp tree using all of the sides
	// of all of the structural brushes
	faces = MakeStructuralBspFaceList ( e->primitives );

    SetupGBounds( faces );

	e->tree = FaceBSP( faces );

    DrawFaceBSP( e->tree );

	// create portals at every leaf intersection
	// to allow flood filling
	MakeTreePortals( e->tree );

    DrawTreePortals( e->tree );

	// classify the leafs as opaque or areaportal
	FilterBrushesIntoTree( e );

	// see if the bsp is completely enclosed
	if ( floodFill && !dmapGlobals.noFlood ) {
		if ( FloodEntities( e->tree ) ) {
			// set the outside leafs to opaque
			FillOutside( e );
		} else {
			common->Printf ( "**********************\n" );
			common->Warning( "******* leaked *******" );
			common->Printf ( "**********************\n" );
			LeakFile( e->tree );
			// bail out here.  If someone really wants to
			// process a map that leaks, they should use
			// -noFlood
			return false;
		}
	}

    DrawOccupiedBounds( e->tree );

	// get minimum convex hulls for each visible side
	// this must be done before creating area portals,
	// because the visible hull is used as the portal
	ClipSidesByTree( e );

	// determine areas before clipping tris into the
	// tree, so tris will never cross area boundaries
	FloodAreas( e );

	// we now have a BSP tree with solid and non-solid leafs marked with areas
	// all primitives will now be clipped into this, throwing away
	// fragments in the solid areas
	PutPrimitivesInAreas( e );

    DrawSideWindings( e );
    DrawSideVisibleHulls( e );

    DrawAreaOptimizedGroups( e->tree );
    DrawAreaTriLists( e->tree );

	// now build shadow volumes for the lights and split
	// the optimize lists by the light beam trees
	// so there won't be unneeded overdraw in the static
	// case
	Prelight( e );

	// optimizing is a superset of fixing tjunctions
	if ( !dmapGlobals.noOptimize ) {
		OptimizeEntity( e );
	} else  if ( !dmapGlobals.noTJunc ) {
		FixEntityTjunctions( e );
	}

	// now fix t junctions across areas
	FixGlobalTjunctions( e );

	return true;
}

/*
============
ProcessModels
============
*/
bool ProcessModels( void ) {
	bool	oldVerbose;
	uEntity_t	*entity;

	oldVerbose = dmapGlobals.verbose;

	for ( dmapGlobals.entityNum = 0 ; dmapGlobals.entityNum < dmapGlobals.num_entities ; dmapGlobals.entityNum++ ) {

		entity = &dmapGlobals.uEntities[dmapGlobals.entityNum];
		if ( !entity->primitives ) {
			continue;
		}

		common->Printf( "############### entity %i ###############\n", dmapGlobals.entityNum );

		// if we leaked, stop without any more processing
		if ( !ProcessModel( entity, (bool)(dmapGlobals.entityNum == 0 ) ) ) {
			return false;
		}

		// we usually don't want to see output for submodels unless
		// something strange is going on
		if ( !dmapGlobals.verboseentities ) {
			dmapGlobals.verbose = false;
		}
	}

	dmapGlobals.verbose = oldVerbose;

	return true;
}

/*
============
DmapHelp
============
*/
void DmapHelp( void ) {
	common->Printf(
		
	"Usage: dmap [options] mapfile\n"
	"Options:\n"
	"noCurves          = don't process curves\n"
	"noCM              = don't create collision map\n"
	"noAAS             = don't create AAS files\n"
	
	);
}

/*
============
ResetDmapGlobals
============
*/
void ResetDmapGlobals( void ) {
	dmapGlobals.mapFileBase[0] = '\0';
	dmapGlobals.dmapFile = NULL;
	dmapGlobals.mapPlanes.Clear();
	dmapGlobals.num_entities = 0;
	dmapGlobals.uEntities = NULL;
	dmapGlobals.entityNum = 0;
	dmapGlobals.mapLights.Clear();
	dmapGlobals.verbose = false;
	dmapGlobals.glview = false;
	dmapGlobals.noOptimize = false;
	dmapGlobals.verboseentities = false;
	dmapGlobals.noCurves = false;
	dmapGlobals.fullCarve = false;
	dmapGlobals.noModelBrushes = false;
	dmapGlobals.noTJunc = false;
	dmapGlobals.nomerge = false;
	dmapGlobals.noFlood = false;
	dmapGlobals.noClipSides = false;
	dmapGlobals.noLightCarve = false;
	dmapGlobals.noShadow = false;
	dmapGlobals.shadowOptLevel = SO_NONE;
	dmapGlobals.drawBounds.Clear();
	dmapGlobals.drawflag = true;
	dmapGlobals.totalShadowTriangles = 0;
	dmapGlobals.totalShadowVerts = 0;
}

/*
============
Dmap
============
*/
void Dmap( const idCmdArgs &args ) {
	int			i;
	int			start, end;
	char		path[1024];
	idStr		passedName;
	bool		leaked = false;
	bool		noCM = false;
	bool		noAAS = false;

	ResetDmapGlobals();

	if ( args.Argc() < 2 ) {
		DmapHelp();
		return;
	}

    InitScene();

	common->Printf("---- dmap ----\n");

	dmapGlobals.fullCarve = true;
	dmapGlobals.shadowOptLevel = SO_MERGE_SURFACES;		// create shadows by merging all surfaces, but no super optimization
//	dmapGlobals.shadowOptLevel = SO_CLIP_OCCLUDERS;		// remove occluders that are completely covered
//	dmapGlobals.shadowOptLevel = SO_SIL_OPTIMIZE;
//	dmapGlobals.shadowOptLevel = SO_CULL_OCCLUDED;

	dmapGlobals.noLightCarve = true;

	for ( i = 1 ; i < args.Argc() ; i++ ) {
		const char *s;

		s = args.Argv(i);
		if ( s[0] == '-' ) {
			s++;
			if ( s[0] == '\0' ) {
				continue;
			}
		}

		if ( !idStr::Icmp( s,"glview" ) ) {
			dmapGlobals.glview = true;
		} else if ( !idStr::Icmp( s, "v" ) ) {
			common->Printf( "verbose = true\n" );
			dmapGlobals.verbose = true;
		} else if ( !idStr::Icmp( s, "draw" ) ) {
			common->Printf( "drawflag = true\n" );
			dmapGlobals.drawflag = true;
		} else if ( !idStr::Icmp( s, "noFlood" ) ) {
			common->Printf( "noFlood = true\n" );
			dmapGlobals.noFlood = true;
		} else if ( !idStr::Icmp( s, "noLightCarve" ) ) {
			common->Printf( "noLightCarve = true\n" );
			dmapGlobals.noLightCarve = true;
		} else if ( !idStr::Icmp( s, "lightCarve" ) ) {
			common->Printf( "noLightCarve = false\n" );
			dmapGlobals.noLightCarve = false;
		} else if ( !idStr::Icmp( s, "noOpt" ) ) {
			common->Printf( "noOptimize = true\n" );
			dmapGlobals.noOptimize = true;
		} else if ( !idStr::Icmp( s, "verboseentities" ) ) {
			common->Printf( "verboseentities = true\n");
			dmapGlobals.verboseentities = true;
		} else if ( !idStr::Icmp( s, "noCurves" ) ) {
			common->Printf( "noCurves = true\n");
			dmapGlobals.noCurves = true;
		} else if ( !idStr::Icmp( s, "noModels" ) ) {
			common->Printf( "noModels = true\n" );
			dmapGlobals.noModelBrushes = true;
		} else if ( !idStr::Icmp( s, "noClipSides" ) ) {
			common->Printf( "noClipSides = true\n" );
			dmapGlobals.noClipSides = true;
		} else if ( !idStr::Icmp( s, "noCarve" ) ) {
			common->Printf( "noCarve = true\n" );
			dmapGlobals.fullCarve = false;
		} else if ( !idStr::Icmp( s, "shadowOpt" ) ) {
			dmapGlobals.shadowOptLevel = (shadowOptLevel_t)atoi( args.Argv( i+1 ) );
			common->Printf( "shadowOpt = %i\n",dmapGlobals.shadowOptLevel );
			i += 1;
		} else if ( !idStr::Icmp( s, "noTjunc" ) ) {
			// triangle optimization won't work properly without tjunction fixing
			common->Printf ("noTJunc = true\n" );
			dmapGlobals.noTJunc = true;
			dmapGlobals.noOptimize = true;
			common->Printf ("forcing noOptimize = true\n" );
		} else if ( !idStr::Icmp( s, "noCM" ) ) {
			noCM = true;
			common->Printf( "noCM = true\n" );
		} else if ( !idStr::Icmp( s, "noAAS" ) ) {
			noAAS = true;
			common->Printf( "noAAS = true\n" );
		} else if ( !idStr::Icmp( s, "editorOutput" ) ) {
#ifdef _WIN32
			com_outputMsg = true;
#endif
		} else {
			break;
		}
	}

	if ( i >= args.Argc() ) {
		common->Error( "usage: dmap [options] mapfile" );
	}

	passedName = args.Argv(i);		// may have an extension
	passedName.BackSlashesToSlashes();
	if ( passedName.Icmpn( "maps/", 4 ) != 0 ) {
		passedName = "maps/" + passedName;
	}

	idStr stripped = passedName;
	stripped.StripFileExtension();
	idStr::Copynz( dmapGlobals.mapFileBase, stripped, sizeof(dmapGlobals.mapFileBase) );

	bool region = false;
	// if this isn't a regioned map, delete the last saved region map
	if ( passedName.Right( 4 ) != ".reg" ) {
		sprintf( path, "%s.reg", dmapGlobals.mapFileBase );
		fileSystem->RemoveFile( path );
	} else {
		region = true;
	}


	passedName = stripped;

	// delete any old line leak files
	sprintf( path, "%s.lin", dmapGlobals.mapFileBase );
	fileSystem->RemoveFile( path );


	//
	// start from scratch
	//
	start = Sys_Milliseconds();

	if ( !LoadDMapFile( passedName ) ) {
		return;
	}

	if ( ProcessModels() ) {
		WriteOutputFile();
	} else {
		leaked = true;
	}

	FreeDMapFile();

	common->Printf( "%i total shadow triangles\n", dmapGlobals.totalShadowTriangles );
	common->Printf( "%i total shadow verts\n", dmapGlobals.totalShadowVerts );

	end = Sys_Milliseconds();
	common->Printf( "-----------------------\n" );
	common->Printf( "%5.0f seconds for dmap\n", ( end - start ) * 0.001f );

	if ( !leaked ) {

		if ( !noCM ) {

			// make sure the collision model manager is not used by the game
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "disconnect" );

			// create the collision map
			start = Sys_Milliseconds();

			collisionModelManager->LoadMap( dmapGlobals.dmapFile );
			collisionModelManager->FreeMap();

			end = Sys_Milliseconds();
			common->Printf( "-------------------------------------\n" );
			common->Printf( "%5.0f seconds to create collision map\n", ( end - start ) * 0.001f );
		}

		if ( !noAAS && !region ) {
			// create AAS files
			RunAAS_f( args );
		}
	}

	// free the common .map representation
	delete dmapGlobals.dmapFile;

	// clear the map plane list
	dmapGlobals.mapPlanes.Clear();

#ifdef _WIN32
	if ( com_outputMsg && com_hwndMsg != NULL ) {
		unsigned int msg = ::RegisterWindowMessage( DMAP_DONE );
		::PostMessage( com_hwndMsg, msg, 0, 0 );
	}
#endif
}

/*
============
Dmap_f
============
*/
void Dmap_f( const idCmdArgs &args ) {

	common->ClearWarnings( "running dmap" );

	// refresh the screen each time we print so it doesn't look
	// like it is hung
	common->SetRefreshOnPrint( true );
	Dmap( args );
	common->SetRefreshOnPrint( false );

	common->PrintWarnings();
}
