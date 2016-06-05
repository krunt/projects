#include <stdarg.h>

typedef struct _TargaHeader {
    unsigned char   id_length, colormap_type, image_type;
    unsigned short  colormap_index, colormap_length;
    unsigned char   colormap_size;
    unsigned short  x_origin, y_origin, width, height;
    unsigned char   pixel_size, attributes;
} TargaHeader;

typedef unsigned char byte;

void giveerror(const char *err, ...) {
    va_list ap;
    va_start(ap, err);
    vfprintf(stderr, err, ap);
    exit(1);
}

static short LittleShort(short p)
{
    return p;
}

static void LoadTGA ( const char *name, byte **pic, int *width, int *height)
{
    int     columns, rows, numPixels;
    byte    *pixbuf;
    int     row, column;
    byte    *buf_p;
    byte    *buffer = NULL;
    TargaHeader targa_header;
    byte        *targa_rgba;

    *pic = NULL;

    //
    // load the file
    //
    {
        FILE *fd = fopen(name, "r");
        fseek(fd, 0, SEEK_END);
        int fsize = ftell(fd);
        buffer = (byte*)malloc(fsize);
        fseek(fd, 0, SEEK_SET);
        fread(buffer, 1, fsize, fd);
    }

    if (!buffer) {
        return;
    }

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

    if (//targa_header.image_type!=2 
        //&& targa_header.image_type!=10
        targa_header.image_type != 3 ) 
    {
        giveerror("LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n");
    }

    if ( targa_header.colormap_type != 0 )
    {
        giveerror("LoadTGA: colormaps not supported\n" );
    }

    if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 )
    {
        giveerror("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
    }

    columns = targa_header.width;
    rows = targa_header.height;
    numPixels = columns * rows;

    if (width)
        *width = columns;
    if (height)
        *height = rows;

    targa_rgba = (byte*)malloc (numPixels);
    *pic = targa_rgba;

    if (targa_header.id_length != 0)
        buf_p += targa_header.id_length;  // skip TARGA image comment
    
    if ( targa_header.image_type==2 || targa_header.image_type == 3 )
    { 
        // Uncompressed RGB or gray scale image
        for(row=rows-1; row>=0; row--) 
        {
            pixbuf = targa_rgba + row*columns;
            for(column=0; column<columns; column++) 
            {
                unsigned char red,green,blue,alphabyte;
                switch (targa_header.pixel_size) 
                {
                    
                case 8:
                    blue = *buf_p++;
                    green = blue;
                    red = blue;
                    *pixbuf++ = red;
//                    *pixbuf++ = green;
//                    *pixbuf++ = blue;
//                    *pixbuf++ = 255;
                    break;

                case 24:
                    giveerror("24 bit is unsupported\n");
#if 0
                    blue = *buf_p++;
                    green = *buf_p++;
                    red = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = 255;
#endif
                    break;
                case 32:
                    giveerror("32 bit is unsupported\n");
#if 0
                    blue = *buf_p++;
                    green = *buf_p++;
                    red = *buf_p++;
                    alphabyte = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = alphabyte;
#endif
                    break;
                default:
                    giveerror("LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
                    break;
                }
            }
        }
    }
    else if (targa_header.image_type==10) {   // Runlength encoded RGB images
        giveerror("image_type==10 is unsupported\n");
    }

  free(buffer);
}
