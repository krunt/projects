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

#include "Lib.h"

#include <signal.h>
#include <sys/types.h>
#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>

/*
===============================================================================

	idLib

===============================================================================
*/

/*
================
idLib::Init
================
*/
void idLib::Init( void ) {

	assert( sizeof( bool ) == 1 );

	// initialize math
	idMath::Init();
}

/*
================
idLib::ShutDown
================
*/
void idLib::ShutDown( void ) {
}

int  idLib::Milliseconds( void ) {
   SYSTEMTIME curTime;
   FILETIME fileTime;
   ULARGE_INTEGER quadNumber;

   GetSystemTime(&curTime);
   SystemTimeToFileTime(&curTime, &fileTime);

   quadNumber.LowPart= fileTime.dwLowDateTime;
   quadNumber.HighPart= fileTime.dwHighDateTime;

   quadNumber.QuadPart*= 10000ULL;

   return quadNumber.QuadPart&0xFFFFFFFFULL;
}


/*
===============================================================================

	Colors

===============================================================================
*/

idVec4	colorBlack	= idVec4( 0.00f, 0.00f, 0.00f, 1.00f );
idVec4	colorWhite	= idVec4( 1.00f, 1.00f, 1.00f, 1.00f );
idVec4	colorRed	= idVec4( 1.00f, 0.00f, 0.00f, 1.00f );
idVec4	colorGreen	= idVec4( 0.00f, 1.00f, 0.00f, 1.00f );
idVec4	colorBlue	= idVec4( 0.00f, 0.00f, 1.00f, 1.00f );
idVec4	colorYellow	= idVec4( 1.00f, 1.00f, 0.00f, 1.00f );
idVec4	colorMagenta= idVec4( 1.00f, 0.00f, 1.00f, 1.00f );
idVec4	colorCyan	= idVec4( 0.00f, 1.00f, 1.00f, 1.00f );
idVec4	colorOrange	= idVec4( 1.00f, 0.50f, 0.00f, 1.00f );
idVec4	colorPurple	= idVec4( 0.60f, 0.00f, 0.60f, 1.00f );
idVec4	colorPink	= idVec4( 0.73f, 0.40f, 0.48f, 1.00f );
idVec4	colorBrown	= idVec4( 0.40f, 0.35f, 0.08f, 1.00f );
idVec4	colorLtGrey	= idVec4( 0.75f, 0.75f, 0.75f, 1.00f );
idVec4	colorMdGrey	= idVec4( 0.50f, 0.50f, 0.50f, 1.00f );
idVec4	colorDkGrey	= idVec4( 0.25f, 0.25f, 0.25f, 1.00f );

static dword colorMask[2] = { 255, 0 };

/*
================
ColorFloatToByte
================
*/
ID_INLINE static byte ColorFloatToByte( float c ) {
	return (byte) ( ( (dword) ( c * 255.0f ) ) & colorMask[FLOATSIGNBITSET(c)] );
}

/*
================
PackColor
================
*/
dword PackColor( const idVec4 &color ) {
	dword dw, dx, dy, dz;

	dx = ColorFloatToByte( color.x );
	dy = ColorFloatToByte( color.y );
	dz = ColorFloatToByte( color.z );
	dw = ColorFloatToByte( color.w );

#if defined(_WIN32) || defined(__linux__) || (defined(MACOS_X) && defined(__i386__))
	return ( dx << 0 ) | ( dy << 8 ) | ( dz << 16 ) | ( dw << 24 );
#elif (defined(MACOS_X) && defined(__ppc__))
	return ( dx << 24 ) | ( dy << 16 ) | ( dz << 8 ) | ( dw << 0 );
#else
#error OS define is required!
#endif
}

/*
================
UnpackColor
================
*/
void UnpackColor( const dword color, idVec4 &unpackedColor ) {
#if defined(_WIN32) || defined(__linux__) || (defined(MACOS_X) && defined(__i386__))
	unpackedColor.Set( ( ( color >> 0 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 8 ) & 255 ) * ( 1.0f / 255.0f ), 
						( ( color >> 16 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 24 ) & 255 ) * ( 1.0f / 255.0f ) );
#elif (defined(MACOS_X) && defined(__ppc__))
	unpackedColor.Set( ( ( color >> 24 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 16 ) & 255 ) * ( 1.0f / 255.0f ), 
						( ( color >> 8 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 0 ) & 255 ) * ( 1.0f / 255.0f ) );
#else
#error OS define is required!
#endif
}

/*
================
PackColor
================
*/
dword PackColor( const idVec3 &color ) {
	dword dx, dy, dz;

	dx = ColorFloatToByte( color.x );
	dy = ColorFloatToByte( color.y );
	dz = ColorFloatToByte( color.z );

#if defined(_WIN32) || defined(__linux__) || (defined(MACOS_X) && defined(__i386__))
	return ( dx << 0 ) | ( dy << 8 ) | ( dz << 16 );
#elif (defined(MACOS_X) && defined(__ppc__))
	return ( dy << 16 ) | ( dz << 8 ) | ( dx << 0 );
#else
#error OS define is required!
#endif
}

/*
================
UnpackColor
================
*/
void UnpackColor( const dword color, idVec3 &unpackedColor ) {
#if defined(_WIN32) || defined(__linux__) || (defined(MACOS_X) && defined(__i386__))
	unpackedColor.Set( ( ( color >> 0 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 8 ) & 255 ) * ( 1.0f / 255.0f ), 
						( ( color >> 16 ) & 255 ) * ( 1.0f / 255.0f ) );
#elif (defined(MACOS_X) && defined(__ppc__))
	unpackedColor.Set( ( ( color >> 16 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 8 ) & 255 ) * ( 1.0f / 255.0f ),
						( ( color >> 0 ) & 255 ) * ( 1.0f / 255.0f ) );
#else
#error OS define is required!
#endif
}

void *Mem_ClearedAlloc( int size ) {
    void *p = malloc( size );
    memset( p, 0, size );
    return p;
}

void *Mem_Alloc16( int size ) {
    void *p;
    p= _aligned_malloc( size, 16 );
    return p;
}

void Mem_Free16( void *p ) {
    _aligned_free( p );
}

void *idBloatFile(const char *filename, int *size) {
   int fd;
   
      struct _stat buf;

   if( (fd = _open( filename, _O_RDONLY | _O_BINARY )) ==  -1 )
      return NULL;

      /* Get data associated with "fh": */
      int result = _fstat( fd, &buf );

      /* Check if statistics are valid: */
      if( result != 0 )
         return NULL;

      void *res= malloc(buf.st_size);
      if(!res)
         return NULL;

      if (size) {
         *size= (int)buf.st_size;
      }

      return _read(fd, res, buf.st_size)==buf.st_size?(_close(fd),res):(_close(fd),free(res),NULL);
}

void idReportError(const char *format, ...)
{
   va_list ap;
   va_start(ap,format);
   vfprintf(stderr, format, ap);
   va_end(ap);
}

bool  idWriteBloatFile(const char *filename, const void *buf, int size)
{
   int fd;
   if( (fd = _open( filename, _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY )) ==  -1 )
      return false;

   if (_write(fd, buf, size)!=size)
      return false;

   _close(fd);

   return true;
}