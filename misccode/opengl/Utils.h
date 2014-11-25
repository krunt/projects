#ifndef UTILS__H_
#define UTILS__H_

#include <stdio.h>
#include <string>
#include "d3lib/Lib.h"

bool EndsWith( const std::string &what, const std::string &suffix );
void BloatFile( const std::string &filename, std::string &contents );
bool IsPowerOf2( int v );
unsigned short LittleShort ( int b );

#define msg_failure0(format) \
    fprintf(stderr, format); \
    exit(1)

#define msg_warning0(format) \
    fprintf(stderr, format); \

#define msg_failure(format, ...) \
    fprintf(stderr, format, __VA_ARGS__); \
    exit(1)

#define msg_warning(format, ...) \
    fprintf(stderr, format, __VA_ARGS__)

#define _CH(x) \
    (x); checkError()

void checkError();
void printVector( const idVec2 &vec );
void printVector( const idVec3 &vec );
void printVector( const idVec4 &vec );
void printProjectedVector( const idVec4 &vec );

#endif