#ifndef TEXTURE_BASE__H_
#define TEXTURE_BASE__H_

#include "d3lib/Lib.h"

#include <boost/shared_ptr.hpp>

class TextureBase {
public:
    TextureBase() {}
    virtual ~TextureBase() {}

    virtual bool Init( void ) = 0;
    virtual bool Init( const std::string &name ) = 0;
    virtual bool Init( byte *data, int width, int height, int format ) = 0;

    virtual void Bind( int unit ) = 0;
    virtual void Unbind( void ) = 0;
};

class TextureCache {
public:
    boost::shared_ptr<TextureBase> Get( const std::string &name ) {
        if ( m_cache.find( name ) == m_cache.end() ) {
            boost::shared_ptr<TextureBase> v = Setup( name );
            if ( v ) {
                m_cache.insert( std::make_pair( name, v ) );
            }
        }
        return m_cache[ name ];
    }

    boost::shared_ptr<TextureBase> Setup( const std::string &name );

private:
    std::map<std::string, boost::shared_ptr<TextureBase> > m_cache;
};

extern TextureCache glTextureCache;

#endif /* TEXTURE_BASE__H_ */
