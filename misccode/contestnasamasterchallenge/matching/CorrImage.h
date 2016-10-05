#ifndef CORR_IMAGE_DEF__
#define CORR_IMAGE_DEF__

#include "common.h"
#include "RPCModel.h"


class MyCorrImage;


struct MyCorrWindow
{
   MyPoint2d p1, p2, p3, p4, pcenter;
   int pointsX, pointsY;

   MyCorrWindow() : pointsX(1), pointsY(1) {}

   double GetBilinear(int idx, double t1, double t2) const {
      return (1-t1)*(1-t2)*p1.p[idx] 
         + t1*(1-t2)*p2.p[idx] 
         + t1*t2*p3.p[idx] 
         + (1-t1)*t2*p4.p[idx];
   }

   MyPoint2d GetInterpPoint(double t1, double t2) const {
      MyPoint2d res;
      res.p[0] = GetBilinear(0, t1, t2);
      res.p[1] = GetBilinear(1, t1, t2);
      return res;
   }
   int GetWidth() const { return (int)abs(p2.p[0] - p1.p[0]); }
   int GetHeight() const { return (int)abs(p4.p[1] - p1.p[1]); }
   int GetPointsX() const { return pointsX; }
   int GetPointsY() const { return pointsY; }

   MyPoint2d GetCenter() const;
   MyVector2d GetVectorX() const;
   MyVector2d GetVectorY() const;

   void InitFromCenter(const MyPoint2d & center, const CorrWindowSize & wSize);
   void ProjectToCamera(double h, const RPCCamera & refCamera, const RPCCamera & searchCamera);
   void RecenterProjected(const MyPoint2d & newCenter);
};


class MyCorrImage
{
public:

   MyCorrImage(void) : m_mostInclined(false) {}

   MyCorrImage(const KMLBounds & bounds, const RPCCamera & camera, const cv::Mat & inp) 
      : m_bounds(bounds), m_camera(camera), m_image(inp), m_cachedResolution(-1.0), m_mostInclined(false)
   {}

   bool CrossCorrelation(const MyCorrWindow &wmy, const MyCorrImage &ref, 
      const MyCorrWindow &wref, double &ncc) const;

   bool CrossCorrelation(const MyCorrWindow &wmy, const MyLine & myLine, const MyCorrImage &ref, 
      const MyCorrWindow &wref, const MyLine & refLine, double &ncc) const;

   // in meters per pixel (min(x,y))
   double Resolution() const;
   double ResolutionX() const;
   double ResolutionY() const;

   const cv::Mat & GetImage() const { return m_image; }

   const RPCCamera & GetCamera() const { return m_camera; }

   cv::Point2d GetLonLatByPoint(int row, int col) const;

   MyLine GetEpiLine() const { return m_epiLine; }

   void ComputeEpiLine();

   bool IsMostInclined() const { return m_mostInclined; }
   void SetMostInclined(bool v) { m_mostInclined = v; }


private:
   double GetIntensity(const MyPoint2d &p) const;
   bool IsInside(const MyPoint2d &p) const;

   KMLBounds m_bounds;
   RPCCamera m_camera;
   cv::Mat m_image;
   mutable double m_cachedResolution;
   MyLine m_epiLine;
   bool m_mostInclined;
};



#endif