#include "tga.h"

static void R_VerticalFlip( byte *data, int width, int height ) {
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

#define	MAX_DIMENSION	4096
static void R_ResampleTexture( const byte *in, 
        byte *out, int inwidth, int inheight,  int outwidth, int outheight ) {
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

byte *R_LoadTGA( const std::string &filename, int &width, 
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
		msg_failure( "R_LoadTGA( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n", filename.c_str() );
	}

	if ( targa_header.colormap_type != 0 ) {
		msg_failure( "R_LoadTGA( %s ): colormaps not supported\n", 
                filename.c_str() );
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 ) {
		msg_failure( "R_LoadTGA( %s ): Only 32 or 24 bit images supported (no colormaps)\n", 
                filename.c_str() );
	}

	if ( targa_header.image_type == 2 || targa_header.image_type == 3 ) {
		numBytes = targa_header.width * targa_header.height * ( targa_header.pixel_size >> 3 );
		if ( numBytes > fileSize - 18 - targa_header.id_length ) {
			msg_failure( "R_LoadTGA( %s ): incomplete file\n", filename.c_str() );
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
					msg_failure( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                            filename.c_str(), targa_header.pixel_size );
					break;
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
							msg_failure( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                                    filename.c_str(), targa_header.pixel_size );
							break;
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
								msg_failure( "LoadTGA( %s ): illegal pixel_size '%d'\n", 
                                        filename.c_str(), targa_header.pixel_size );
								break;
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
