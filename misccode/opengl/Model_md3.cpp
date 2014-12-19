
#include "d3lib/Lib.h"
#include "Model_md3.h"

#include "Render.h"

#include "Utils.h"

#define	LL(x) x=LittleLong(x)

void GLRenderModelMD3::Spawn( void ) {
    m_pos = vec3_origin;
    m_axis = mat3_identity;

    Precache();
}

void GLRenderModelMD3::Precache( void ) 
{
	int					i, j;
	md3Header_t			*pinmodel;
        md3Frame_t			*frame;
	md3Surface_t		*surf;
	md3Shader_t			*shader;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3XyzNormal_t		*xyz;
	md3Tag_t			*tag;
	int					version;
	int					size;
        md3Header_t *md3;
        std::string buffer;
        surf_t gsurf;

        gsurf.m_matName = m_textureName;

        BloatFile( m_fileName, buffer );

	pinmodel = (md3Header_t *)buffer.data();

	version = LittleLong (pinmodel->version);
	if (version != MD3_VERSION) {
            msg_warning( "InitFromFile: %s has wrong version (%i should be %i)",
                m_fileName.c_str(), version, MD3_VERSION );
	    return;
	}

	size = LittleLong(pinmodel->ofsEnd);

        md3 = reinterpret_cast<md3Header_t *>(const_cast<char *>(buffer.data()));

        LL(md3->ident);
        LL(md3->version);
        LL(md3->numFrames);
        LL(md3->numTags);
        LL(md3->numSurfaces);
        LL(md3->ofsFrames);
        LL(md3->ofsTags);
        LL(md3->ofsSurfaces);
        LL(md3->ofsEnd);

        fprintf( stderr, "frames=%d, surfaces=%d\n", md3->numFrames, md3->numSurfaces );

	if ( md3->numFrames < 1 ) {
                msg_warning( "InitFromFile: %s has no frames", 
                    m_fileName.c_str() );
		return;
	}
    
	// swap all the frames
    frame = (md3Frame_t *) ( (byte *)md3 + md3->ofsFrames );
    for ( i = 0 ; i < md3->numFrames ; i++, frame++) {
    	frame->radius = LittleFloat( frame->radius );
        for ( j = 0 ; j < 3 ; j++ ) {
            frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
            frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
	    	frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
        }
	}

	// swap all the tags
    tag = (md3Tag_t *) ( (byte *)md3 + md3->ofsTags );
    for ( i = 0 ; i < md3->numTags * md3->numFrames ; i++, tag++) {
        for ( j = 0 ; j < 3 ; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
        }
	}

	// swap all the surfaces
	surf = (md3Surface_t *) ( (byte *)md3 + md3->ofsSurfaces );
	for ( i = 0 ; i < md3->numSurfaces ; i++) {

        LL(surf->ident);
        LL(surf->flags);
        LL(surf->numFrames);
        LL(surf->numShaders);
        LL(surf->numTriangles);
        LL(surf->ofsTriangles);
        LL(surf->numVerts);
        LL(surf->ofsShaders);
        LL(surf->ofsSt);
        LL(surf->ofsXyzNormals);
        LL(surf->ofsEnd);
		
		if ( surf->numVerts > SHADER_MAX_VERTEXES ) {
			msg_failure( 
                "InitFromFile: %s has more than %i verts on a surface (%i)",
			m_fileName.c_str(), SHADER_MAX_VERTEXES, surf->numVerts );
		}
		if ( surf->numTriangles*3 > SHADER_MAX_INDEXES ) {
			msg_failure( "InitFromFile: %s has more than %i triangles on a surface (%i)",
				m_fileName.c_str(), 
                                SHADER_MAX_INDEXES / 3, surf->numTriangles );
		}
	
		// change to surface identifier
		surf->ident = 0;	//SF_MD3;

		// lowercase the surface name so skin compares are faster
		int slen = (int)strlen( surf->name );
		for( j = 0; j < slen; j++ ) {
			surf->name[j] = tolower( surf->name[j] );
		}

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

        // register the shaders
        shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
        for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
            /*
            if ( !shader->shader.Init( shader->name ) ) {
                msg_failure( "shader with name `%s' failed to initialize\n",
                       shader->name );
            }
            */
            printf( "=`%s'\n", shader->name );
        }

		// swap all the triangles
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
                gsurf.m_indices.resize( surf->numTriangles * 3 );
		for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);

                        gsurf.m_indices[j*3] = tri->indexes[0];
                        gsurf.m_indices[j*3+1] = tri->indexes[1];
                        gsurf.m_indices[j*3+2] = tri->indexes[2];
		}

            gsurf.m_verts.resize( surf->numVerts );

		// swap all the ST
        st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
        for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
            st->st[0] = LittleFloat( st->st[0] );
            st->st[1] = LittleFloat( st->st[1] );
        }

		// swap all the XyzNormals
        xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
        for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ ) 
	{
            xyz->xyz[0] = LittleShort( xyz->xyz[0] );
            xyz->xyz[1] = LittleShort( xyz->xyz[1] );
            xyz->xyz[2] = LittleShort( xyz->xyz[2] );

            xyz->normal = LittleShort( xyz->normal );
        }

        st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
        xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
        for ( j = 0 ; j < surf->numVerts ; j++, xyz++, st++ )  {
            gsurf.m_verts[j].m_pos[0] = (float)xyz->xyz[0] / 64.f;
            gsurf.m_verts[j].m_pos[1] = (float)xyz->xyz[1] / 64.f;
            gsurf.m_verts[j].m_pos[2] = (float)xyz->xyz[2] / 64.f;

            gsurf.m_verts[j].m_tex[0] = st->st[0];
            gsurf.m_verts[j].m_tex[1] = st->st[1];

            /*
            fprintf(stderr, "=%f, %f, %f\n",
                gsurf.m_verts[j].m_pos[0],
                gsurf.m_verts[j].m_pos[1],
                gsurf.m_verts[j].m_pos[2] );
                */

            float lat, lon;
            lat = ( xyz->normal >> 8 )  * 2 * idMath::PI / 255.f;
            lon = ( xyz->normal & 255 ) * 2 * idMath::PI / 255.f;

            gsurf.m_verts[j].m_normal[0] = idMath::Sin( lat ) * idMath::Cos( lon );
            gsurf.m_verts[j].m_normal[0] = idMath::Sin( lat ) * idMath::Sin( lon );
            gsurf.m_verts[j].m_normal[2] = idMath::Cos( lat );
        }



		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}


        gl_render.CacheSurface( gsurf, m_surf );
}

void GLRenderModelMD3::Think( int ms ) {
}

void GLRenderModelMD3::Render( void ) {
    glsurf_t surf;

    surf.m_modelMatrix = idMat4( m_axis, m_pos );
    surf.m_surf = m_surf;

    gl_render.AddSurface( surf );
}
