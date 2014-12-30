#ifndef GLSL_PACK_HELPER__H_
#define GLSL_PACK_HELPER__H_

template <typename AlignmentHelper>
class GLSLPackHelper {
public:
    typedef AlignmentHelper align_type;

    static byte *Pack( byte *p, GLint v, int alignment = 0 ) {
        if ( !alignment ) alignment = AlignmentHelper::GetAlignment<GLint>();
        GLint *pp = static_cast<GLint *>( p ); *pp = v;
        return p + alignment;
    }

    static byte *Pack( byte *p, float v, int alignment = 0 ) {
        if ( !alignment ) alignment = AlignmentHelper::GetAlignment<GLint>();
        float *pp = static_cast<float *>( p ); *pp = v;
        return p + alignment;
    }

    static byte *Pack( byte *p, const idVec3 &v, int alignment = 0 ) {
        if ( !alignment ) alignment = AlignmentHelper::GetAlignment<idVec3>();
        float *pp = static_cast<float *>( p ); 
        *pp++ = vec[0]; *pp++ = vec[1]; *pp++ = vec[2];
        return p + alignment;
    }

    static byte *Pack( byte *p, const idVec4 &v, int alignment = 0 ) {
        if ( !alignment ) alignment = typename AlignmentHelper::GetAlignment<idVec4>();
        float *pp = static_cast<float *>( p ); 
        *pp++ = vec[0]; *pp++ = vec[1]; *pp++ = vec[2]; *pp++ = vec[3];
        return p + alignment;
    }

    static byte *Pack( byte *p, const idMat2 &m, int alignment = 0 ) {
        if ( !alignment ) alignment = typename AlignmentHelper::GetAlignment<idMat2>();
        float *pp = static_cast<float *>( p ); 
        *pp++ = m[0][0]; *pp++ = m[0][1]; pp++; pp++;
        *pp++ = m[1][0]; *pp++ = vec[1][1];
        return p + alignment;
    }
};

struct Std140OpenglAlignmentHelper {

    template <typename T, int arrSize = 0>
    struct AHelper;

    template <typename T, int arrSize = 0>
    static int GetAlignment() { return typename AHelper<T, arrSize>::alignment; }

    template <typename T, int arrSize = 0>
    static int GetSize() { return typename AHelper<T, arrSize>::size; }
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

#define ROUNDUP(x,n) (!((x) % (n)) ? (x) : ((x) + (n) - ((x) % (n)))

template <typename T, int n>
struct Std140OpenglAlignmentHelper::AHelper {
    static const int alignment 
        = ROUNDUP(typename Std140OpenglAlignmentHelper::AHelper<T>::size, 16);
    static const int size = alignment * n;
};

typedef GLSLPackHelper<Std140OpenglAlignmentHelper> Std140GLSLPacker;

#endif /* GLSL_PACK_HELPER__H_ */
