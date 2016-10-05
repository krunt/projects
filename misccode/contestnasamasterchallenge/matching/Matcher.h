#ifndef MY_MATCHER_DEF__
#define MY_MATCHER_DEF__

#include "common.h"
#include "PointCloud.h"
#include "CorrImage.h"
#include "TinStructure.h"

#include "nr3.h"
#include "interp_1d.h"
#include "pointbox.h"
#include "Kdtree.h"


struct Gc3Result;

struct MyFeatureEdge
{
   cv::Point2i m_p[2];
   MyLine m_line;
   cv::Vec2d m_orient;
   bool m_sign;
   std::vector<cv::Point2i> m_points;

   // relaxation
   std::vector<Gc3Result *> m_relaxationEdgePoints;

   cv::Point2i GetDominantPoint(int idx) const 
   {
      return m_p[idx];
   }
   double GetLength() const
   {
      cv::Vec2d v = cv::Vec2d(m_p[1].x-m_p[0].x,m_p[1].y-m_p[1].y);
      return sqrt(v.dot(v));
   }
   cv::Point2d Interpolate(double t) const
   {
      return cv::Point2d(m_p[0].x+t*(m_p[1].x-m_p[0].x), m_p[0].y+t*(m_p[1].y-m_p[1].y));
   }
};


struct Gc3Candidate
{
   Gc3Candidate() : m_relaxPrevProb(0.0), m_relaxProb(0.0) {}

   double      m_sncc;
   double      m_snccSlope;
   cv::Point3d m_worldPoint;
   std::vector<cv::Point2i> m_searchPoints;

   // for relaxation
   double m_relaxPrevProb, m_relaxProb;

   double GetSncc() const { return m_sncc; }
   double GetSnccSlope() const { return m_snccSlope; }

   double GetReliability() const 
   {
      return  0.6 * GetSncc() + 0.4 * GetSnccSlope(); 
   }

   bool operator<(const Gc3Candidate & other) const 
   {
      return GetSncc() < other.GetSncc();
   }

   static double GetSlopeResult(double deltaZ, double deltaSncc)
   {
      return atan(deltaSncc / deltaZ) / (M_PI / 2);
   }

   void Serialize(std::fstream & fs);
   void Deserialize(std::fstream & fs);
};


struct Gc3Result
{
   Gc3Result() : m_isRelaxationFinished(false), m_bestCandidateIdx(-1), m_edgeIndex(-1), 
      m_edgePointIdx(-1) {}

   CorrWindowSize m_bestWindow;
   cv::Point2i m_refPoint;
   std::vector<Gc3Candidate> m_candidates;
   int m_edgeIndex;

   // for relaxation
   std::vector<Gc3Result *> m_refCandidates;
   bool m_isRelaxationFinished;
   int m_bestCandidateIdx;
   int m_edgePointIdx;

   bool IsPartOfEdge() const 
   {
      return m_edgeIndex != -1;
   }

   const Gc3Candidate & GetBestCandidate() const
   {
      assert(m_isRelaxationFinished);
      return m_candidates[m_bestCandidateIdx];
   }

   double GetSnccRatio() const 
   { 
      if (m_candidates.size() == 1) {
         return 1.0;
      }

      //if (m_candidates.empty() || m_candidates[m_candidates.size()-2].GetSncc() == 0) {
      if (m_candidates.empty() || m_candidates.front().GetSncc() == 0) {
         return -1.0;
      }

      return m_candidates.back().GetSncc() / m_candidates.front().GetSncc();
   }

   void Serialize(std::fstream & fs);
   void Deserialize(std::fstream & fs);
};





class MyMatcher
{
public:
   MyMatcher(void) : m_referenceImageIndex(-1), m_mostInclinedImageIndex(-1), m_kdTree(NULL) {}

   void Init(const MyCmdArgs & args);
   void InitNewLevel(
      int levelIndex, double scale,
      const std::vector<cv::Mat> &images,
      std::vector<RPCCamera> &cameras);

   void Match();
   void MatchOneLevel();
   void MatchPointFeatures();
   void MatchEdgeFeatures();

   // version for points
   bool MatchGc3(const cv::Point2i & pnt, Gc3Result & res);

   // version for points along edge
   bool MatchGc3(const cv::Point2i & pnt, double t,
      const MyFeatureEdge & edge,
      const Gc3Result & res0, 
      const Gc3Result & res1, Gc3Result & res);

private:
   double GetZStep() const;
   double GetAltitude(const MyPointCloud & cloud, const MyCorrImage &img, const cv::Point2i & pnt) const;
   void BuildKdTreeFromPointFeatures();
   void PerformMatchPointRelaxation();
   Gc3Result * & GetPResultFromCache(int y, int x) { return m_fPointCache[y * m_refImage.GetImage().cols + x]; }
   void FreeKdTree() { delete m_kdTree; m_kdTree = NULL; }
   void InitNewLevel();
   int ChooseReferenceImage(const std::vector<RPCCamera> & cams) const;
   int ChooseMostInclinedImage(const std::vector<RPCCamera> & cams) const;
   void ConstructTinStructure(const MyPointCloud & cloud);
   void DebugShowGc3Result(const std::vector<Gc3Result *> & res);
   void LoadIntermediateGc3Results(const std::string & fname);
   void SaveIntermediateGc3Results(const std::string & fname);
   
   MyCmdArgs m_cmdArgs;
   std::vector<cv::Mat> m_fsearchImageRotMatrices;
   KMLBounds m_kmlBounds;
   int m_referenceImageIndex;
   int m_mostInclinedImageIndex;

   MyCorrImage m_refImage;
   std::vector<MyCorrImage> m_searchImages;

   std::vector<cv::Point2i> m_fPoints;
   std::vector<MyFeatureEdge> m_fRefEdges;
   std::vector<Gc3Result *> m_fPointCache;

   MyPointCloud m_prevCloud;
   
   TinStructure m_tinPrev;
   TinStructure m_tinCurr;

   std::vector<std::vector<MyFeatureEdge>> m_fsearchImageEdges;
   std::vector<cv::Mat> m_fsearchImageEdgesIndices;
   
   KDtree<2> * m_kdTree;
};


#endif