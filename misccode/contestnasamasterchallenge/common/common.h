
#ifndef _COMMON_DEF_
#define _COMMON_DEF_

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <algorithm>
#include <math.h>
#include <numeric>
#include "MyMath.h"


#define	CLAMP(f,min,max)	((f)<(min)?(min):(f)>(max)?(max):(f))
#define MYEPS 1e-9

struct KMLBounds
{
   double m_minLat, m_maxLat;
   double m_minLon, m_maxLon;

   double GetLatLen() const { return m_maxLat - m_minLat; }
   double GetLonLen() const { return m_maxLon - m_minLon; }
   cv::Point2d GetCenter() const
   {
      cv::Point2d res;
      res.x = (m_minLon + m_maxLon) / 2.0;
      res.y = (m_minLat + m_maxLat) / 2.0;
      return res;
   }
   double GetResolutionX(const cv::Mat & img) const 
   { 
      return GetLonLen() / img.cols;
   }
   double GetResolutionY(const cv::Mat & img) const 
   { 
      return GetLatLen() / img.rows;
   }
   cv::Point2i ToCoordinates(const cv::Mat & img, const cv::Point2d & lonLat) const
   {
      cv::Point2i res;
      res.x = int((lonLat.x - m_minLon) / GetResolutionX(img));
      res.y = int((lonLat.y - m_minLat) / GetResolutionY(img));
      return res;
   }
   cv::Point2d ToLonLat(const cv::Mat & img, const cv::Point2i & coord) const
   {
      cv::Point2d res;
      res.x = m_minLon + coord.x * GetResolutionX(img);
      res.y = m_minLat + coord.y * GetResolutionY(img);
      return res;
   }
};

KMLBounds myParseKML (const std::string& kml_file);

struct MyLine
{
   cv::Point2d m_point;
   cv::Vec2d   m_vec;
   static MyLine FromPoints(const cv::Point2d & p1, const cv::Point2d & p2) 
   {
      MyLine res;
      res.m_point = p1;
      res.m_vec = cv::Point2d(p2.x-p1.x,p2.y-p1.y);
      res.Normalize();
      return res;
   }
   static MyLine FromPointAndVector(const cv::Point2d & p1, const cv::Vec2d & v) 
   {
      MyLine res;
      res.m_point = p1;
      res.m_vec = v;
      res.Normalize();
      return res;
   }
   bool OnCounterClockwiseSide(const cv::Point2d & p) const
   {
      MyLine oth = MyLine::FromPoints(m_point, p);
      return Cross(oth) > 0;
   }
   double Distance(const cv::Point2d & p) const 
   {
      cv::Vec2d yp = cv::Vec2d(p.x - m_point.x, p.y - m_point.y);
      double othSide = m_vec.dot(yp);
      return sqrt(yp.dot(yp) - othSide*othSide);
   }
   cv::Vec2d ToNormalizedVec() const { return m_vec; }
   cv::Vec2d ToNormalizedPerpendVec() const { return cv::Vec2d(-m_vec[1], m_vec[0]); }
   void Normalize() 
   { 
      double n = sqrt(m_vec.dot(m_vec)); 
      if (abs(n) < MYEPS) {
         m_vec[0] = m_vec[1] = 0.0;
      } else {
         m_vec[0] /= n;
         m_vec[1] /= n;
      }
   }
   double Cross(const MyLine & oth) const
   { 
      return m_vec[0]*oth.m_vec[1] - m_vec[1]*oth.m_vec[0];
   }
   double Cosine(const MyLine & oth) const { return m_vec.dot(oth.m_vec); }
};

struct CorrWindowSize
{
   int m_deltaX0, m_deltaX1;
   int m_deltaY0, m_deltaY1;
};



void myShowImage(const std::string & image);


template <typename T>
double myMean(const std::vector<T> &t) { 
   if (t.empty()) return 0.0;
   return std::accumulate(t.begin(), t.end(), 0.0) / (double)t.size(); 
}

template <typename T>
double myStdDev(const std::vector<T> &t, double mprecomp=0.0) { 
   if (t.empty()) return 0.0;
   double mn = mprecomp == 0.0 ? myMean(t) : mprecomp;
   double res = std::accumulate(t.begin(), t.end(), 0.0, [mn](double acc, double v) { return acc + (mn-v)*(mn-v); });
   return sqrt(res / (double)t.size());
}

template <typename T>
double myMean(const cv::Mat & m, const cv::Rect & r) {
   std::vector<double> t;
   for (int i = r.y; i < r.y + r.height; ++i) {
      for (int j = r.x; j < r.x + r.width; ++j) {
         if (i>= 0 && j>=0 && i<m.rows && j<m.cols)
         t.push_back((double)m.at<T>(i,j));
      }
   }
   return myMean(t);
}

template <typename T>
double myStdDev(const cv::Mat & m, const cv::Rect & r, double mprecomp=0.0) {
   std::vector<double> t;
   for (int i = r.y; i < r.y + r.height; ++i) {
      for (int j = r.x; j < r.x + r.width; ++j) {
         if (i>= 0 && j>=0 && i<m.rows && j<m.cols)
         t.push_back((double)m.at<T>(i,j));
      }
   }
   return myStdDev(t,mprecomp);
}


inline bool myIsInsideMat(const cv::Mat & m, const cv::Point2i & p)
{
   return p.x >= 0 && p.y >= 0 && p.x < m.cols && p.y < m.rows;
}


inline bool myIsInsideMat(const cv::Mat & m, const cv::Point2d & p)
{
   return myIsInsideMat(m, cv::Point2i((int)p.x, (int)p.y));
}


inline cv::Point2i myInterpolatePoints(const cv::Point2i & from, const cv::Point2i & to, double t)
{
   cv::Point2i res;
   res.x = from.x + int(t * double(to.x - from.x));
   res.y = from.y + int(t * double(to.y - from.y));
   return res;
}

class MyCmdArgsImage {
public:

   void Init(const std::string & baseName, const std::string &idir, const std::string &rdir, const std::string &odir);
   std::string GetInputImageName(const std::string & ext="tif") const;
   std::string GetOutputImageName(const std::string & ext="jpg") const;
   std::string GetInputRpcName() const;

private:
   std::string m_baseImageName;
   std::string m_inputPathDir;
   std::string m_rpcPathDir;
   std::string m_outputPathDir;
};


class MyCmdArgs {
public:

   void Init(const std::string & kmlPath, const std::string & imagesPath, const std::string & rpcPath,
      const std::string & outPath, const std::string &ext="tif");

   std::string GetKmlPath() const { return m_kmlPath; }
   MyCmdArgsImage GetImageByIndex(int idx) { return m_argImages[idx]; }
   int GetImageCount() const { return (int)m_argImages.size(); }

   std::string GetOutputPath(const std::string & name) const { return m_outPath + "/" + name; }

   // in meters
   double GetOutResolutionX() const { return m_resolutionX; }
   double GetOutResolutionY() const { return m_resolutionY; }

private:
   std::string m_kmlPath;
   std::vector<MyCmdArgsImage> m_argImages;
   std::string m_outPath;
   double m_resolutionX, m_resolutionY;
};

double metricToGeodetic(const KMLBounds & bounds, double meters, bool isX);

void common_init();

#endif
