
#include "CorrImage.h"


bool MyCorrImage::CrossCorrelation(const MyCorrWindow &wmy, const MyCorrImage &ref, 
                      const MyCorrWindow &wref, double &ncc) const
{
   bool b1 = IsInside(wmy.p1) && IsInside(wmy.p2) && 
      IsInside(wmy.p3) && IsInside(wmy.p4);
   bool b2 = ref.IsInside(wref.p1) && ref.IsInside(wref.p2) && 
      ref.IsInside(wref.p3) && ref.IsInside(wref.p4);

   if (!b1 || !b2) {
      return false;
   }

   double i0 = 0, imy = 0;

   double pointsX = wmy.GetPointsX();
   double pointsY = wmy.GetPointsY();
   double wszx = pointsX - 2;
   double wszy = pointsY - 2;

   for (int i = 1; i < pointsY; ++i) {
      for (int j = 1; j < pointsX; ++j) {
         MyPoint2d refp1 = wref.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         MyPoint2d myp2 = wmy.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         i0 += ref.GetIntensity(refp1);
         imy += GetIntensity(myp2);
      }
   }

   i0 /= (wszx * wszy);
   imy /= (wszx * wszy);

   if (i0 < MYEPS || imy < MYEPS) {
      return false;
   }

   double pnum = 0, pden1 = 0, pden2 = 0;
   for (int i = 1; i < pointsY; ++i) {
      for (int j = 1; j < pointsX; ++j) {
         MyPoint2d refp1 = wref.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         MyPoint2d myp2 = wmy.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         double vref = ref.GetIntensity(refp1);
         double vmy  = GetIntensity(myp2);

         pnum += (vref - i0) * (vmy - imy);
         pden1 += (vref - i0) * (vref - i0);
         pden2 += (vmy - imy) * (vmy - imy);
      }
   }

   pden1 = sqrt(pden1);
   pden2 = sqrt(pden2);

   if (pden1 < MYEPS || pden2 < MYEPS) {
      return false;
   }

   ncc = pnum / (pden1 * pden2);

   return true;
}


bool MyCorrImage::CrossCorrelation(const MyCorrWindow &wmy, const MyLine & myLine, const MyCorrImage &ref, 
                      const MyCorrWindow &wref, const MyLine & refLine, double &ncc) const
{
   bool b1 = IsInside(wmy.p1) && IsInside(wmy.p2) && 
      IsInside(wmy.p3) && IsInside(wmy.p4);
   bool b2 = ref.IsInside(wref.p1) && ref.IsInside(wref.p2) && 
      ref.IsInside(wref.p3) && ref.IsInside(wref.p4);

   if (!b1 || !b2) {
      return false;
   }

   double i0[2] = {0,0};
   double imy[2] = {0,0};
   int n0[2] = {0,0};
   int nmy[2] = {0,0};

   double pointsX = wmy.GetPointsX();
   double pointsY = wmy.GetPointsY();

   for (int i = 1; i < pointsY; ++i) {
      for (int j = 1; j < pointsX; ++j) {
         MyPoint2d refp1 = wref.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         MyPoint2d myp2 = wmy.GetInterpPoint((double)j/pointsX, (double)i/pointsY);

         int refSign = (int)refLine.OnCounterClockwiseSide(cv::Point2d(refp1.p[0],refp1.p[1]));
         n0[refSign]++;
         i0[refSign] += ref.GetIntensity(refp1);

         int mySign = (int)myLine.OnCounterClockwiseSide(cv::Point2d(myp2.p[0],myp2.p[1]));
         nmy[refSign]++;
         imy[refSign] += GetIntensity(myp2);
      }
   }

   for (int i = 0; i < 2; ++i) {
      if (n0[i] != 0) {
         i0[i] /= n0[i];
      }
      if (nmy[i] != 0) {
         imy[i] /= nmy[i];
      }
   }

   double pnum[2] = {0,0};
   double pden1[2] = {0,0};
   double pden2[2] = {0,0};

   for (int i = 1; i < pointsY; ++i) {
      for (int j = 1; j < pointsX; ++j) {
         MyPoint2d refp1 = wref.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         MyPoint2d myp2 = wmy.GetInterpPoint((double)j/pointsX, (double)i/pointsY);
         double vref = ref.GetIntensity(refp1);
         double vmy  = GetIntensity(myp2);

         int refSign = (int)refLine.OnCounterClockwiseSide(cv::Point2d(refp1.p[0],refp1.p[1]));
         int mySign = (int)myLine.OnCounterClockwiseSide(cv::Point2d(myp2.p[0],myp2.p[1]));

         // correctness check
         if (refSign != mySign) {
            continue;
         }

         int sign = refSign;

         pnum[sign] += (vref - i0[sign]) * (vmy - imy[sign]);
         pden1[sign] += (vref - i0[sign]) * (vref - i0[sign]);
         pden2[sign] += (vmy - imy[sign]) * (vmy - imy[sign]);
      }
   }

   for (int i = 0; i < 2; ++i) {
      pden1[i] = sqrt(pden1[i]);
      pden2[i] = sqrt(pden2[i]);

      if (pden1[i] < MYEPS || pden2[i] < MYEPS) {
         return false;
      }
   }

   ncc = std::max(pnum[0] / (pden1[0] * pden2[0]), pnum[1] / (pden1[1] * pden2[1]));
   return true;
}


double MyCorrImage::ResolutionX() const 
{
   RPCModel model = m_camera.GetRpcModel();

   double minx, miny, minh;
   double maxx, maxy, maxh;
   geodetic_to_cartesian(m_bounds.m_minLon, m_bounds.m_minLat, 0, minx, miny, minh);
   geodetic_to_cartesian(m_bounds.m_maxLon, m_bounds.m_maxLat, 0, maxx, maxy, maxh);

   return abs(minx - maxx) / (double)m_image.cols;
}


double MyCorrImage::ResolutionY() const 
{
   RPCModel model = m_camera.GetRpcModel();

   double minx, miny, minh;
   double maxx, maxy, maxh;
   geodetic_to_cartesian(m_bounds.m_minLon, m_bounds.m_minLat, 0, minx, miny, minh);
   geodetic_to_cartesian(m_bounds.m_maxLon, m_bounds.m_maxLat, 0, maxx, maxy, maxh);

   return abs(miny - maxy) / (double)m_image.rows;
}

// in meters per pixel (min(x,y))
double MyCorrImage::Resolution() const 
{
   if (m_cachedResolution < 0) {
      // here is max not min (todo: think about oy resolution)
      m_cachedResolution = std::max(ResolutionX(), ResolutionY());
   }

   return m_cachedResolution;
}


double MyCorrImage::GetIntensity(const MyPoint2d &p) const 
{
   int x = (int)p.p[0]; 
   int y = (int)p.p[1];
   return (double)m_image.at<uchar>(y, x);
}

bool MyCorrImage::IsInside(const MyPoint2d &p) const
{
   if (p.p[0] < 0 || p.p[0] >= m_image.cols
      || p.p[1] < 0 || p.p[1] >= m_image.rows)
      return false;
   return true;
}

void MyCorrImage::ComputeEpiLine() 
{
   cv::Point2d center = m_bounds.GetCenter();

   cv::Point3d from(center.x, center.y, 0);
   cv::Point3d to(center.x, center.y, 10000);

   cv::Point2d fromP, toP;
   GetCamera().ToPixelCoordinates(from.x, from.y, from.z, fromP.x, fromP.y);
   GetCamera().ToPixelCoordinates(to.x, to.y, to.z, toP.x, toP.y);

   m_epiLine = MyLine::FromPoints(cv::Point2i((int)fromP.x, (int)fromP.y), cv::Point2i((int)toP.x, (int)toP.y));
}

cv::Point2d MyCorrImage::GetLonLatByPoint(int row, int col) const 
{
   return m_bounds.ToLonLat(m_image, cv::Point2i(col, row));
}

void MyCorrWindow::InitFromCenter(const MyPoint2d & center, const CorrWindowSize & wSize)
{
   pcenter = center;

   p1.p[0] = center.p[0] + wSize.m_deltaX0;
   p1.p[1] = center.p[1] + wSize.m_deltaY0;

   p2.p[0] = center.p[0] + wSize.m_deltaX1;
   p2.p[1] = center.p[1] + wSize.m_deltaY0;

   p3.p[0] = center.p[0] + wSize.m_deltaX1;
   p3.p[1] = center.p[1] + wSize.m_deltaY1;

   p4.p[0] = center.p[0] + wSize.m_deltaX0;
   p4.p[1] = center.p[1] + wSize.m_deltaY1;

   pointsX = wSize.m_deltaX1 - wSize.m_deltaX0 + 1;
   pointsY = wSize.m_deltaY1 - wSize.m_deltaY0 + 1;
}

void MyCorrWindow::ProjectToCamera(double h, const RPCCamera & refCamera, const RPCCamera & searchCamera)
{
   cv::Point2d lonLatP1, lonLatP2, lonLatP3, lonLatP4, lonLatPCenter;
   refCamera.ToGeodetic(p1.p[0], p1.p[1], h, lonLatP1.x, lonLatP1.y);
   refCamera.ToGeodetic(p2.p[0], p2.p[1], h, lonLatP2.x, lonLatP2.y);
   refCamera.ToGeodetic(p3.p[0], p3.p[1], h, lonLatP3.x, lonLatP3.y);
   refCamera.ToGeodetic(p4.p[0], p4.p[1], h, lonLatP4.x, lonLatP4.y);
   refCamera.ToGeodetic(pcenter.p[0], pcenter.p[1], h, lonLatPCenter.x, lonLatPCenter.y);

   searchCamera.ToPixelCoordinates(lonLatP1.x, lonLatP1.y, h, p1.p[0], p1.p[1]);
   searchCamera.ToPixelCoordinates(lonLatP2.x, lonLatP2.y, h, p2.p[0], p2.p[1]);
   searchCamera.ToPixelCoordinates(lonLatP3.x, lonLatP3.y, h, p3.p[0], p3.p[1]);
   searchCamera.ToPixelCoordinates(lonLatP4.x, lonLatP4.y, h, p4.p[0], p4.p[1]);
   searchCamera.ToPixelCoordinates(lonLatPCenter.x, lonLatPCenter.y, h, pcenter.p[0], pcenter.p[1]);
}

MyPoint2d MyCorrWindow::GetCenter() const 
{
   return pcenter;
}

void MyCorrWindow::RecenterProjected(const MyPoint2d & newCenter)
{
   MyPoint2d currCenter = GetCenter();

   MyPoint2d delta;
   delta.p[0] = newCenter.p[0] - currCenter.p[0];
   delta.p[1] = newCenter.p[1] - currCenter.p[1];

   p1.p[0] += delta.p[0];
   p1.p[1] += delta.p[1];

   p2.p[0] += delta.p[0];
   p2.p[1] += delta.p[1];

   p3.p[0] += delta.p[0];
   p3.p[1] += delta.p[1];

   p4.p[0] += delta.p[0];
   p4.p[1] += delta.p[1];
}

MyVector2d MyCorrWindow::GetVectorX() const
{
   MyVector2d res;
   res.p[0] = p2.p[0] - p1.p[0];
   res.p[1] = p2.p[1] - p1.p[1];
   MyVectorNormalize(res);
   return res;
}

MyVector2d MyCorrWindow::GetVectorY() const
{
   MyVector2d res;
   res.p[0] = p4.p[0] - p1.p[0];
   res.p[1] = p4.p[1] - p1.p[1];
   MyVectorNormalize(res);
   return res;
}