/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __LIB_H__
#define __LIB_H__

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

/*
===============================================================================

	idLib contains stateless support classes and concrete types. Some classes
	do have static variables, but such variables are initialized once and
	read-only after initialization (they do not maintain a modifiable state).

	The interface pointers idSys, idCommon, idCVarSystem and idFileSystem
	should be set before using idLib. The pointers stored here should not
	be used by any part of the engine except for idLib.

	The frameNumber should be continuously set to the number of the current
	frame if frame base memory logging is required.

===============================================================================
*/

class idLib {
public:
	static void					Init( void );
	static void					ShutDown( void );
    static  int                 Milliseconds( void );
};


/*
===============================================================================

	Types and defines used throughout the engine.

===============================================================================
*/

typedef unsigned char			byte;		// 8 bits
typedef unsigned short			word;		// 16 bits
typedef unsigned int			dword;		// 32 bits
typedef unsigned int			uint;
typedef unsigned long			ulong;

typedef signed char			int8;
typedef unsigned char		uint8;
typedef short int			int16;
typedef unsigned short int	uint16;
typedef int					int32;
typedef unsigned int		uint32;
typedef long long			int64;
typedef unsigned long long	uint64;

typedef int						qhandle_t;

class idVec3;
class idVec4;

#ifndef NULL
#define NULL					((void *)0)
#endif

#ifndef BIT
#define BIT( num )				( 1 << ( num ) )
#endif

#define ID_TIME_T int64

#define	MAX_STRING_CHARS		1024		// max length of a string
#define MAX_PRINT_MSG			16384	

// maximum world size
#define MAX_WORLD_COORD			( 128 * 1024 )
#define MIN_WORLD_COORD			( -128 * 1024 )
#define MAX_WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )

#define ID_INLINE   inline
#define ID_INLINE_EXTERN				extern inline
#define ID_FORCE_INLINE_EXTERN			extern __forceinline

#define ALIGN16( x )					__declspec(align(16)) x
#define ALIGNTYPE16						__declspec(align(16))
#define ALIGNTYPE128					__declspec(align(128))

#define		Mem_Alloc( size )				malloc( ( size ) )
#define		Mem_Free( ptr )					free( ( ptr ) )
#define		Mem_CopyString( s )				strdup( ( s ) )

void *Mem_ClearedAlloc( int size );
void *Mem_Alloc16( int size );
void Mem_Free16( void *p );

void *idBloatFile(const char *filename, int *size );
bool  idWriteBloatFile(const char *filename, const void *buf, int size);
void  idReportError(const char *format, ...);
#define idReportWarning(...) idReportError(__VA_ARGS__)
#define idFatalError(...) do { idReportError(__VA_ARGS__); idSysQuit(1); } while (0)
void idSysQuit(int exit_code);

#define _alloca16 alloca

// basic colors
extern	idVec4 colorBlack;
extern	idVec4 colorWhite;
extern	idVec4 colorRed;
extern	idVec4 colorGreen;
extern	idVec4 colorBlue;
extern	idVec4 colorYellow;
extern	idVec4 colorMagenta;
extern	idVec4 colorCyan;
extern	idVec4 colorOrange;
extern	idVec4 colorPurple;
extern	idVec4 colorPink;
extern	idVec4 colorBrown;
extern	idVec4 colorLtGrey;
extern	idVec4 colorMdGrey;
extern	idVec4 colorDkGrey;

// packs color floats in the range [0,1] into an integer
dword	PackColor( const idVec3 &color );
void	UnpackColor( const dword color, idVec3 &unpackedColor );
dword	PackColor( const idVec4 &color );
void	UnpackColor( const dword color, idVec4 &unpackedColor );

short	BigShort( short l );
short	LittleShort( short l );
int		BigLong( int l );
int		LittleLong( int l );
float	BigFloat( float l );
float	LittleFloat( float l );
void	BigRevBytes( void *bp, int elsize, int elcount );
void	LittleRevBytes( void *bp, int elsize, int elcount );
void	LittleBitField( void *bp, int elsize );

void	SixtetsForInt( byte *out, int src);
int		IntForSixtets( byte *in );

class idException {
public:
	char error[MAX_STRING_CHARS];

	idException( const char *text = "" ) { strcpy( error, text ); }
};

// move from Math.h to keep gcc happy
template<class T> ID_INLINE T	Max( T x, T y ) { return ( x > y ) ? x : y; }
template<class T> ID_INLINE T	Min( T x, T y ) { return ( x < y ) ? x : y; }

/*
===============================================================================

	idLib headers.

===============================================================================
*/

#include "Math.h"
#include "Vector.h"
#include "Matrix.h"
#include "Rotation.h"
#include "Angles.h"
#include "Quat.h"
#include "List.h"
#include "Plane.h"
#include "Pluecker.h"
#include "Winding.h"
#include "Winding2D.h"
#include "Bounds.h"
#include "Box.h"
#include "Sphere.h"
#include "TraceModel.h"
#include "Sort.h"
#include "Str.h"
#include "StrStatic.h"

#endif	/* !__LIB_H__ */
