#ifndef GLSL_PACK_HELPER__H_
#define GLSL_PACK_HELPER__H_

#include "Common.h"

#ifndef GL_ROUNDUP
#define GL_ROUNDUP(x,n) (!((x) % (n)) ? (x) : ((x) + (n) - ((x) % (n))))
#endif

template <typename AlignmentHelper>
class GLSLPackHelper {
public:
    typedef AlignmentHelper align_type;

    static byte *Pack( byte *p, GLint v, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<GLint> alType;
        if ( !alignment ) alignment = alType::value;
        GLint *pp = reinterpret_cast<GLint *>( p ); *pp = v;
        return p + alignment;
    }

    static byte *Pack( byte *p, float v, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<float> alType;
        if ( !alignment ) alignment = alType::value;
        float *pp = reinterpret_cast<float *>( p ); *pp = v;
        return p + alignment;
    }

    static byte *Pack( byte *p, const idVec2 &v, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<idVec2> alType;
        if ( !alignment ) alignment = alType::value;
        float *pp = reinterpret_cast<float *>( p ); 
        *pp++ = v[0]; *pp++ = v[1];
        return p + alignment;
    }

    static byte *Pack( byte *p, const idVec3 &v, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<idVec3> alType;
        if ( !alignment ) alignment = alType::value;
        float *pp = reinterpret_cast<float *>( p ); 
        *pp++ = v[0]; *pp++ = v[1]; *pp++ = v[2];
        return p + alignment;
    }

    static byte *Pack( byte *p, const idVec4 &v, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<idVec4> alType;
        if ( !alignment ) alignment = alType::value;
        float *pp = reinterpret_cast<float *>( p ); 
        *pp++ = v[0]; *pp++ = v[1]; *pp++ = v[2]; *pp++ = v[3];
        return p + alignment;
    }

    static byte *Pack( byte *p, const idMat2 &m, int alignment = 0 ) {
        typedef typename AlignmentHelper::template GetSize<idMat2> alType;
        if ( !alignment ) alignment = alType::value;
        float *pp = reinterpret_cast<float *>( p ); 
        *pp++ = m[0][0]; *pp++ = m[0][1]; pp++; pp++;
        *pp++ = m[1][0]; *pp++ = m[1][1];
        return p + alignment;
    }
};

struct Std140OpenglAlignmentHelper {

    template <typename T, int n = 0>
    struct AHelper {
        static const int size0 = Std140OpenglAlignmentHelper::AHelper<T>::size;
        static const int alignment = 16;
        static const int size = GL_ROUNDUP(size0, 16) * n;
    };

    template <typename T, int arrSize = 0>
    struct GetAlignment { 
        static const int value = AHelper<T, arrSize>::alignment;
    };

    template <typename T, int arrSize = 0>
    struct GetSize { 
        static const int value = AHelper<T, arrSize>::size;
    };
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<GLint, 0> {
    static const int alignment = sizeof(GLint);
    static const int size = sizeof(GLint);
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<float, 0> {
    static const int alignment = sizeof(float);
    static const int size = sizeof(float);
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<idVec2, 0> {
    static const int alignment = sizeof(float) * 2;
    static const int size = sizeof(float) * 2;
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<idVec3, 0> {
    static const int alignment = sizeof(float) * 3;
    static const int size = sizeof(float) * 3;
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<idVec4, 0> {
    static const int alignment = sizeof(float) * 4;
    static const int size = sizeof(float) * 4;
};

template <>
struct Std140OpenglAlignmentHelper::AHelper<idMat2, 0> 
    : public Std140OpenglAlignmentHelper::AHelper<idVec2, 2>
{};

typedef GLSLPackHelper<Std140OpenglAlignmentHelper> Std140GLSLPacker;

#endif /* GLSL_PACK_HELPER__H_ */
