#ifndef COMMON__H_
#define COMMON__H_

#include "d3lib/Lib.h"

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>

class GLTexture;
struct material_t {
    GLTexture *m_matPtr;
};

struct __attribute__((packed)) drawVert_t {
    drawVert_t() {}

    drawVert_t( const idVec3 &p, const idVec2 &t )
    {
        VectorCopy( p, m_pos );

        m_tex[0] = t[0];
        m_tex[1] = t[1];
    }

    drawVert_t( const idVec3 &p, const idVec2 &t, const idVec3 &normal )
    {
        VectorCopy( p, m_pos );

        m_tex[0] = t[0];
        m_tex[1] = t[1];

        VectorCopy( normal, m_normal );
    }

    float m_pos[3];
    float m_tex[2];
    float m_normal[3];
};

struct surf_t {
    std::vector<drawVert_t> m_verts;
    std::vector<GLushort> m_indices;
    std::string m_matName;
};

struct cached_surf_t {
    GLuint m_vao;
    int m_numIndices;
    int m_indexBuffer;
    material_t m_material;
};

struct glsurf_t {
    idMat4 m_modelMatrix;
    cached_surf_t m_surf;
};

struct playerView_t {
    idVec3 m_pos;
    idMat3 m_axis;

    float m_fovx, m_fovy;

    float m_znear, m_zfar;
    int m_width, m_height;
};

#endif /* COMMON__H_ */
