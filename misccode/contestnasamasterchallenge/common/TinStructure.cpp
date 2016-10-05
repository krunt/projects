
#include  "TinStructure.h"


void TinStructure::Construct(const std::vector<cv::Point3d> & pnts, const std::vector<std::pair<int,int>> & edges)
{
   m_pnts = pnts;
   m_edges = edges;

   struct triangulateio in;
   struct triangulateio &out = m_structure;
   
   memset(&in,0,sizeof(in));
   memset(&out,0,sizeof(out));
   
   in.numberofpoints = (int)pnts.size();
   in.pointlist = (REAL*)malloc(in.numberofpoints * 2 * sizeof(REAL));
   in.numberofpointattributes = 1;
   in.pointattributelist = (REAL*)malloc(in.numberofpoints * sizeof(REAL));
   
   for (int i = 0; i < in.numberofpoints; ++i) {
      in.pointlist[i*2+0] = pnts[i].x;
      in.pointlist[i*2+1] = pnts[i].y;
      in.pointattributelist[i] = pnts[i].z;
   }
   
   in.pointmarkerlist = NULL;
   in.holelist = NULL;
   in.numberofholes = 0;
   
   in.numberofsegments = (int)edges.size();
   in.segmentlist = (int*)malloc(edges.size()*2*sizeof(int));
   in.segmentmarkerlist  = (int*)malloc(edges.size()*sizeof(int));
   
   for (int i = 0; i < edges.size(); ++i) {
      in.segmentlist[2*i+0] = edges[i].first;
      in.segmentlist[2*i+1] = edges[i].second;
      in.segmentmarkerlist[i] = i+2;
   }
   
   out.pointlist = NULL;
   out.pointattributelist = NULL;
   out.pointmarkerlist = NULL;
   out.trianglelist = NULL;
   out.neighborlist = NULL;
   out.segmentlist = NULL;
   out.segmentmarkerlist = NULL;
   out.edgelist = NULL;
   out.edgemarkerlist = NULL;

   m_meshStruct = NULL;
   m_behavStruct = NULL;
   
   triangulate("ecpz", &in, &out, NULL, &m_meshStruct, &m_behavStruct);
}


// lon/lat
bool TinStructure::GetTriangleByPoint(const cv::Point2d & pnt, std::vector<TinPoint> & triangle)
{
   REAL queryPoint[2];

   if (!m_meshStruct || !m_behavStruct) {
      return false;
   }

   queryPoint[0] = pnt.x;
   queryPoint[1] = pnt.y;

   triangle.clear();

   int outindices[3];
   if (delaunaylocate(m_meshStruct, m_behavStruct, queryPoint, outindices) == 0) {
      for (int i = 0; i < 3; ++i) {
         REAL * pt = &m_structure.pointlist[2*outindices[i]];
         REAL altitude = m_structure.pointattributelist[outindices[i]];
         bool isLieOnSegment = m_structure.pointmarkerlist[outindices[i]] > 1;
         triangle.push_back(TinPoint(cv::Point3d(pt[0],pt[1],altitude),isLieOnSegment));
      }

      return true;
   }

   return false;
}


static void CalcBarycentric(cv::Point2d p, const std::vector<TinPoint> & abc, float &u, float &v, float &w)
{
   const cv::Point3d &a = abc[0].m_pnt;
   const cv::Point3d &b = abc[1].m_pnt;
   const cv::Point3d &c = abc[2].m_pnt;

   cv::Point2f v0, v1, v2;
   v0 = cv::Point2f(float(b.x-a.x),float(b.y-a.y));
   v1 = cv::Point2f(float(c.x-a.x),float(c.y-a.y));
   v2 = cv::Point2f(float(p.x-a.x),float(p.y-a.y));
   
   float d00 = v0.dot(v0);
   float d01 = v0.dot(v1);
   float d11 = v1.dot(v1);
   float d20 = v2.dot(v0);
   float d21 = v2.dot(v1);
   float denom = d00 * d11 - d01 * d01;
   v = (d11 * d20 - d01 * d21) / denom;
   w = (d00 * d21 - d01 * d20) / denom;
   u = 1.0f - v - w;
}


void TinStructure::Interpolate(MyPointCloud & out)
{
   int ww = out.GetWidth();
   int hh = out.GetHeight();

   std::vector<TinPoint> triangle;
   for (int i = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         cv::Point2d pnt = cv::Point2d(j,i);
         if (GetTriangleByPoint(pnt, triangle)) {

            float u, v, w;
            CalcBarycentric(pnt, triangle, u, v, w);

            float h = u * (float)triangle[0].m_pnt.z 
               + v * (float)triangle[1].m_pnt.z + w * (float)triangle[2].m_pnt.z;
            out.SetAltitude(i, j, (double)h);
         }
      }
   }
}