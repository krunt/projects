#ifndef LIGHT__H_
#define LIGHT__H_

#include "d3lib/Lib.h"

#include "Common.h"

class GLLight {
public:
    GLLight( const std::string &name );
    virtual void Pack( byte *buf, const std::vector<GLint> &offsets ) const = 0;

    std::string GetName() const { return m_name; }

private:
    std::string m_name;
};

class GLDirectionLight : public GLLight {
public:
    GLDirectionLight( const idVec3 &dir );
    virtual void Pack( byte *buf, const std::vector<GLint> &offsets ) const;

private:
    idVec3 m_dir;
};

class GLPointLight : public GLLight {
public:
    GLPointLight( const idVec3 &dir );
    virtual void Pack( byte *buf, const std::vector<GLint> &offsets ) const;

private:
    idVec3 m_pos;
    float  m_range;
    float  m_atten;
};

class GLSpotLight : public GLLight {
public:
    GLSpotLight( const idVec3 &dir );
    virtual void Pack( byte *buf, const std::vector<GLint> &offsets ) const;

private:
    idVec3 m_pos;
    idVec3  m_dir;
    float  m_range;
    float  m_atten;
};

#endif /* LIGHT__H_ */
