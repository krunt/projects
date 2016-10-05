#ifndef POINT_CLOUD_DEF__
#define POINT_CLOUD_DEF__

#include "common.h"

class MyPointCloud
{
public:

   MyPointCloud(void) {}

   MyPointCloud(const KMLBounds & bounds, double resolutionX, double resolutionY)
      : m_bounds(bounds), m_resolutionX(resolutionX), m_resolutionY(resolutionY)
   {
      int width = int(m_bounds.GetLonLen() / m_resolutionX);
      int height = int(m_bounds.GetLatLen() / m_resolutionY);
      m_image = cv::Mat::zeros(height, width, CV_64FC1);
   }

   bool ToCoordinates(double lon, double lat, cv::Point2i & res) const 
   {
      if (lon < m_bounds.m_minLon || lon > m_bounds.m_maxLon) return false;
      if (lat < m_bounds.m_minLat || lat > m_bounds.m_maxLat) return false;
      res = m_bounds.ToCoordinates(m_image, cv::Point2d(lon, lat));
      return true;
   }

   // in meters
   double GetAltitude(double lon, double lat) const 
   {
      cv::Point2i pnt;
      if (!ToCoordinates(lon, lat, pnt)) {
         return 1e30;
      }
      return m_image.at<double>(pnt);
   }

   void SetAltitude(int row, int col, double val) 
   {
      m_image.at<double>(row, col) = val;
   }

   int GetWidth() const { return m_image.cols; }
   int GetHeight() const { return m_image.rows; }

private:
   cv::Mat m_image;
   KMLBounds m_bounds;
   double m_resolutionX;
   double m_resolutionY;
};


#endif