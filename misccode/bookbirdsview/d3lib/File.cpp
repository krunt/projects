/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "Lib.h"

#include "File.h"

#include <stdarg.h>



/*
=================
FS_WriteFloatString
=================
*/
int FS_WriteFloatString( char *buf, const char *fmt, va_list argPtr ) {
	long i;
	unsigned long u;
	double f;
	char *str;
	int index;
	idStr tmp, format;

	index = 0;

	while( *fmt ) {
		switch( *fmt ) {
			case '%':
				format = "";
				format += *fmt++;
				while ( (*fmt >= '0' && *fmt <= '9') ||
						*fmt == '.' || *fmt == '-' || *fmt == '+' || *fmt == '#') {
					format += *fmt++;
				}
				format += *fmt;
				switch( *fmt ) {
					case 'f':
					case 'e':
					case 'E':
					case 'g':
					case 'G':
						f = va_arg( argPtr, double );
						if ( format.Length() <= 2 ) {
							// high precision floating point number without trailing zeros
							sprintf( tmp, "%1.10f", f );
							tmp.StripTrailing( '0' );
							tmp.StripTrailing( '.' );
							index += sprintf( buf+index, "%s", tmp.c_str() );
						}
						else {
							index += sprintf( buf+index, format.c_str(), f );
						}
						break;
					case 'd':
					case 'i':
						i = va_arg( argPtr, long );
						index += sprintf( buf+index, format.c_str(), i );
						break;
					case 'u':
						u = va_arg( argPtr, unsigned long );
						index += sprintf( buf+index, format.c_str(), u );
						break;
					case 'o':
						u = va_arg( argPtr, unsigned long );
						index += sprintf( buf+index, format.c_str(), u );
						break;
					case 'x':
						u = va_arg( argPtr, unsigned long );
						index += sprintf( buf+index, format.c_str(), u );
						break;
					case 'X':
						u = va_arg( argPtr, unsigned long );
						index += sprintf( buf+index, format.c_str(), u );
						break;
					case 'c':
						i = va_arg( argPtr, long );
						index += sprintf( buf+index, format.c_str(), (char) i );
						break;
					case 's':
						str = va_arg( argPtr, char * );
						index += sprintf( buf+index, format.c_str(), str );
						break;
					case '%':
						index += sprintf( buf+index, format.c_str() ); //-V618
						break;
					default:
						idReportError( "FS_WriteFloatString: invalid format %s", format.c_str() );
						break;
				}
				fmt++;
				break;
			case '\\':
				fmt++;
				switch( *fmt ) {
					case 't':
						index += sprintf( buf+index, "\t" );
						break;
					case 'v':
						index += sprintf( buf+index, "\v" );
						break;
					case 'n':
						index += sprintf( buf+index, "\n" );
						break;
					case '\\':
						index += sprintf( buf+index, "\\" );
						break;
					default:
						idReportError( "FS_WriteFloatString: unknown escape character \'%c\'", *fmt );
						break;
				}
				fmt++;
				break;
			default:
				index += sprintf( buf+index, "%c", *fmt );
				fmt++;
				break;
		}
	}

	return index;
}

/*
=================================================================================

idFile

=================================================================================
*/

/*
=================
idFile::GetName
=================
*/
const char *idFile::GetName() const {
	return "";
}

/*
=================
idFile::GetFullPath
=================
*/
const char *idFile::GetFullPath() const {
	return "";
}

/*
=================
idFile::Read
=================
*/
int idFile::Read( void *buffer, int len ) {
	idFatalError( "idFile::Read: cannot read from idFile" );
	return 0;
}

/*
=================
idFile::Write
=================
*/
int idFile::Write( const void *buffer, int len ) {
	idFatalError( "idFile::Write: cannot write to idFile" );
	return 0;
}

/*
=================
idFile::Length
=================
*/
int idFile::Length() const {
	return 0;
}

/*
=================
idFile::Timestamp
=================
*/
ID_TIME_T idFile::Timestamp() const {
	return 0;
}

/*
=================
idFile::Tell
=================
*/
int idFile::Tell() const {
	return 0;
}

/*
=================
idFile::ForceFlush
=================
*/
void idFile::ForceFlush() {
}

/*
=================
idFile::Flush
=================
*/
void idFile::Flush() {
}

/*
=================
idFile::Seek
=================
*/
int idFile::Seek( long offset, fsOrigin_t origin ) {
	return -1;
}

/*
=================
idFile::Rewind
=================
*/
void idFile::Rewind() {
	Seek( 0, FS_SEEK_SET );
}

/*
=================
idFile::Printf
=================
*/
int idFile::Printf( const char *fmt, ... ) {
	char buf[MAX_PRINT_MSG];
	int length;
	va_list argptr;

	va_start( argptr, fmt );
	length = idStr::vsnPrintf( buf, MAX_PRINT_MSG-1, fmt, argptr );
	va_end( argptr );

	// so notepad formats the lines correctly
  	idStr	work( buf );
 	work.Replace( "\n", "\r\n" );
  
  	return Write( work.c_str(), work.Length() );
}

/*
=================
idFile::VPrintf
=================
*/
int idFile::VPrintf( const char *fmt, va_list args ) {
	char buf[MAX_PRINT_MSG];
	int length;

	length = idStr::vsnPrintf( buf, MAX_PRINT_MSG-1, fmt, args );
	return Write( buf, length );
}

/*
=================
idFile::WriteFloatString
=================
*/
int idFile::WriteFloatString( const char *fmt, ... ) {
	char buf[MAX_PRINT_MSG];
	int len;
	va_list argPtr;

	va_start( argPtr, fmt );
	len = FS_WriteFloatString( buf, fmt, argPtr );
	va_end( argPtr );

	return Write( buf, len );
}

/*
 =================
 idFile::ReadInt
 =================
 */
int idFile::ReadInt( int &value ) {
	int result = Read( &value, sizeof( value ) );
	value = LittleLong(value);
	return result;
}

/*
 =================
 idFile::ReadUnsignedInt
 =================
 */
int idFile::ReadUnsignedInt( unsigned int &value ) {
	int result = Read( &value, sizeof( value ) );
	value = LittleLong(value);
	return result;
}

/*
 =================
 idFile::ReadShort
 =================
 */
int idFile::ReadShort( short &value ) {
	int result = Read( &value, sizeof( value ) );
	value = LittleShort(value);
	return result;
}

/*
 =================
 idFile::ReadUnsignedShort
 =================
 */
int idFile::ReadUnsignedShort( unsigned short &value ) {
	int result = Read( &value, sizeof( value ) );
	value = LittleShort(value);
	return result;
}

/*
 =================
 idFile::ReadChar
 =================
 */
int idFile::ReadChar( char &value ) {
	return Read( &value, sizeof( value ) );
}

/*
 =================
 idFile::ReadUnsignedChar
 =================
 */
int idFile::ReadUnsignedChar( unsigned char &value ) {
	return Read( &value, sizeof( value ) );
}

/*
 =================
 idFile::ReadFloat
 =================
 */
int idFile::ReadFloat( float &value ) {
	int result = Read( &value, sizeof( value ) );
	value = LittleFloat(value);
	return result;
}

/*
 =================
 idFile::ReadBool
 =================
 */
int idFile::ReadBool( bool &value ) {
	unsigned char c;
	int result = ReadUnsignedChar( c );
	value = c ? true : false;
	return result;
}

/*
 =================
 idFile::ReadString
 =================
 */
int idFile::ReadString( idStr &string ) {
	int len;
	int result = 0;
	
	ReadInt( len );
	if ( len >= 0 ) {
		string.Fill( ' ', len );
		result = Read( &string[ 0 ], len );
	}
	return result;
}

/*
 =================
 idFile::ReadVec2
 =================
 */
int idFile::ReadVec2( idVec2 &vec ) {
	int result = Read( &vec, sizeof( vec ) );
	LittleRevBytes( &vec, sizeof(float), sizeof(vec)/sizeof(float) );
	return result;
}

/*
 =================
 idFile::ReadVec3
 =================
 */
int idFile::ReadVec3( idVec3 &vec ) {
	int result = Read( &vec, sizeof( vec ) );
	LittleRevBytes( &vec, sizeof(float), sizeof(vec)/sizeof(float) );
	return result;
}

/*
 =================
 idFile::ReadVec4
 =================
 */
int idFile::ReadVec4( idVec4 &vec ) {
	int result = Read( &vec, sizeof( vec ) );
	LittleRevBytes( &vec, sizeof(float), sizeof(vec)/sizeof(float) );
	return result;
}

/*
 =================
 idFile::ReadVec6
 =================
 */
int idFile::ReadVec6( idVec6 &vec ) {
	int result = Read( &vec, sizeof( vec ) );
	LittleRevBytes( &vec, sizeof(float), sizeof(vec)/sizeof(float) );
	return result;
}

/*
 =================
 idFile::ReadMat3
 =================
 */
int idFile::ReadMat3( idMat3 &mat ) {
	int result = Read( &mat, sizeof( mat ) );
	LittleRevBytes( &mat, sizeof(float), sizeof(mat)/sizeof(float) );
	return result;
}

/*
 =================
 idFile::WriteInt
 =================
 */
int idFile::WriteInt( const int value ) {
	int v = LittleLong(value);
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteUnsignedInt
 =================
 */
int idFile::WriteUnsignedInt( const unsigned int value ) {
	unsigned int v = LittleLong(value);
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteShort
 =================
 */
int idFile::WriteShort( const short value ) {
	short v = LittleShort(value);
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteUnsignedShort
 =================
 */
int idFile::WriteUnsignedShort( const unsigned short value ) {
	unsigned short v = LittleShort(value);
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteChar
 =================
 */
int idFile::WriteChar( const char value ) {
	return Write( &value, sizeof( value ) );
}

/*
 =================
 idFile::WriteUnsignedChar
 =================
 */
int idFile::WriteUnsignedChar( const unsigned char value ) {
	return Write( &value, sizeof( value ) );
}

/*
 =================
 idFile::WriteFloat
 =================
 */
int idFile::WriteFloat( const float value ) {
	float v = LittleFloat(value);
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteBool
 =================
 */
int idFile::WriteBool( const bool value ) {
	unsigned char c = value;
	return WriteUnsignedChar( c );
}

/*
 =================
 idFile::WriteString
 =================
 */
int idFile::WriteString( const char *value ) {
	int len = strlen( value );
	WriteInt( len );
    return Write( value, len );
}

/*
 =================
 idFile::WriteVec2
 =================
 */
int idFile::WriteVec2( const idVec2 &vec ) {
	idVec2 v = vec;
	LittleRevBytes( &v, sizeof(float), sizeof(v)/sizeof(float) );
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteVec3
 =================
 */
int idFile::WriteVec3( const idVec3 &vec ) {
	idVec3 v = vec;
	LittleRevBytes( &v, sizeof(float), sizeof(v)/sizeof(float) );
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteVec4
 =================
 */
int idFile::WriteVec4( const idVec4 &vec ) {
	idVec4 v = vec;
	LittleRevBytes( &v, sizeof(float), sizeof(v)/sizeof(float) );
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteVec6
 =================
 */
int idFile::WriteVec6( const idVec6 &vec ) {
	idVec6 v = vec;
	LittleRevBytes( &v, sizeof(float), sizeof(v)/sizeof(float) );
	return Write( &v, sizeof( v ) );
}

/*
 =================
 idFile::WriteMat3
 =================
 */
int idFile::WriteMat3( const idMat3 &mat ) {
	idMat3 v = mat;
	LittleRevBytes(&v, sizeof(float), sizeof(v)/sizeof(float) );
	return Write( &v, sizeof( v ) );
}

/*
=================================================================================

idFile_Memory

=================================================================================
*/


/*
=================
idFile_Memory::idFile_Memory
=================
*/
idFile_Memory::idFile_Memory() {
	name = "*unknown*";
	maxSize = 0;
	fileSize = 0;
	allocated = 0;
	granularity = 16384;

	mode = ( 1 << FS_WRITE );
	filePtr = NULL;
	curPtr = NULL;
}

/*
=================
idFile_Memory::idFile_Memory
=================
*/
idFile_Memory::idFile_Memory( const char *name ) {
	this->name = name;
	maxSize = 0;
	fileSize = 0;
	allocated = 0;
	granularity = 16384;

	mode = ( 1 << FS_WRITE );
	filePtr = NULL;
	curPtr = NULL;
}

/*
=================
idFile_Memory::idFile_Memory
=================
*/
idFile_Memory::idFile_Memory( const char *name, char *data, int length ) {
	this->name = name;
	maxSize = length;
	fileSize = 0;
	allocated = length;
	granularity = 16384;

	mode = ( 1 << FS_WRITE );
	filePtr = data;
	curPtr = data;
}

/*
=================
idFile_Memory::idFile_Memory
=================
*/
idFile_Memory::idFile_Memory( const char *name, const char *data, int length ) {
	this->name = name;
	maxSize = 0;
	fileSize = length;
	allocated = 0;
	granularity = 16384;

	mode = ( 1 << FS_READ );
	filePtr = const_cast<char *>(data);
	curPtr = const_cast<char *>(data);
}

/*
=================
idFile_Memory::TakeDataOwnership

this also makes the file read only
=================
*/
void idFile_Memory::TakeDataOwnership() {
	if ( filePtr != NULL && fileSize > 0 ) {
		maxSize = 0;
		mode = ( 1 << FS_READ );
		allocated = fileSize;
	}
}

/*
=================
idFile_Memory::~idFile_Memory
=================
*/
idFile_Memory::~idFile_Memory() {
	if ( filePtr && allocated > 0 && maxSize == 0 ) {
		Mem_Free( filePtr );
	}
}

/*
=================
idFile_Memory::Read
=================
*/
int idFile_Memory::Read( void *buffer, int len ) {

	if ( !( mode & ( 1 << FS_READ ) ) ) {
		idFatalError( "idFile_Memory::Read: %s not opened in read mode", name.c_str() );
		return 0;
	}

	if ( curPtr + len > filePtr + fileSize ) {
		len = filePtr + fileSize - curPtr;
	}
	memcpy( buffer, curPtr, len );
	curPtr += len;
	return len;
}

void * memcpy2( void * __restrict b, const void * __restrict a, size_t n ) {
	char * s1 = (char *)b;
	const char * s2 = (const char *)a;
	for ( ; 0 < n; --n ) {
		*s1++ = *s2++;
	}
	return b;
}

/*
=================
idFile_Memory::Write
=================
*/
int idFile_Memory::Write( const void *buffer, int len ) {
	if ( len == 0 ) {
		// ~4% falls into this case for some reason...
		return 0;
	}

	if ( !( mode & ( 1 << FS_WRITE ) ) ) {
		idFatalError( "idFile_Memory::Write: %s not opened in write mode", name.c_str() );
		return 0;
	}

	int alloc = curPtr + len + 1 - filePtr - allocated; // need room for len+1
	if ( alloc > 0 ) {
		if ( maxSize != 0 ) {
			idReportError( "idFile_Memory::Write: exceeded maximum size %d", maxSize );
			return 0;
		}
		int extra = granularity * ( 1 + alloc / granularity );
		char *newPtr = (char *) Mem_Alloc( allocated + extra );
		if ( allocated ) {
			memcpy( newPtr, filePtr, allocated );
		}
		allocated += extra;
		curPtr = newPtr + ( curPtr - filePtr );		
		if ( filePtr ) {
			Mem_Free( filePtr );
		}
		filePtr = newPtr;
	}

	//memcpy( curPtr, buffer, len );
	memcpy2( curPtr, buffer, len );

#if 0
	if ( memcpyImpl.GetInteger() == 0 ) {
		memcpy( curPtr, buffer, len );
	} else if ( memcpyImpl.GetInteger() == 1 ) {
		memcpy( curPtr, buffer, len );
	} else if ( memcpyImpl.GetInteger() == 2 ) {
		memcpy2( curPtr, buffer, len );
	}
#endif

#if 0
	int * value;
	if ( histogram.Get( len, &value ) && value != NULL ) {
		(*value)++;
	} else {
		histogram.Set( len, 1 );
	}
#endif

	curPtr += len;
	fileSize += len;
	filePtr[ fileSize ] = 0; // len + 1
	return len;
}

/*
=================
idFile_Memory::Length
=================
*/
int idFile_Memory::Length() const {
	return fileSize;
}

/*
========================
idFile_Memory::SetLength
========================
*/
void idFile_Memory::SetLength( size_t len ) {
	PreAllocate( len );
	fileSize = len;
}

/*
========================
idFile_Memory::PreAllocate
========================
*/
void idFile_Memory::PreAllocate( size_t len ) {
	if ( len > allocated ) {
		if ( maxSize != 0 ) {
			idReportError( "idFile_Memory::SetLength: exceeded maximum size %d", maxSize );
		}
		char * newPtr = (char *)Mem_Alloc( len );
		if ( allocated > 0 ) {
			memcpy( newPtr, filePtr, allocated );
		}
		allocated = len;
		curPtr = newPtr + ( curPtr - filePtr );		
		if ( filePtr != NULL ) {
			Mem_Free( filePtr );
		}
		filePtr = newPtr;
	}
}

/*
=================
idFile_Memory::Timestamp
=================
*/
ID_TIME_T idFile_Memory::Timestamp() const {
	return 0;
}

/*
=================
idFile_Memory::Tell
=================
*/
int idFile_Memory::Tell() const {
	return ( curPtr - filePtr );
}

/*
=================
idFile_Memory::ForceFlush
=================
*/
void idFile_Memory::ForceFlush() {
}

/*
=================
idFile_Memory::Flush
=================
*/
void idFile_Memory::Flush() {
}

/*
=================
idFile_Memory::Seek

  returns zero on success and -1 on failure
=================
*/
int idFile_Memory::Seek( long offset, fsOrigin_t origin ) {

	switch( origin ) {
		case FS_SEEK_CUR: {
			curPtr += offset;
			break;
		}
		case FS_SEEK_END: {
			curPtr = filePtr + fileSize - offset;
			break;
		}
		case FS_SEEK_SET: {
			curPtr = filePtr + offset;
			break;
		}
		default: {
			idFatalError( "idFile_Memory::Seek: bad origin for %s\n", name.c_str() );
			return -1;
		}
	}
	if ( curPtr < filePtr ) {
		curPtr = filePtr;
		return -1;
	}
	if ( curPtr > filePtr + fileSize ) {
		curPtr = filePtr + fileSize;
		return -1;
	}
	return 0;
}

/*
========================
idFile_Memory::SetMaxLength 
========================
*/
void idFile_Memory::SetMaxLength( size_t len ) {
	size_t oldLength = fileSize;

	SetLength( len );

	maxSize = len;
	fileSize = oldLength;
}

/*
=================
idFile_Memory::MakeReadOnly
=================
*/
void idFile_Memory::MakeReadOnly() {
	mode = ( 1 << FS_READ );
	Rewind();
}

/*
========================
idFile_Memory::MakeWritable
========================
*/
void idFile_Memory::MakeWritable() {
	mode = ( 1 << FS_WRITE );
	Rewind();
}

/*
=================
idFile_Memory::Clear
=================
*/
void idFile_Memory::Clear( bool freeMemory ) {
	fileSize = 0;
	granularity = 16384;
	if ( freeMemory ) {
		allocated = 0;
		Mem_Free( filePtr );
		filePtr = NULL;
		curPtr = NULL;
	} else {
		curPtr = filePtr;
	}
}

/*
=================
idFile_Memory::SetData
=================
*/
void idFile_Memory::SetData( const char *data, int length ) {
	maxSize = 0;
	fileSize = length;
	allocated = 0;
	granularity = 16384;

	mode = ( 1 << FS_READ );
	filePtr = const_cast<char *>(data);
	curPtr = const_cast<char *>(data);
}

/*
========================
idFile_Memory::TruncateData
========================
*/
void idFile_Memory::TruncateData( size_t len ) {
	if ( len > allocated ) {
		idReportError( "idFile_Memory::TruncateData: len (%d) exceeded allocated size (%d)", len, allocated );
	} else {
		fileSize = len;
	}
}

/*
=================================================================================

idFile_Permanent

=================================================================================
*/

/*
=================
idFile_Permanent::idFile_Permanent
=================
*/
idFile_Permanent::idFile_Permanent() {
	name = "invalid";
	o = NULL;
	mode = 0;
	fileSize = 0;
	handleSync = false;
}

/*
=================
idFile_Permanent::~idFile_Permanent
=================
*/
idFile_Permanent::~idFile_Permanent() {
	if ( o ) {
		CloseHandle( o );
	}
}

/*
=================
idFile_Permanent::Read

Properly handles partial reads
=================
*/
int idFile_Permanent::Read( void *buffer, int len ) {
	int		block, remaining;
	int		read;
	byte *	buf;
	int		tries;

	if ( !(mode & ( 1 << FS_READ ) ) ) {
		idFatalError( "idFile_Permanent::Read: %s not opened in read mode", name.c_str() );
		return 0;
	}

	if ( !o ) {
		return 0;
	}

	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while( remaining ) {
		block = remaining;
		DWORD bytesRead;
		if ( !ReadFile( o, buf, block, &bytesRead, NULL ) ) {
			idReportWarning( "idFile_Permanent::Read failed with %d from %s", GetLastError(), name.c_str() );
		}
		read = bytesRead;
		if ( read == 0 ) {
			// we might have been trying to read from a CD, which
			// sometimes returns a 0 read on windows
			if ( !tries ) {
				tries = 1;
			}
			else {
				return len-remaining;
			}
		}

		if ( read == -1 ) {
			idFatalError( "idFile_Permanent::Read: -1 bytes read from %s", name.c_str() );
		}

		remaining -= read;
		buf += read;
	}
	return len;
}

/*
=================
idFile_Permanent::Write

Properly handles partial writes
=================
*/
int idFile_Permanent::Write( const void *buffer, int len ) {
	int		block, remaining;
	int		written;
	byte *	buf;
	int		tries;

	if ( !( mode & ( 1 << FS_WRITE ) ) ) {
		idFatalError( "idFile_Permanent::Write: %s not opened in write mode", name.c_str() );
		return 0;
	}

	if ( !o ) {
		return 0;
	}

	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while( remaining ) {
		block = remaining;
		DWORD bytesWritten;
		WriteFile( o, buf, block, &bytesWritten, NULL );
		written = bytesWritten;
		if ( written == 0 ) {
			if ( !tries ) {
				tries = 1;
			}
			else {
				idReportWarning( "idFile_Permanent::Write: 0 bytes written to %s\n", name.c_str() );
				return 0;
			}
		}

		if ( written == -1 ) {
			idReportWarning( "idFile_Permanent::Write: -1 bytes written to %s\n", name.c_str() );
			return 0;
		}

		remaining -= written;
		buf += written;
		fileSize += written;
	}
	if ( handleSync ) {
		Flush();
	}
	return len;
}

/*
=================
idFile_Permanent::ForceFlush
=================
*/
void idFile_Permanent::ForceFlush() {
	FlushFileBuffers( o );
}

/*
=================
idFile_Permanent::Flush
=================
*/
void idFile_Permanent::Flush() {
	FlushFileBuffers( o );
}

/*
=================
idFile_Permanent::Tell
=================
*/
int idFile_Permanent::Tell() const {
	return SetFilePointer( o, 0, NULL, FILE_CURRENT );
}

/*
================
idFile_Permanent::Length
================
*/
int idFile_Permanent::Length() const {
	return fileSize;
}

/*
================
idFile_Permanent::Timestamp
================
*/
ID_TIME_T idFile_Permanent::Timestamp() const {
	return 0;
}

/*
=================
idFile_Permanent::Seek

  returns zero on success and -1 on failure
=================
*/
int idFile_Permanent::Seek( long offset, fsOrigin_t origin ) {
	int retVal = INVALID_SET_FILE_POINTER;
	switch( origin ) {
		case FS_SEEK_CUR: retVal = SetFilePointer( o, offset, NULL, FILE_CURRENT ); break;
		case FS_SEEK_END: retVal = SetFilePointer( o, offset, NULL, FILE_END ); break;
		case FS_SEEK_SET: retVal = SetFilePointer( o, offset, NULL, FILE_BEGIN ); break;
	}
	return ( retVal == INVALID_SET_FILE_POINTER ) ? -1 : 0;
}

#if 1
/*
=================================================================================

idFile_Cached

=================================================================================
*/

/*
=================
idFile_Cached::idFile_Cached
=================
*/
idFile_Cached::idFile_Cached() : idFile_Permanent() {
	internalFilePos = 0;
	bufferedStartOffset = 0;
	bufferedEndOffset = 0;
	buffered = NULL;
}

/*
=================
idFile_Cached::~idFile_Cached
=================
*/
idFile_Cached::~idFile_Cached() {
	Mem_Free( buffered );
}

/*
=================
idFile_ReadBuffered::BufferData

Buffer a section of the file
=================
*/
void idFile_Cached::CacheData( uint64 offset, uint64 length ) {
	Mem_Free( buffered );
	bufferedStartOffset = offset;
	bufferedEndOffset = offset + length;
	buffered = ( byte* )Mem_Alloc( length );
	if ( buffered == NULL ) {
		return;
	}
	int internalFilePos = idFile_Permanent::Tell();
	idFile_Permanent::Seek( offset, FS_SEEK_SET );
	idFile_Permanent::Read( buffered, length );
	idFile_Permanent::Seek( internalFilePos, FS_SEEK_SET );
}

/*
=================
idFile_ReadBuffered::Read

=================
*/
int idFile_Cached::Read( void *buffer, int len ) {
	if ( internalFilePos >= bufferedStartOffset && internalFilePos + len < bufferedEndOffset ) {
		// this is in the buffer
		memcpy( buffer, (void*)&buffered[ internalFilePos - bufferedStartOffset ], len );
		internalFilePos += len;
		return len;
	}
	int read = idFile_Permanent::Read( buffer, len );
	if ( read != -1 ) {
		internalFilePos += ( int64 )read;
	}
	return read;
}



/*
=================
idFile_Cached::Tell
=================
*/
int idFile_Cached::Tell() const {
	return internalFilePos;
}

/*
=================
idFile_Cached::Seek

  returns zero on success and -1 on failure
=================
*/
int idFile_Cached::Seek( long offset, fsOrigin_t origin ) {
	if ( origin == FS_SEEK_SET && offset >= bufferedStartOffset && offset < bufferedEndOffset ) {
		// don't do anything to the actual file ptr, just update or internal position
		internalFilePos = offset;
		return 0;
	}

	int retVal = idFile_Permanent::Seek( offset, origin );
	internalFilePos = idFile_Permanent::Tell();
	return retVal;
}
#endif

/*
================================================================================================

idFileLocal

================================================================================================
*/

/*
========================
idFileLocal::~idFileLocal

Destructor that will destroy (close) the managed file when this wrapper class goes out of scope.
========================
*/
idFileLocal::~idFileLocal() {
	if ( file != NULL ) {
		delete file;
		file = NULL;
	}
}

idFile *idFileSystemLocal::OpenFile(const char *filename, int mode)
{
   HANDLE res;

   DWORD desiredAccess= 0, createDisposition= 0;

   if (mode&1<<FS_READ)
   {
      desiredAccess|= GENERIC_READ;
      createDisposition|= OPEN_EXISTING;
   }
   else if (mode&1<<FS_WRITE)
   {
      desiredAccess|= GENERIC_WRITE;
      createDisposition|= TRUNCATE_EXISTING|CREATE_ALWAYS;
   }
   else if (mode&1<<FS_APPEND)
   {
      desiredAccess|= GENERIC_WRITE | GENERIC_READ;
      createDisposition|= CREATE_ALWAYS;
   }

   res= CreateFileA(
      filename,
      desiredAccess,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      createDisposition,
      FILE_ATTRIBUTE_NORMAL,
      0
   );

   idFile *file= NULL;
   if (res!=INVALID_HANDLE_VALUE) {
      idFile_Permanent *fperm= new idFile_Permanent();
      fperm->o= res;
      file= fperm;
   }
   return file;
}

idFile *idFileSystemLocal::OpenFileRead(const char *filename) 
{ 
   return idFileSystemLocal::OpenFile(filename, 1<<FS_READ);
}
idFile *idFileSystemLocal::OpenFileWrite(const char *filename) 
{
   return idFileSystemLocal::OpenFile(filename, 1<<FS_WRITE);
}
idFile *idFileSystemLocal::OpenFileAppend(const char *filename)
{
   return idFileSystemLocal::OpenFile(filename, 1<<FS_APPEND);
}

void    idFileSystemLocal::CloseFile(idFile *fd)
{
   if (idFile_Permanent *perm= dynamic_cast<idFile_Permanent*>(fd)) {
      CloseHandle(perm->GetFilePtr());
   }
}