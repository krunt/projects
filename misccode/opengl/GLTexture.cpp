
#include <stdio.h>
#include <fstream>

#include "GLTexture.h"
#include "Utils.h"
#include "d3lib/Lib.h"

void R_VerticalFlip( byte *data, int width, int height ) {
	int		i, j;
	int		temp;

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height / 2 ; j++ ) {
			temp = *( (int *)data + j * width + i );
			*( (int *)data + j * width + i ) = *( (int *)data + ( height - 1 - j ) * width + i );
			*( (int *)data + ( height - 1 - j ) * width + i ) = temp;
		}
	}
}

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

static byte *R_LoadTGA( const std::string &filename, int &width, 
        int &height, int &format ) {
	int		columns, rows, numPixels, fileSize, numBytes;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	TargaHeader	targa_header;
	byte		*targa_rgba;
    byte    *pic;
    std::string contents;

    format = GL_RGBA;

	pic = NULL;

    BloatFile( filename, contents );

    fileSize = contents.size();

    if ( !fileSize ) {
		msg_warning( "R_LoadTGA( %s ): File not found\n", filename.c_str() );
        return NULL;
    }

    buffer = (byte *)contents.data();

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if ( targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3 ) {
		msg_warning( "R_LoadTGA( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n", filename.c_str() );
        return NULL;
	}

	if ( targa_header.colormap_type != 0 ) {
		msg_warning( "R_LoadTGA( %s ): colormaps not supported\n", 
                filename.c_str() );
        return NULL;
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 ) {
		msg_warning( "R_LoadTGA( %s ): Only 32 or 24 bit images supported (no colormaps)\n", 
                filename.c_str() );
        return NULL;
	}

	if ( targa_header.image_type == 2 || targa_header.image_type == 3 ) {
		numBytes = targa_header.width * targa_header.height * ( targa_header.pixel_size >> 3 );
		if ( numBytes > fileSize - 18 - targa_header.id_length ) {
			msg_warning( "R_LoadTGA( %s ): incomplete file\n", filename.c_str() );
            return NULL;
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	width = columns;
	height = rows;

	targa_rgba = (byte *)malloc(numPixels*4);
    memset( targa_rgba, 0, numPixels*4 );
	pic = targa_rgba;

	if ( targa_header.id_length != 0 ) {
		buf_p += targa_header.id_length;  // skip TARGA image comment
	}
	
	if ( targa_header.image_type == 2 || targa_header.image_type == 3 )
	{ 
		// Uncompressed RGB or gray scale image
		for( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row*columns*4;
			for( column = 0; column < columns; column++)
			{
				unsigned char red,green,blue,alphabyte;
				switch( targa_header.pixel_size )
				{
					
				case 8:
					blue = *buf_p++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					msg_warning( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                            filename.c_str(), targa_header.pixel_size );
                    return NULL;
				}
			}
		}
	}
	else if ( targa_header.image_type == 10 ) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for( row = rows - 1; row >= 0; row-- ) {
			pixbuf = targa_rgba + row*columns*4;
			for( column = 0; column < columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if ( packetHeader & 0x80 ) {        // run-length packet
					switch( targa_header.pixel_size ) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
						default:
							msg_warning( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                                    filename.c_str(), targa_header.pixel_size );
                            return NULL;
					}
	
					for( j = 0; j < packetSize; j++ ) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if ( column == columns ) { // run spans across rows
							column = 0;
							if ( row > 0) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for( j = 0; j < packetSize; j++ ) {
						switch( targa_header.pixel_size ) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
							default:
								msg_warning( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                                        filename.c_str(), targa_header.pixel_size );
                                return NULL;
						}
						column++;
						if ( column == columns ) { // pixel packet run spans across rows
							column = 0;
							if ( row > 0 ) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut: ;
		}
	}

	if ( (targa_header.attributes & (1<<5)) ) {			// image flp bit
		R_VerticalFlip( pic, width, height );
	}

    return pic;
}

#define	MAX_DIMENSION	4096
void R_ResampleTexture( const byte *in, byte *out, int inwidth, int inheight,  
							int outwidth, int outheight ) {
	int		i, j;
	const byte	*inrow, *inrow2;
	unsigned int	frac, fracstep;
	unsigned int	p1[MAX_DIMENSION], p2[MAX_DIMENSION];
	const byte		*pix1, *pix2, *pix3, *pix4;
	byte		*out_p;

	if ( outwidth > MAX_DIMENSION ) {
		outwidth = MAX_DIMENSION;
	}
	if ( outheight > MAX_DIMENSION ) {
		outheight = MAX_DIMENSION;
	}

	out_p = out;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out_p += outwidth*4 ) {
		inrow = in + 4 * inwidth * (int)( ( i + 0.25f ) * inheight / outheight );
		inrow2 = in + 4 * inwidth * (int)( ( i + 0.75f ) * inheight / outheight );
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++) {
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			out_p[j*4+0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			out_p[j*4+1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			out_p[j*4+2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			out_p[j*4+3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

void LoadPCX (const char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;
    std::string contents;

	*pic = NULL;

    BloatFile( filename, contents );

    if ( contents.empty() ) {
		msg_failure ("no pcx found %s\n", filename);
        return;
    }

    len = contents.size();
    raw = (byte *)&contents[0];

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

    pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		msg_failure ("Bad pcx file %s\n", filename);
		return;
	}

	out = (byte *)malloc ( (pcx->ymax+1) * (pcx->xmax+1) );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = (byte*)malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		msg_failure("PCX file %s was malformed", filename);
		free (*pic);
		*pic = NULL;
	}
}

const int kStepSize = 8;
typedef unsigned char byte;

static int getmin( int v1, int v2 ) {
    return v1 < v2 ? v1 : v2;
}

static int getmax( int v1, int v2 ) {
    return v1 < v2 ? v2 : v1;
}

static int getsquare( int v ) {
    return v * v;
}

static int inrange(int val, int from, int to) {
    return val >= from && val < to;
}

void find_nearby_colors( int sr, int sg, int sb,
        byte *palette, byte candidates[256])
{
    int mindist2_mas[256];
    int minmaxdist2 = 1<<30;

    memset(candidates, 0, 256);

    int mr = sr + kStepSize / 2;
    int mg = sg + kStepSize / 2;
    int mb = sb + kStepSize / 2;

    for ( int i = 0; i < 256; ++i ) {
        int r, g, b;

        r = palette[i*3+0];
        g = palette[i*3+1];
        b = palette[i*3+2];

        int v = 0;
        if ( inrange(r, sr, sr+kStepSize) ) {
            v |= 1;
        }

        if ( inrange(g, sg, sg+kStepSize) ) {
            v |= 2;
        }

        if ( inrange(b, sb, sb+kStepSize) ) {
            v |= 4;
        }

        int mindist2 = 0;
        switch ( v ) {
        case 7:
            mindist2 = 0;
            break;

        case 6:
            mindist2 = getsquare(sr - r);
            break;

        case 5:
            mindist2 = getsquare(sg - g);
            break;

        case 4:
            mindist2 = getsquare(getmin(abs(sg - g), abs(sr - r)));
            break;

        case 3:
            mindist2 = getsquare(sb - b);
            break;

        case 2:
            mindist2 = getsquare(getmin(abs(sb - b), abs(sr - r)));
            break;

        case 1:
            mindist2 = getsquare(getmin(abs(sb - b), abs(sg - g)));
            break;

        /* point case next */
        default:
            break;
        };

        int sign[3] = {0};

        sign[0] = b >= mb ? 1 : -1; //blue
        sign[1] = r >= mr ? 1 : -1; //red
        sign[2] = ( g >= mg ) ^ ( r < mr ) ? 1 : -1; //green

        if ( !v ) {
            int near_r = mr + sign[1] * kStepSize / 2;
            int near_g = mg + sign[2] * kStepSize / 2;
            int near_b = mb + sign[0] * kStepSize / 2;

            mindist2 = (near_r - r) * (near_r - r)
                + (near_g - g) * (near_g - g)
                + (near_b - b) * (near_b - b);
        }

        mindist2_mas[i] = mindist2;

        int far_r = mr + -sign[1] * kStepSize / 2;
        int far_g = mg + -sign[2] * kStepSize / 2;
        int far_b = mb + -sign[0] * kStepSize / 2;

        int maxdist2 = (far_r - r) * (far_r - r)
                + (far_g - g) * (far_g - g)
                + (far_b - b) * (far_b - b);

        if ( maxdist2 < minmaxdist2 ) {
            minmaxdist2 = maxdist2;
        }
    }

    for ( int i = 0; i < 256; ++i ) {
        if ( mindist2_mas[i] <= minmaxdist2 ) {
            candidates[i] = mindist2_mas[i] <= minmaxdist2;
        }
    }
}


int find_best_colors( int sr, int sg, int sb,
        byte *image, byte *palette, byte candidates[256] )
{
    for ( int i = 0; i < kStepSize; ++i ) {
        for ( int j = 0; j < kStepSize; ++j ) {
            for ( int k = 0; k < kStepSize; ++k ) {
                int mindist2 = 1<<30;
                int minindex = -1;
                for ( int t = 0; t < 256; ++t ) {
                    /*
                    if ( !candidates[t] ) {
                        continue;
                    }
                    */

                    int r, g, b;

                    r = palette[t*3+0];
                    g = palette[t*3+1];
                    b = palette[t*3+2];

                    int mdist = (sr + i - r) * (sr + i - r)
                        + (sg + j - g) * (sg + j - g)
                        + (sb + k - b) * (sb + k - b);

                    if ( mindist2 > mdist ) {
                        mindist2 = mdist;
                        minindex = t;
                    }
                }

                assert(minindex != -1);
                image[((sr+i)<<16) + ((sg+j)<<8) + (sb+k)] = (byte)minindex;
            }
        }
    }
}

void convert_image(byte *image, int width, int height, int depth, 
        byte *palette) 
{
    for ( int i = 0; i < width; i += kStepSize ) {
        for ( int j = 0; j < height; j += kStepSize ) {
            for ( int k = 0; k < depth; k += kStepSize ) {
                byte candidates[256];
                //find_nearby_colors(i, j, k, palette, candidates);
                find_best_colors(i, j, k, image, palette, candidates);
            }
        }
    }
}

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[32];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
	char		animname[32];			// next frame in animation chain
	int			flags;
	int			contents;
	int			value;
} miptex_t;

byte *d_24to8table = NULL;
bool ConvertTgaToWal( const std::string &tganame, const std::string &walname )
{
    if (!d_24to8table) {
        d_24to8table = (byte*)malloc(1<<24);
        memset(d_24to8table, 0, 1<<24);

        byte *colormap, *pal;
	    LoadPCX ("pics/colormap.pcx", &colormap, &pal, NULL, NULL);

        convert_image(d_24to8table, 256, 256, 256, (byte*)pal);

        free(colormap);
        free(pal);
    }

    byte *pic, *picCopy, *outPic;
    int width, height, format;
    pic = R_LoadTGA( tganame, width, height, format );
    if (!pic) {
        return false;
    }

    picCopy = (byte *)malloc( width * height * 4 );
    outPic = (byte *)malloc(width * height);

    miptex_t miptex = {0};
    memcpy( miptex.name, tganame.c_str(), tganame.size() );
    miptex.width = width;
    miptex.height = height;
    miptex.offsets[0] = sizeof(miptex_t);

    std::fstream fs( walname.c_str(),
            std::ios_base::out | std::ios_base::binary );

    fs.write( (const char *)&miptex, sizeof(miptex) );

    for (int j = 0; j < 4; ++j ) {
        int oldWidth, oldHeight;

        oldWidth = width;
        oldHeight = height;

        if ( width > 1 ) {
            width >>= 1;
        }

        if ( height > 1 ) {
            height >>= 1;
        }

        /* convert */
        for (int k = 0; k < oldWidth * oldHeight; ++k ) {
            outPic[k] = d_24to8table[ pic[4*k] << 16 | pic[4*k+1] << 8 | pic[4*k+2] ];
        }
        fs.write( (const char *)outPic, oldWidth * oldHeight );

        R_ResampleTexture( pic, picCopy, oldWidth, oldHeight, width, height );

        std::swap( pic, picCopy );
    }

    free(picCopy);
    free(pic);
    free(outPic);
}


GLTexture::~GLTexture() {
    if ( IsOk() ) {
        glDeleteTextures( 1, &m_texture );
    }
}

bool GLTexture::Init( void ) {
    GLuint texture;

    _CH(glActiveTexture( GL_TEXTURE0 ));
    _CH(glGenTextures( 1, &texture ));
    _CH(glBindTexture( GL_TEXTURE_2D, texture ));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    _CH(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    m_texture = texture;
    m_loadOk = true;

    return true;
}

bool GLTexture::Init( const std::string &name ) {
    byte *pic, *picCopy;
    GLuint texture;
    int width, height, format;

    format = GL_RGBA;

    if ( EndsWith( name, ".tga" ) ) {
        pic = R_LoadTGA( name, width, height, format );
        if ( !pic ) {
            return false;
        }
    } else {
        fprintf( stderr, "no image file with name `%s' found\n",
            name.c_str() );
        return false;
    }

    if ( !IsPowerOf2( width ) || !IsPowerOf2( height ) ) {
        fprintf( stderr, "texture must have size in power of two\n" );
        free( pic );
        return false;
    }

    picCopy = (byte *)malloc( width * height * 4 );

    _CH(glActiveTexture( GL_TEXTURE0 ));
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
            int format ) 
{
    GLuint texture;
    byte *pic, *picCopy;

    _CH(glActiveTexture( GL_TEXTURE0 ));
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

void GLTexture::Bind( int unit ) {
    assert( IsOk() );
    _CH(glActiveTexture( GL_TEXTURE0 + unit ));
    _CH(glBindTexture( GL_TEXTURE_2D, m_texture ));
}

void GLTexture::Unbind( void ) {
    /*
    _CH(glActiveTexture( GL_TEXTURE0 + m_textureUnit ));
    _CH(glBindTexture( GL_TEXTURE_2D, 0 ));
    */
}

bool GLTextureCube::Init( const std::string &name ) {
    int i;
    byte *pic, *picCopy;
    GLuint texture;
    int width, height, format;

    format = GL_RGBA;

    _CH(glActiveTexture( GL_TEXTURE0 ));
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
            if ( !pic ) {
                return false;
            }
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

bool GLTextureCube::Init( byte *data, int width, int height, int format ) {
    return true;
}

void GLTextureCube::Bind( int unit ) {
    assert( IsOk() );
    _CH(glActiveTexture( GL_TEXTURE0 + unit ));
    _CH(glBindTexture( GL_TEXTURE_CUBE_MAP, m_texture ));
}

void GLTextureCube::Unbind( void ) {
    /*
    _CH(glActiveTexture( GL_TEXTURE0 + 0 ));
    _CH(glBindTexture( GL_TEXTURE_CUBE_MAP, 0 ));
    */
}

