
#include <fstream>
#include <streambuf>

#include "Utils.h"

bool EndsWith( const std::string &what, const std::string &suffix ) {
    if ( suffix.size() >= what.size() ) {
        return false;
    }
    return what.substr( what.size() - suffix.size() ) == suffix;
}

void BloatFile( const std::string &filename, std::string &contents ) {
    std::ifstream t( filename.c_str() );
    contents = std::string((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
}

bool IsPowerOf2( int v ) {
    return (!(v & ( v - 1 ))) ? true : false;
}

unsigned short LittleShort ( int b ) { return b; }
unsigned int LittleLong( int b ) { return b; }
float LittleFloat( float b ) { return b; }

void checkError() {
    int err, anyErr = 0;
    const char *s;
    while ( ( err = glGetError() ) != GL_NO_ERROR ) {
        switch ( err ) {
        case GL_INVALID_ENUM: s = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: s = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: s = "GL_INVALID_OPERATION"; break;
        default: s = "other"; break;
        };
        fprintf( stderr, "err: %s\n", s );
        anyErr = 1;
    }

    if ( anyErr ) {
        assert( 0 );
    }
}

void printVector( const idVec2 &vec ) {
    fprintf( stderr, "(%f/%f)\n", vec.x, vec.y );
}

void printVector( const idVec3 &vec ) {
    fprintf( stderr, "(%f/%f/%f)\n", vec.x, vec.y, vec.z );
}

void printVector( const idVec4 &vec ) {
    fprintf( stderr, "(%f/%f/%f/%f)\n", vec[0], vec[1], vec[2], vec[3] );
}

void printProjectedVector( const idVec4 &vec ) {
    idVec4 v(vec);
    v /= vec[3];
    printVector(v);
}

byte *AlignUp( byte *v, int alignment ) {
    int m = reinterpret_cast<std::size_t>(v) % alignment;
    if ( m ) { v += alignment - m; }
    return v;
}
