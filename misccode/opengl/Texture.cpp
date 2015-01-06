#include "Texture.h"
#include "GLTexture.h"

boost::shared_ptr<TextureBase> TextureCache::Setup( const std::string &name ) {
    if ( name == "white" ) {

        byte white[16] = 
        { 0xFF, 0x00, 0xFF, 0xFF,
          0xFF, 0x00, 0x00, 0xFF,
          0x00, 0xFF, 0x00, 0xFF,
          0x00, 0x00, 0xFF, 0xFF,
        };

        boost::shared_ptr<TextureBase> res( new GLTexture() );

        if ( !res->Init( white, 2, 2, GL_RGBA ) ) {
            return boost::shared_ptr<TextureBase>();
        }

        return res;

    } else if ( name == "sky" ) {

        boost::shared_ptr<TextureBase> res( new GLTextureCube() );

        if ( !res->Init( "images/cloudy" ) ) {
            return boost::shared_ptr<TextureBase>();
        }

        return res;

    } else if ( name == "postprocess" ) {

        boost::shared_ptr<TextureBase> res( new GLTexture() );

        if ( !res->Init() ) {
            return boost::shared_ptr<TextureBase>();
        }

        return res;
    }


    boost::shared_ptr<TextureBase> res( new GLTexture() );

    if ( !res->Init( name ) ) {
        return boost::shared_ptr<TextureBase>();
    }

    return res;
}

TextureCache glTextureCache;
