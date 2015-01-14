

GLTexture::~GLTexture() {
    if ( IsOk() ) {
        glDeleteTextures( 1, &m_texture );
    }
}

bool GLTexture::Init( const std::string &name, int textureUnit ) {
    byte *pic, *picCopy;
    GLuint texture;
    int width, height, format;

    format = GL_RGBA;

    if ( EndsWith( name, ".tga" ) ) {
        pic = R_LoadTGA( name, width, height, format );
    } else {
        msg_warning( "no image file with name `%s' found\n", name.c_str() );
        return false;
    }

    if ( !IsPowerOf2( width ) || !IsPowerOf2( height ) ) {
        msg_warning0( "texture must have size in power of two\n" );
        return false;
    }

    picCopy = (byte *)malloc( width * height * 4 );

    _CH(glActiveTexture( GL_TEXTURE0 + textureUnit ));
    _CH(glGenTextures( 1, &texture ));
    _CH(glBindTexture( GL_TEXTURE_2D, texture ));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    int mipmap = 0;
    while ( width > 1 || height > 1 ) {
        _CH(glTexImage2D( GL_TEXTURE_2D,
            mipmap,
            format,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            pic));

        int oldWidth, oldHeight;

        oldWidth = width;
        oldHeight = height;

        if ( width > 1 ) {
            width >>= 1;
        }

        if ( height > 1 ) {
            height >>= 1;
        }

        R_ResampleTexture( pic, picCopy, oldWidth, oldHeight, width, height );

        std::swap( pic, picCopy );

        mipmap += 1;
    }

    free( pic );
    free( picCopy );

    m_texture = texture;
    m_loadOk = true;
    return true;
}

bool GLTexture::Init( byte *data, int width, int height, 
            int format, int textureUnit ) 
{
    GLuint texture;
    byte *pic, *picCopy;

    _CH(glActiveTexture( GL_TEXTURE0 + textureUnit ));
    _CH(glGenTextures( 1, &texture ));
    _CH(glBindTexture( GL_TEXTURE_2D, texture ));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    pic = data;
    picCopy = (byte *)malloc( width * height * 4 );

    int mipmap = 0;
    while ( width > 1 || height > 1 ) {
        _CH(glTexImage2D( GL_TEXTURE_2D,
            mipmap,
            format,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            pic));

        int oldWidth, oldHeight;

        oldWidth = width;
        oldHeight = height;

        if ( width > 1 ) {
            width >>= 1;
        }

        if ( height > 1 ) {
            height >>= 1;
        }

        R_ResampleTexture( pic, picCopy, oldWidth, oldHeight, width, height );

        std::swap( pic, picCopy );

        mipmap += 1;
    }

    if ( pic != data ) {
        free( pic );
    }

    if ( picCopy != data ) {
        free( picCopy );
    }

    m_texture = texture;
    m_loadOk = true;
    return true;
}

void GLTexture::Bind( void ) {
    assert( IsOk() );
    _CH(glActiveTexture( GL_TEXTURE0 + 1 ));
    _CH(glBindTexture( GL_TEXTURE_2D, m_texture ));
}

void GLTexture::Unbind( void ) {
    _CH(glActiveTexture( GL_TEXTURE0 + 1 ));
    _CH(glBindTexture( GL_TEXTURE_2D, 0 ));
}

bool GLTextureCube::Init( const std::string &name, int textureUnit ) {
    int i;
    byte *pic, *picCopy;
    GLuint texture;
    int width, height, format;

    format = GL_RGBA;

    _CH(glActiveTexture( GL_TEXTURE0 + textureUnit ));
    _CH(glGenTextures( 1, &texture ));
    _CH(glBindTexture( GL_TEXTURE_CUBE_MAP, texture ));
    _CH(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    _CH(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    int bindTarget[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };

    const char *texSuffixes[6] = {
        "_left.tga", "_right.tga",
        "_up.tga", "_down.tga", 
        "_back.tga", "_forward.tga",
    };

    for ( i = 0; i < 6; ++i ) {
        std::string imgName = name + texSuffixes[i];

        if ( EndsWith( imgName, ".tga" ) ) {
            pic = R_LoadTGA( imgName, width, height, format );
        } else {
            fprintf( stderr, "no image file with name `%s' found\n",
                name.c_str() );
            return false;
        }
    
        if ( !IsPowerOf2( width ) || !IsPowerOf2( height ) ) {
            fprintf( stderr, "texture must have size in power of two\n" );
            return false;
        }
    
        picCopy = (byte *)malloc( width * height * 4 );
    
        int mipmap = 0;
        while ( width > 1 || height > 1 ) {
            _CH(glTexImage2D( 
                bindTarget[i],
                mipmap,
                format,
                width,
                height,
                0,
                format,
                GL_UNSIGNED_BYTE,
                pic));
    
            int oldWidth, oldHeight;
    
            oldWidth = width;
            oldHeight = height;
    
            if ( width > 1 ) {
                width >>= 1;
            }
    
            if ( height > 1 ) {
                height >>= 1;
            }
    
            R_ResampleTexture( pic, picCopy, oldWidth, oldHeight, width, height );
    
            std::swap( pic, picCopy );
    
            mipmap += 1;
        }
    
        free( pic );
        free( picCopy );
    }

    m_texture = texture;
    m_loadOk = true;
    return true;
}

void GLTextureCube::Bind( void ) {
    assert( IsOk() );
    _CH(glActiveTexture( GL_TEXTURE0 + 1 ));
    _CH(glBindTexture( GL_TEXTURE_CUBE_MAP, m_texture ));
}

void GLTextureCube::Unbind( void ) {
    _CH(glActiveTexture( GL_TEXTURE0 + 1 ));
    _CH(glBindTexture( GL_TEXTURE_CUBE_MAP, 0 ));
}
