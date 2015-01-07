#ifndef MATERIAL__H_
#define MATERIAL__H_

#include "d3lib/Lib.h"

#include "ObjectCache.h"
#include "GLSLProgram.h"

#include <boost/shared_ptr.hpp>

struct CommonMaterialParams {
    idMat4 m_mvpMatrix;
    idMat4 m_modelMatrix;
    idVec3 m_eye;
    idVec3 m_lightDir;
    float  m_time;
};

class MaterialBase {
public:
    virtual ~MaterialBase() {}
    virtual bool Init( const std::string &name ) = 0;
    virtual void Bind( const CommonMaterialParams &params );
    virtual void Unbind( void );

protected:
    boost::shared_ptr<GLSLProgram> m_program;
};

class MaterialCache {
public:
    boost::shared_ptr<MaterialBase> Get( const std::string &name ) {
        if ( m_cache.find( name ) == m_cache.end() ) {
            boost::shared_ptr<MaterialBase> v = Setup( name );
            if ( v ) {
                m_cache.insert( std::make_pair( name, v ) );
                return v;
            }
            return boost::shared_ptr<MaterialBase>();
        }
        return m_cache[ name ];
    }
    boost::shared_ptr<MaterialBase> Setup( const std::string &name );

private:
    std::map<std::string, boost::shared_ptr<MaterialBase> > m_cache;
};

extern MaterialCache glMaterialCache;

#endif
