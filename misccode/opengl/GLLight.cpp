#include "GLLight.h"

static void *PackHelper( byte *data, float v );
static void *PackHelper( byte *data, const idVec3 &vec );
static void *PackHelper( byte *data, const idVec4 &vec );

/*
static void *PackHelper( byte *data, const idMat3 &mat );
static void *PackHelper( byte *data, const idMat4 &mat );
*/

void GLDirectionLight::Pack( byte *buf, const std::vector<GLint> &offsets ) const {
    PackHelper( buf + offsets[0], m_dir );
}

void GLPointLight::Pack( byte *buf, const std::vector<GLint> &offsets ) const {
    PackHelper( buf + offsets[0], m_pos );
    PackHelper( buf + offsets[1], m_range );
    PackHelper( buf + offsets[2], m_atten );
}

void GLSpotLight::Pack( byte *buf, const std::vector<GLint> &offsets ) const {
    PackHelper( buf + offsets[0], m_pos );
    PackHelper( buf + offsets[1], m_dir );
    PackHelper( buf + offsets[2], m_range );
    PackHelper( buf + offsets[3], m_atten );
}

static void *PackHelper( byte *data, GLint v ) {
    GLint *p = (GLint *)data; *p++ = v;
    return p;
}

static void *PackHelper( byte *data, float v ) {
    float *p = (float *)data; *p++ = v;
    return p;
}

static void *PackHelper( byte *data, const idVec3 &vec ) {
    float *p = (float *)data; *p++ = vec[0]; *p++ = vec[1]; *p++ = vec[2];
    return p;
}

static void *PackHelper( byte *data, const idVec4 &vec ) {
    float *p = (float *)data; 
    *p++ = vec[0]; *p++ = vec[1]; *p++ = vec[2]; *p++ = vec[3];
    return p;
}
