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

idVec2 vec2_origin( 0.0f, 0.0f );
idVec3 vec3_origin( 0.0f, 0.0f, 0.0f );
idVec4 vec4_origin( 0.0f, 0.0f, 0.0f, 0.0f );
idVec5 vec5_origin( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
idVec6 vec6_origin( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
idVec6 vec6_infinity( idMath::INFINITY, idMath::INFINITY, idMath::INFINITY, idMath::INFINITY, idMath::INFINITY, idMath::INFINITY );


//===============================================================
//
//	idVec2
//
//===============================================================

/*
=============
idVec2::ToString
=============
*/
std::string idVec2::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    //return 0;

    char buf[4096];
    snprintf( buf, sizeof( buf ), "[ %f %f ]", x, y );
    return buf;
}

idVec2 idVec2::FromString( const std::string &msg ) {
    float x, y;
    sscanf( msg.c_str(), "[ %f %f ]", &x, &y );
    return idVec2( x, y );
}

/*
=============
Lerp

Linearly inperpolates one vector to another.
=============
*/
void idVec2::Lerp( const idVec2 &v1, const idVec2 &v2, const float l ) {
	if ( l <= 0.0f ) {
		(*this) = v1;
	} else if ( l >= 1.0f ) {
		(*this) = v2;
	} else {
		(*this) = v1 + l * ( v2 - v1 );
	}
}


//===============================================================
//
//	idVec3
//
//===============================================================

/*
=============
idVec3::ToYaw
=============
*/
float idVec3::ToYaw( void ) const {
	float yaw;
	
	if ( ( y == 0.0f ) && ( x == 0.0f ) ) {
		yaw = 0.0f;
	} else {
		yaw = RAD2DEG( atan2( y, x ) );
		if ( yaw < 0.0f ) {
			yaw += 360.0f;
		}
	}

	return yaw;
}

/*
=============
idVec3::ToPitch
=============
*/
float idVec3::ToPitch( void ) const {
	float	forward;
	float	pitch;
	
	if ( ( x == 0.0f ) && ( y == 0.0f ) ) {
		if ( z > 0.0f ) {
			pitch = 90.0f;
		} else {
			pitch = 270.0f;
		}
	} else {
		forward = ( float )idMath::Sqrt( x * x + y * y );
		pitch = RAD2DEG( atan2( z, forward ) );
		if ( pitch < 0.0f ) {
			pitch += 360.0f;
		}
	}

	return pitch;
}

/*
=============
idVec3::ToAngles
=============
*/
/*
=============
idVec3::ToPolar
=============
*/


/*
=============
idVec3::ToMat3
=============
*/
idMat3 idVec3::ToMat3( void ) const {
	idMat3	mat;
	float	d;

	mat[0] = *this;
	d = x * x + y * y;
	if ( !d ) {
		mat[1][0] = 1.0f;
		mat[1][1] = 0.0f;
		mat[1][2] = 0.0f;
	} else {
		d = idMath::InvSqrt( d );
		mat[1][0] = -y * d;
		mat[1][1] = x * d;
		mat[1][2] = 0.0f;
	}
	mat[2] = Cross( mat[1] );

	return mat;
}

/*
=============
idVec3::ToString
=============
*/
std::string idVec3::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    char buf[4096];
    snprintf( buf, sizeof( buf ), "[ %f %f %f ]", x, y, z );
    return buf;
}

idVec3 idVec3::FromString( const std::string &msg ) {
    float x, y, z;
    sscanf( msg.c_str(), "[ %f %f %f ]", &x, &y, &z );
    return idVec3( x, y, z );
}

/*
=============
Lerp

Linearly inperpolates one vector to another.
=============
*/
void idVec3::Lerp( const idVec3 &v1, const idVec3 &v2, const float l ) {
	if ( l <= 0.0f ) {
		(*this) = v1;
	} else if ( l >= 1.0f ) {
		(*this) = v2;
	} else {
		(*this) = v1 + l * ( v2 - v1 );
	}
}

/*
=============
SLerp

Spherical linear interpolation from v1 to v2.
Vectors are expected to be normalized.
=============
*/
#define LERP_DELTA 1e-6

void idVec3::SLerp( const idVec3 &v1, const idVec3 &v2, const float t ) {
	float omega, cosom, sinom, scale0, scale1;

	if ( t <= 0.0f ) {
		(*this) = v1;
		return;
	} else if ( t >= 1.0f ) {
		(*this) = v2;
		return;
	}

	cosom = v1 * v2;
	if ( ( 1.0f - cosom ) > LERP_DELTA ) {
		omega = acos( cosom );
		sinom = sin( omega );
		scale0 = sin( ( 1.0f - t ) * omega ) / sinom;
		scale1 = sin( t * omega ) / sinom;
	} else {
		scale0 = 1.0f - t;
		scale1 = t;
	}

	(*this) = ( v1 * scale0 + v2 * scale1 );
}

/*
=============
ProjectSelfOntoSphere

Projects the z component onto a sphere.
=============
*/
void idVec3::ProjectSelfOntoSphere( const float radius ) {
	float rsqr = radius * radius;
	float len = Length();
	if ( len  < rsqr * 0.5f ) {
		z = sqrt( rsqr - len );
	} else {
		z = rsqr / ( 2.0f * sqrt( len ) );
	}
}



//===============================================================
//
//	idVec4
//
//===============================================================

/*
=============
idVec4::ToString
=============
*/
std::string idVec4::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    char buf[4096];
    snprintf( buf, sizeof( buf ), "[ %f %f %f %f ]", x, y, z, w );
    return buf;
}

idVec4 idVec4::FromString( const std::string &msg ) {
    float x, y, z, w;
    sscanf( msg.c_str(), "[ %f %f %f %f ]", &x, &y, &z, &w );
    return idVec4( x, y, z, w );
}

/*
=============
Lerp

Linearly inperpolates one vector to another.
=============
*/
void idVec4::Lerp( const idVec4 &v1, const idVec4 &v2, const float l ) {
	if ( l <= 0.0f ) {
		(*this) = v1;
	} else if ( l >= 1.0f ) {
		(*this) = v2;
	} else {
		(*this) = v1 + l * ( v2 - v1 );
	}
}


//===============================================================
//
//	idVec5
//
//===============================================================

/*
=============
idVec5::ToString
=============
*/
const char *idVec5::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    return 0;
}

/*
=============
idVec5::Lerp
=============
*/
void idVec5::Lerp( const idVec5 &v1, const idVec5 &v2, const float l ) {
	if ( l <= 0.0f ) {
		(*this) = v1;
	} else if ( l >= 1.0f ) {
		(*this) = v2;
	} else {
		x = v1.x + l * ( v2.x - v1.x );
		y = v1.y + l * ( v2.y - v1.y );
		z = v1.z + l * ( v2.z - v1.z );
		s = v1.s + l * ( v2.s - v1.s );
		t = v1.t + l * ( v2.t - v1.t );
	}
}


//===============================================================
//
//	idVec6
//
//===============================================================

/*
=============
idVec6::ToString
=============
*/
const char *idVec6::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    return 0;
}


//===============================================================
//
//	idVecX
//
//===============================================================

float	idVecX::temp[VECX_MAX_TEMP+4];
float *	idVecX::tempPtr = (float *) ( ( (int) idVecX::temp + 15 ) & ~15 );
int		idVecX::tempIndex = 0;

/*
=============
idVecX::ToString
=============
*/
const char *idVecX::ToString( int precision ) const {
	//return idStr::FloatArrayToString( ToFloatPtr(), GetDimension(), precision );
    return 0;
}

float VectorLengthSquared(vec3_t v) {
	return DotProduct(v, v);
}

float VectorNormalize(vec3_t inout)
{
	float	length, ilength;

	length = sqrt (inout[0]*inout[0] + inout[1]*inout[1] + inout[2]*inout[2]);
	if (length == 0)
	{
		VectorClear (inout);
		return 0;
	}

	ilength = 1.0/length;
	inout[0] = inout[0]*ilength;
	inout[1] = inout[1]*ilength;
	inout[2] = inout[2]*ilength;

	return length;
}

float VectorNormalize2(const vec3_t in, vec3_t out)
{
	float	length, ilength;

	length = sqrt (in[0]*in[0] + in[1]*in[1] + in[2]*in[2]);
	if (length == 0)
	{
		VectorClear (out);
		return 0;
	}

	ilength = 1.0/length;
	out[0] = in[0]*ilength;
	out[1] = in[1]*ilength;
	out[2] = in[2]*ilength;

	return length;
}

void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

