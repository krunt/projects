#ifndef TEXTURE_BASE__H_
#define TEXTURE_BASE__H_

class TextureBase {
public:
    TextureBase() {}
    virtual ~TextureBase() {}

    virtual bool Init( const std::string &name ) = 0;
    virtual bool Init( byte *data, int width, int height, int format ) = 0;

    virtual void Bind( int unit ) = 0;
    virtual void Unbind( void ) = 0;
};

#endif /* TEXTURE_BASE__H_ */
