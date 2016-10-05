#ifndef TIN_STRUCTURE_DEF__
#define TIN_STRUCTURE_DEF__

#include "common.h"
#include "PointCloud.h"

#ifdef SINGLE
#define REAL float
#else /* not SINGLE */
#define REAL double
#endif /* not SINGLE */


extern "C" {
#include "triangle.h"
}

struct TinPoint {
   TinPoint(const cv::Point3d & pnt, bool lieOnSegment) : m_pnt(pnt), m_lieOnSegment(lieOnSegment) {}
   cv::Point3d m_pnt;
   bool        m_lieOnSegment;
};

class TinStructure {
public:
   TinStructure() : m_meshStruct(NULL), m_behavStruct(NULL)
   {}

   void Construct(const std::vector<cv::Point3d> & pnts, const std::vector<std::pair<int,int>> & edges);
   // lon/lat
   bool GetTriangleByPoint(const cv::Point2d & pnt, std::vector<TinPoint> & triangle);
   void Interpolate(MyPointCloud & out);

private:
   triangulateio m_structure;
   mesh * m_meshStruct;
   behavior * m_behavStruct;
   std::vector<cv::Point3d> m_pnts;
   std::vector<std::pair<int,int>> m_edges;
};


#endif