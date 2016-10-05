
#ifndef _MATH__DEF_
#define _MATH__DEF_

struct MyVector2d
{
    double p[2];
    MyVector2d(void) {}
    MyVector2d(double v1, double v2) {
        p[0] = v1; p[1] = v2;
    }
};

struct MyVector3d
{
    double p[3];
    MyVector3d(void) {}
    MyVector3d(double v1, double v2, double v3) {
        p[0] = v1; p[1] = v2; p[2] = v3;
    }
    MyVector3d operator-(void) const {
        MyVector3d res(-p[0], -p[1], -p[2]);
        return res;
    }
};

typedef MyVector2d MyPoint2d;
typedef MyVector3d MyPoint3d;

inline void MyVectorNormalize(MyVector2d &v)
{
   double n = sqrt(v.p[0] * v.p[0] + v.p[1] * v.p[1]);
   if (n != 0) {
      v.p[0] /= n;
      v.p[1] /= n;
   }
}

inline void MyVectorNormalize(MyVector3d &v)
{
    double n = sqrt(v.p[0] * v.p[0] + v.p[1] * v.p[1] + v.p[2] * v.p[2]);
    if (n != 0) {
       v.p[0] /= n;
       v.p[1] /= n;
       v.p[2] /= n;
    }
}

inline double MyVectorDot(const MyVector3d &v1, const MyVector3d &v2)
{
    return v1.p[0] * v2.p[0] + v1.p[1] * v2.p[1] + v1.p[2] * v2.p[2];
}

inline MyVector3d MyVectorSub(const MyVector3d &v1, const MyVector3d &v2)
{
    MyVector3d res;
    res.p[0] = v1.p[0] - v2.p[0];
    res.p[1] = v1.p[1] - v2.p[1];
    res.p[2] = v1.p[2] - v2.p[2];
    return res;
}

#endif
