
#include "d3lib/Lib.h"
#include "Model_md3.h"

#include "Render.h"

#include "Utils.h"

#define	LL(x) x=LittleLong(x)

void GLRenderModelMD3::Spawn( void ) {
    m_pos = vec3_origin;
    m_axis = mat3_identity;
    m_isAnimPlaying = false;

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
        std::string &buffer = m_md3Body;
        surf_t gsurf;

        gsurf.m_matName = m_textureName;

        BloatFile( m_fileName, buffer );

	pinmodel = (md3Header_t *)buffer.data();
        m_md3pointer = pinmodel;

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

        fprintf( stderr, "frames=%d, surfaces=%d, tags=%d\n", 
                md3->numFrames, md3->numSurfaces, md3->numTags );

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
            gsurf.m_verts[j].m_normal[1] = idMath::Sin( lat ) * idMath::Sin( lon );
            gsurf.m_verts[j].m_normal[2] = idMath::Cos( lat );
        }



		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

        gl_render.CacheSurface( gsurf, m_surf );

}

void GLRenderModelMD3::Think( int ms ) {
    if ( m_isAnimPlaying ) {
        m_curMs += ms;
    }
}

static md3Tag_t *R_GetTag( md3Header_t *mod, int frame ) {
	md3Tag_t		*tag;
	int				i;

	if ( frame >= mod->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
            return tag;
	}

	return NULL;
}

void GLRenderModelMD3::LerpTags( int startFrame, int endFrame, float backLerp ) {
    md3Tag_t *start = R_GetTag( m_md3pointer, startFrame );
    md3Tag_t *end = R_GetTag( m_md3pointer, endFrame );

    if ( !start || !end ) {
        m_isAnimPlaying = false;
        return;
    }

    float frontLerp = 1 - backLerp;

    for ( int i = 0 ; i < 3 ; i++ ) {
	m_pos[i] = start->origin[i] * backLerp +  end->origin[i] * frontLerp;
	m_axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frontLerp;
	//m_axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frontLerp;
	//m_axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frontLerp;
    }

    m_axis[0].Normalize();
    m_axis[0].OrthogonalBasis( m_axis[1], m_axis[2] );
}

void GLRenderModelMD3::LerpMeshVertexes( int startFrame, int endFrame, float backLerp ) {
    int vertNum, numVerts;
    short *oldXyz, *newXyz;
    float oldXyzScale, newXyzScale;
    md3Header_t *md3 = m_md3pointer;
    md3XyzNormal_t *xyz;
    md3Surface_t *surf;
    md3Triangle_t *tri;
    md3St_t *st;

    surf_t gsurf;

    gl_render.UncacheSurface( m_surf );

    newXyzScale = MD3_XYZ_SCALE * ( 1.0 - backLerp );
    oldXyzScale = MD3_XYZ_SCALE * backLerp;

    surf = (md3Surface_t *) ( (byte *)md3 + md3->ofsSurfaces );

    for ( int i = 0; i < md3->numSurfaces; ++i ) {

        oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
            + ( startFrame * surf->numVerts * 4 );
        newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
            + ( endFrame * surf->numVerts * 4 );
        xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );

        st = (md3St_t *) ( (byte *)surf + surf->ofsSt );

        numVerts = surf->numVerts;

        gsurf.m_matName = m_textureName;
        gsurf.m_indices.resize( surf->numTriangles * 3 );
        gsurf.m_verts.resize( surf->numVerts );
        for ( vertNum = 0; vertNum < numVerts; ++vertNum, oldXyz += 4, newXyz += 4, ++st, ++xyz ) {
            gsurf.m_verts[vertNum].m_pos[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
            gsurf.m_verts[vertNum].m_pos[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
            gsurf.m_verts[vertNum].m_pos[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

            gsurf.m_verts[vertNum].m_tex[0] = st->st[0];
            gsurf.m_verts[vertNum].m_tex[1] = st->st[1];

            float lat, lon;
            lat = ( xyz->normal >> 8 )  * 2 * idMath::PI / 255.f;
            lon = ( xyz->normal & 255 ) * 2 * idMath::PI / 255.f;

            gsurf.m_verts[vertNum].m_normal[0] = idMath::Sin( lat ) * idMath::Cos( lon );
            gsurf.m_verts[vertNum].m_normal[1] = idMath::Sin( lat ) * idMath::Sin( lon );
            gsurf.m_verts[vertNum].m_normal[2] = idMath::Cos( lat );
        }

        /*
	tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
        for ( int j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
            gsurf.m_indices[j*3] = tri->indexes[0];
            gsurf.m_indices[j*3+1] = tri->indexes[1];
            gsurf.m_indices[j*3+2] = tri->indexes[2];
	    }
        */

	    int *triangles = (int *) ( (byte *)surf + surf->ofsTriangles );
        for ( int j = 0; j < surf->numTriangles * 3; ++j ) {
            gsurf.m_indices[j] = triangles[j];
        }

	surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
    }

    gl_render.CacheSurface( gsurf, m_surf );
}

void GLRenderModelMD3::LerpMesh() {
    if ( m_curMs > m_endMs ) {
        m_isAnimPlaying = false;
        return;
    }

    float frame = ( m_curMs - m_startMs ) / 1000.0f * m_currAnim.m_fps;
    int frame1 = (int)frame;
    int frame2 = frame1 + 1;

    float backLerp = frame - (float)frame1;

    LerpTags( frame1, frame2, backLerp );
    LerpMeshVertexes( frame1, frame2, backLerp );
}

void GLRenderModelMD3::Render( void ) {
    glsurf_t surf;

    if ( m_isAnimPlaying ) {
        LerpMesh();
    }

    surf.m_modelMatrix = idMat4( m_axis, m_pos );
    surf.m_surf = m_surf;

    gl_render.AddSurface( surf );
}

void GLRenderModelMD3::RegisterAnim( animNumber_t anim, int startFrame, int endFrame, int fps ) {
    animStruct t;
    t.m_anim = anim;
    t.m_startFrame = startFrame;
    t.m_endFrame = endFrame;
    t.m_fps = fps;
    m_anims.push_back( t );
}

void GLRenderModelMD3::PlayAnim( animNumber_t anim ) {
    int i;
    for ( i = 0; i < m_anims.size(); ++i ) {
        if ( m_anims[i].m_anim == anim ) {
            m_currAnim = m_anims[i];
            break;
        }
    }

    assert( i != m_anims.size() );

    m_startMs = 0;
    m_curMs = 0;
    m_endMs = 1000.0f * ( m_currAnim.m_endFrame - m_currAnim.m_startFrame ) / (float)m_currAnim.m_fps;

    m_isAnimPlaying = true;
}
