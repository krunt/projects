#include <string.h>
#include <assert.h>
#include <stdlib.h>

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
        byte palette[256][3], byte candidates[256])
{
    int mindist2_mas[256];
    int minmaxdist2 = 1<<30;

    memset(candidates, 0, 256);

    int mr = sr + kStepSize / 2;
    int mg = sg + kStepSize / 2;
    int mb = sb + kStepSize / 2;

    for ( int i = 0; i < 256; ++i ) {
        int r, g, b;

        r = palette[i][0];
        g = palette[i][1];
        b = palette[i][2];

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
        byte *image, byte palette[256][3], byte candidates[256] )
{
    for ( int i = 0; i < kStepSize; ++i ) {
        for ( int j = 0; j < kStepSize; ++j ) {
            for ( int k = 0; k < kStepSize; ++k ) {
                int mindist2 = 1<<30;
                int minindex = -1;
                for ( int t = 0; t < 256; ++t ) {
                    if ( !candidates[t] ) {
                        continue;
                    }

                    int r, g, b;

                    r = palette[t][0];
                    g = palette[t][1];
                    b = palette[t][2];

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
        byte palette[256][3]) 
{
    for ( int i = 0; i < width; i += kStepSize ) {
        for ( int j = 0; j < height; j += kStepSize ) {
            for ( int k = 0; k < depth; k += kStepSize ) {
                byte candidates[256];
                find_nearby_colors(i, j, k, palette, candidates);
                find_best_colors(i, j, k, image, palette, candidates);
            }
        }
    }
}

int main() {
    byte *image = (byte *)malloc(1<<24);
    byte palette[256][3] = {0};
    for ( int i = 0; i < 256; ++i ) {
        palette[i][0] = palette[i][1] = palette[i][2] = i;
    }
    convert_image(image, 256, 256, 256, palette);
    return 0;
}
