#ifndef TGA__H_
#define TGA__H_

struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

byte *R_LoadTGA( const std::string &filename, int &width, 
        int &height, int &format );

#endif /* TGA__H_ */
