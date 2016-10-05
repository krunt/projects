
#include "Matcher.h"



/*
cv::namedWindow("myWin");

cv::Mat img = cv::Mat::zeros(512, 512, CV_8UC3);

std::vector<Point<2>> pts;
for (int i = 0; i < 4096; ++i) {
pts.push_back(Point<2>(512.0*rand()/RAND_MAX,512.0*rand()/RAND_MAX));
}

for (int i = 0; i < pts.size(); ++i) {
cv::circle(img, cv::Point(pts[i].x[0], pts[i].x[1]), 3, cv::Scalar(255,255,255));
}

int mylist[128];
KDtree<2> kd(pts);
int nfound = kd.locatenear(Point<2>(256,256),32,mylist,128);
for (int i = 0; i < nfound; ++i) {
int ind = mylist[i];
cv::circle(img, cv::Point(pts[ind].x[0], pts[ind].x[1]), 3, cv::Scalar(255,0,0));
}

cv::imshow("myWin", img);
cv::waitKey();
*/

class CorrWindowSizeExpander
{
public:
   CorrWindowSizeExpander()
   {
      Reset();
   }

   void Reset() 
   {
      m_corrWindowSize.m_deltaX0 = -3;
      m_corrWindowSize.m_deltaX1 = 3;
      m_corrWindowSize.m_deltaY0 = -3;
      m_corrWindowSize.m_deltaY1 = 3;
      m_corrRatio = -1;
      m_prohibited = 0;
      m_iDirection = 0;
   }

   bool Expand(const Gc3Result & gc3Result)
   {
      double newRatio = gc3Result.GetSnccRatio();

      if (m_corrRatio < 0) {
         m_corrRatio = newRatio;
         m_bestResult = gc3Result;
      } else if (m_corrRatio < newRatio) {
         m_corrRatio = newRatio;
         m_bestResult = gc3Result;
      } else {
         m_prohibited |= (1 << m_iDirection);
         *GetDeltaPointer(m_iDirection) -= GetDeltaSign(m_iDirection) * 4;
      }

      int iDirection = (m_iDirection + 1) % 4;
      for (; iDirection != m_iDirection; iDirection = (iDirection + 1) % 4) {
         if (m_prohibited & (1 << iDirection)) {
            continue;
         }

         int *idir = GetDeltaPointer(iDirection);
         if (abs(*idir) >= 11) {
            continue;
         }

         break;
      }

      bool res = iDirection != m_iDirection;

      if (res) {
         m_iDirection = iDirection;
         *GetDeltaPointer(m_iDirection) += GetDeltaSign(m_iDirection) * 4;
      }

      return res;
   }

   const CorrWindowSize & GetCorrWindowSize() const { return m_corrWindowSize; }
   const Gc3Result & GetGc3Result() const { return m_bestResult; }

private:

   int * GetDeltaPointer(int iDirection) 
   {
      switch (iDirection) {
      case 0: return &m_corrWindowSize.m_deltaX1;
      case 1: return &m_corrWindowSize.m_deltaY1;
      case 2: return &m_corrWindowSize.m_deltaX0;
      case 3: return &m_corrWindowSize.m_deltaY0;
      };
      assert(0);
      return NULL;
   }

   int GetDeltaSign(int iDirection)
   {
      switch (iDirection) {
      case 0: return 1;
      case 1: return 1;
      case 2: return -1;
      case 3: return -1;
      };
      assert(0);
      return 0;
   }

   CorrWindowSize m_corrWindowSize;
   int m_prohibited;
   int m_iDirection;
   double m_corrRatio;
   Gc3Result m_bestResult;
};

static void extractForstnerFromPoint(const cv::Mat & inp, int x, int y, double & outQ, double & outW);
static void extractPointFeatures(const cv::Mat & inp, std::vector<cv::Point2i> & fPoints);
static void extractEdgeFeatures(const cv::Mat & inp, std::vector<MyFeatureEdge> & ppList);

void MyMatcher::BuildKdTreeFromPointFeatures() 
{
   std::vector<Point<2>> pnts;

   int hh = m_refImage.GetImage().rows;
   int ww = m_refImage.GetImage().cols;

   for (int i = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         Gc3Result * & r = GetPResultFromCache(i, j);
         if (r != NULL) {
            pnts.push_back(Point<2>(r->m_refPoint.x, r->m_refPoint.y));
         }
      }
   }

   FreeKdTree();

   m_kdTree = new KDtree<2>(pnts);
}


void MyMatcher::Init(const MyCmdArgs & args)
{
   m_cmdArgs = args;
   m_kmlBounds = myParseKML(args.GetKmlPath());
}


void MyMatcher::InitNewLevel(
   int levelIndex, double scale,
   const std::vector<cv::Mat> &images,
   std::vector<RPCCamera> &cameras)
{
   m_searchImages.clear();
   for (int i = 0; i < cameras.size(); ++i) {
      cameras[i].SetScale(scale,scale);
      bool mostInclined = false;
      if (i == m_mostInclinedImageIndex) {
         mostInclined = true;
      }

      if (i == m_referenceImageIndex) {
         m_refImage = MyCorrImage(m_kmlBounds, cameras[i], images[i]);

      } else {
         m_searchImages.push_back(MyCorrImage(m_kmlBounds, cameras[i], images[i]));
         m_searchImages.back().SetMostInclined(mostInclined);
      }
   }

   // compute matrices and epilines
   m_refImage.ComputeEpiLine();
   m_fsearchImageRotMatrices.clear();
   m_fsearchImageRotMatrices.resize(m_searchImages.size());
   for (int i = 0; i < m_searchImages.size(); ++i) {
      m_searchImages[i].ComputeEpiLine();

      cv::Mat m = cv::Mat::zeros(2,2,CV_64F);

      double sign = m_refImage.GetEpiLine().Cross(m_searchImages[i].GetEpiLine());
      sign = sign > 0.0 ? 1.0 : -1.0;
      double cosAngle = m_refImage.GetEpiLine().Cosine(m_searchImages[i].GetEpiLine());
      double sinAngle = sqrt(1 - cosAngle * cosAngle);

      m.at<double>(0,0) = cosAngle;
      m.at<double>(0,1) = sign * -sinAngle;
      m.at<double>(1,0) = sign *  sinAngle;
      m.at<double>(1,1) = cosAngle;

      m_fsearchImageRotMatrices[i] = m;
   }

   m_fPointCache.clear();
   m_fPoints.clear();
   m_fRefEdges.clear();

   m_tinCurr = TinStructure();
}

void MyMatcher::Match()
{
   const int kNumPyramids = 2;
   std::vector<std::vector<cv::Mat>> pyrImages;
   pyrImages.resize(1+kNumPyramids);
   for (int i = 0; i < 1+kNumPyramids; ++i) {
      pyrImages[i].resize(m_cmdArgs.GetImageCount());
      for (int j = 0; j < pyrImages[i].size(); ++j) {
         if (i == 0) {
            pyrImages[i][j] = cv::imread(m_cmdArgs.GetImageByIndex(j).GetInputImageName("jpg"), CV_LOAD_IMAGE_GRAYSCALE);
         } else {
            cv::pyrDown(pyrImages[i-1][j], pyrImages[i][j]);
         }
      }
   }

   std::vector<RPCCamera> imgCameras;
   imgCameras.resize(m_cmdArgs.GetImageCount());
   for (int i = 0; i < m_cmdArgs.GetImageCount(); ++i) {
      imgCameras[i].Init(m_cmdArgs.GetImageByIndex(i).GetInputRpcName());
   }

   //choose reference image
   m_referenceImageIndex = ChooseReferenceImage(imgCameras);
   m_mostInclinedImageIndex = ChooseMostInclinedImage(imgCameras);

   //double geodScaleX, geodScaleY;
   //{
   //   MyCorrImage corrRef(m_kmlBounds, imgCameras[m_referenceImageIndex], pyrImages[0][m_referenceImageIndex]);
   //   geodScaleX = m_kmlBounds.GetLonLen() / (pyrImages[0][m_referenceImageIndex].cols * (corrRef.ResolutionX() / m_cmdArgs.GetOutResolutionX()));
   //   geodScaleY = m_kmlBounds.GetLatLen() / pyrImages[0][m_referenceImageIndex].rows * (corrRef.ResolutionY() / m_cmdArgs.GetOutResolutionY());
   //}

   //{
   //   MyCorrImage corrRef(m_kmlBounds, imgCameras[m_referenceImageIndex], pyrImages[0][m_referenceImageIndex]);
   //   geodScaleX = corrRef.ResolutionX();
   //   geodScaleY = corrRef.ResolutionY();
   //}

   double geodScaleX = metricToGeodetic(m_kmlBounds, m_cmdArgs.GetOutResolutionX(), false);
   double geodScaleY = metricToGeodetic(m_kmlBounds, m_cmdArgs.GetOutResolutionY(), false);

   m_prevCloud = MyPointCloud(m_kmlBounds, geodScaleX, geodScaleY);
   
   double kScales[kNumPyramids+1]={1.0,0.5,0.25};
   for (int i = kNumPyramids; i>= 0; --i) {
      InitNewLevel(i, kScales[i], pyrImages[i], imgCameras);
      MatchOneLevel();

      ConstructTinStructure(m_prevCloud);

      MyPointCloud levelOut(m_kmlBounds, geodScaleX, geodScaleY);
      m_tinCurr.Interpolate(levelOut);

      m_prevCloud = levelOut;
      m_tinPrev = m_tinCurr;
   }
}

void MyMatcher::ConstructTinStructure(const MyPointCloud & cloud)
{
   int hh = m_refImage.GetImage().rows;
   int ww = m_refImage.GetImage().cols;

   std::vector<cv::Point3d> points;
   std::map<Gc3Result *,int> resMap;
   for (int i = 0, cnt = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         Gc3Result * & res = GetPResultFromCache(i, j);
         if (res != NULL) {
            resMap[res] = points.size();
            cv::Point3d p = res->GetBestCandidate().m_worldPoint;

            // convert to pixels
            cv::Point2i pr;
            assert(cloud.ToCoordinates(p.x, p.y, pr));

            p.x = (double)pr.x;
            p.y = (double)pr.y;

            points.push_back(p);
         }
      }
   }

   std::vector<std::pair<int,int>> edges;
   for (int i = 0; i < m_fRefEdges.size(); ++i) {
      const MyFeatureEdge & edge = m_fRefEdges[i];
      for (int j = 1; j < edge.m_relaxationEdgePoints.size(); ++j) {
         int from = resMap[edge.m_relaxationEdgePoints[j-1]];
         int to = resMap[edge.m_relaxationEdgePoints[j]];
         edges.push_back(std::make_pair(from,to));
      }
   }

   m_tinCurr.Construct(points, edges);
}

int MyMatcher::ChooseReferenceImage(const std::vector<RPCCamera> & cams) const
{
   int idx = -1;
   double minNadirAngle = M_PI / 2.0;
   for (int i = 0; i < cams.size(); ++i) {
      double ang = cams[i].GetNadirAngle();
      if (minNadirAngle > ang) {
         idx = i;
         minNadirAngle = ang;
      }
   }
   return idx;
}

int MyMatcher::ChooseMostInclinedImage(const std::vector<RPCCamera> & cams) const
{
   int idx = -1;
   double maxNadirAngle = 0.0;
   for (int i = 0; i < cams.size(); ++i) {
      double ang = cams[i].GetNadirAngle();
      if (maxNadirAngle < ang) {
         idx = i;
         maxNadirAngle = ang;
      }
   }
   return idx;
}

void MyMatcher::MatchOneLevel()
{
   MatchPointFeatures();
   exit(0);

   BuildKdTreeFromPointFeatures();
   PerformMatchPointRelaxation();
   FreeKdTree();

   MatchEdgeFeatures();
   PerformMatchPointRelaxation();
}


void MyMatcher::DebugShowGc3Result(const std::vector<Gc3Result*> & res)
{
   int kwidth = m_refImage.GetImage().cols;
   int kheight = m_refImage.GetImage().rows;
   cv::Mat m1=m_refImage.GetImage(),m2=m_searchImages.front().GetImage();
   //cv::resize(m_refImage.GetImage(), m1, cv::Size(kwidth, kheight));
   //cv::resize(m_searchImages[0].GetImage(), m2, cv::Size(kwidth, kheight));

   cv::Mat m3(kheight, kwidth*2, CV_8UC1);
   m1.copyTo(m3(cv::Rect(0, 0, kwidth, kheight)));
   m2.copyTo(m3(cv::Rect(kwidth, 0, kwidth, kheight)));

   for (int i = 0; i < res.size(); ++i) {
      const Gc3Candidate & cand = res[i]->m_candidates.back();
      cv::circle(m3, res[i]->m_refPoint, 3, cv::Scalar(0, 0, 0));
      cv::circle(m3, cv::Point2i(cand.m_searchPoints[0].x + kwidth, cand.m_searchPoints[0].y), 3, cv::Scalar(0, 0, 0));
      cv::line(m3, res[i]->m_refPoint, cv::Point2i(cand.m_searchPoints[0].x + kwidth, cand.m_searchPoints[0].y), cv::Scalar(0, 0, 0));
   }

   cv::namedWindow("debugWindow");
   cv::imshow("debugWindow",m3);
   cv::waitKey();
}



static bool getLineIntersection(float p0_x, float p0_y, float p1_x, float p1_y, 
                           float p2_x, float p2_y, float p3_x, float p3_y, float *i_x, float *i_y)
{
   float s1_x, s1_y, s2_x, s2_y;
   s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
   s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

   float s, t, denom;
   denom = -s2_x * s1_y + s1_x * s2_y;
   s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / denom;
   t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / denom;

   if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
   {
      // Collision detected
      if (i_x != NULL)
         *i_x = p0_x + (t * s1_x);
      if (i_y != NULL)
         *i_y = p0_y + (t * s1_y);
      return true;
   }

   return false;
}


void MyMatcher::PerformMatchPointRelaxation()
{
   assert(m_kdTree != NULL);

   std::vector<Gc3Result *> pnts;

   int hh = m_refImage.GetImage().rows;
   int ww = m_refImage.GetImage().cols;

   for (int i = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         Gc3Result * & r = GetPResultFromCache(i, j);
         if (r != NULL) {
            pnts.push_back(r);
         }
      }
   }

   const int maxPoints = 16+1;
   double searchRadius = 5.0; // * m_refImage.Resolution();
   for (int i = 0; i < pnts.size(); ++i) {
      Gc3Result & gc3Res = *pnts[i];

      if (gc3Res.m_isRelaxationFinished) {
         continue;
      }

      assert(!gc3Res.m_candidates.empty());

      for (int j = 0; j < gc3Res.m_candidates.size(); ++j) {
         gc3Res.m_candidates[j].m_relaxPrevProb
            = gc3Res.m_candidates[j].m_relaxProb = gc3Res.m_candidates[j].GetReliability();
      }

      if (gc3Res.m_candidates.size() == 1) {
         gc3Res.m_isRelaxationFinished = true;
         gc3Res.m_bestCandidateIdx = 0;
         continue;
      }


      gc3Res.m_isRelaxationFinished = false;
      gc3Res.m_bestCandidateIdx = -1;
      gc3Res.m_refCandidates.clear();

      if (gc3Res.IsPartOfEdge()) {
         int edgeIdx = gc3Res.m_edgeIndex;
         int pointIdx = gc3Res.m_edgePointIdx;

         const MyFeatureEdge & edge = m_fRefEdges[edgeIdx];

         // 5 candidates along edge
         for (int dx = 2, i = 0; i < (int)edge.m_relaxationEdgePoints.size() - 1; ++dx) {
            int offs = (dx >> 1) * ((dx&1) ? 1 : -1);
            int idx = pointIdx + offs;
            if (idx >= 0 && idx < edge.m_relaxationEdgePoints.size()) {
               gc3Res.m_refCandidates.push_back(edge.m_relaxationEdgePoints[idx]);
               if (gc3Res.m_refCandidates.size() == 5) {
                  break;
               }
            }
         }


      } else {
         int qPointsIdx[maxPoints];
         Point<2> queryPoint(gc3Res.m_refPoint.x, gc3Res.m_refPoint.y);
         int qpCount = m_kdTree->locatenear(queryPoint, searchRadius, qPointsIdx, maxPoints);

         for (int h = 0; h < qpCount; ++h) {
            if (qPointsIdx[h] == i) {
               continue;
            }

            gc3Res.m_refCandidates.push_back(pnts[qPointsIdx[h]]);
         }
      }
   }

   auto compatFunc = [&pnts,this](int i, int j, Gc3Result * hcand, int k) {
      double tconst = 1.0; // todo
      const double beta = 1.0;
      const Gc3Candidate & myCand1 = pnts[i]->m_candidates[j];
      const Gc3Candidate & myCand2 = hcand->m_candidates[k];

      {
         cv::Point2i p1, p2;
         assert(m_prevCloud.ToCoordinates(myCand1.m_worldPoint.x, myCand1.m_worldPoint.y, p1));
         assert(m_prevCloud.ToCoordinates(myCand2.m_worldPoint.x, myCand2.m_worldPoint.y, p2));

         cv::Point2d pp1(p1.x,p1.y), pp2(p2.x,p2.y), *pfrom = NULL, *pto = NULL;
         std::vector<TinPoint> tri1, tri2, *tri = NULL;
         bool b1 = m_tinPrev.GetTriangleByPoint(pp1, tri1);
         bool b2 = m_tinPrev.GetTriangleByPoint(pp2, tri2);
         if (b1) { pfrom = &pp1; pto = &pp2; tri = &tri1; }
         else if (b2) { pfrom = &pp2; pto = &pp1; tri = &tri2; }

         if (tri != NULL) {
            for (int k = 0; k < 3; ++k) {
               const TinPoint &tp1 = (*tri)[k];
               const TinPoint &tp2 = (*tri)[(k+1)%3];
               if (tp1.m_lieOnSegment && tp2.m_lieOnSegment &&
                  getLineIntersection(pp1.x,pp1.y,pp2.x,pp2.y,tp1.m_pnt.x,tp1.m_pnt.y,
                     tp2.m_pnt.x,tp2.m_pnt.y,NULL,NULL))
               {
                  tconst = 0.001;
                  break;
               }
            }
         }
      }

      cv::Point2i deltaP = pnts[i]->m_refPoint - hcand->m_refPoint;
      double dih2 = (double)deltaP.dot(deltaP);
      double dih = (double)sqrt(dih2);
      double deltaZ = myCand2.m_worldPoint.z - myCand1.m_worldPoint.z;
      return tconst / (beta * dih) * exp(- deltaZ * deltaZ / (beta * beta * dih2));
   };

   auto supportFunc = [&pnts,compatFunc](int i, int j) {
      double res = 1.0;
      for (int h = 0; h < pnts[i]->m_refCandidates.size(); ++h) {
         Gc3Result * hcand = pnts[i]->m_refCandidates[h];
         double s = 0.0;
         for (int k = 0; k < hcand->m_candidates.size(); ++k) {
            s += hcand->m_candidates[k].m_relaxPrevProb * compatFunc(i, j, hcand, k);
         }
         res *= s;
      }
      return res;
   };

   for (int iter = 0; iter < 1000; ++iter) {
      for (int i = 0; i < pnts.size(); ++i) {
         Gc3Result & gc3Res = *pnts[i];

         if (gc3Res.m_isRelaxationFinished) {
            continue;
         }

         double denom = 0.0;
         std::vector<double> pp, qq;
         for (int j = 0; j < gc3Res.m_candidates.size(); ++j) {
            double p1 = gc3Res.m_candidates[j].m_relaxPrevProb;
            double q1 = supportFunc(i, j);

            pp.push_back(p1);
            qq.push_back(q1);

            denom += p1 * q1;
         }

         for (int j = 0; j < gc3Res.m_candidates.size(); ++j) {
            gc3Res.m_candidates[j].m_relaxProb = (pp[j] * qq[j]) / denom;
         }
      }

      for (int i = 0; i < pnts.size(); ++i) {
         Gc3Result & gc3Res = *pnts[i];

         if (gc3Res.m_isRelaxationFinished) {
            continue;
         }

         for (int j = 0; j < gc3Res.m_candidates.size(); ++j) {
            double prob = gc3Res.m_candidates[j].m_relaxProb;
            gc3Res.m_candidates[j].m_relaxPrevProb = prob;

            const double probEps = 0.1;
            if (prob > 1 - probEps) {
               gc3Res.m_isRelaxationFinished = true;
               gc3Res.m_bestCandidateIdx = j;
               break;
            }
         }
      }
   }

   for (int i = 0; i < pnts.size(); ++i) {
      Gc3Result & gc3Res = *pnts[i];

      if (gc3Res.m_isRelaxationFinished) {
         continue;
      }

      int bestIdx = -1;
      double maxProb = -1.0;
      for (int j = 0; j < gc3Res.m_candidates.size(); ++j) {
         double prob = gc3Res.m_candidates[j].m_relaxProb;
         if (prob > maxProb) {
            maxProb = prob;
            bestIdx = j;
         }
      }

      gc3Res.m_isRelaxationFinished = true;
      gc3Res.m_bestCandidateIdx = bestIdx;      
   }
}


void MyMatcher::MatchPointFeatures()
{
   extractPointFeatures(m_refImage.GetImage(), m_fPoints);
   extractEdgeFeatures(m_refImage.GetImage(), m_fRefEdges);

   int hh = m_refImage.GetImage().rows;
   int ww = m_refImage.GetImage().cols;

   m_fPointCache.clear();
   m_fPointCache.resize(hh * ww, NULL);

   for (int i = 0; i < m_fPoints.size(); ++i) {
      int ind = m_fPoints[i].y * ww + m_fPoints[i].x;
      m_fPointCache[ind] = new Gc3Result();
   }

   for (int i = 0; i < m_fRefEdges.size(); ++i) {
      cv::Point2i p1 = m_fRefEdges[i].GetDominantPoint(0);
      cv::Point2i p2 = m_fRefEdges[i].GetDominantPoint(1);

      int ind1 = p1.y * ww + p1.x;
      int ind2 = p2.y * ww + p2.x;

      if (!m_fPointCache[ind1]) {
         m_fPointCache[ind1] = new Gc3Result();
      }

      if (!m_fPointCache[ind2]) {
         m_fPointCache[ind2] = new Gc3Result();
      }
   }



   std::vector<Gc3Result*> resVec;
   for (int i = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         Gc3Result * & res = GetPResultFromCache(i, j);
         if (res && !MatchGc3(cv::Point2i(j, i), *res)) {
            delete res;
            res = NULL;
         }

         // todo for debug
         //if (res) {
         //   resVec.push_back(res);
         //   if (resVec.size() == 20) {
         //      DebugShowGc3Result(resVec);
         //      return;
         //   }
         //}
      }
   }

   SaveIntermediateGc3Results(m_cmdArgs.GetOutputPath("intermResults.fct"));
}


void MyMatcher::MatchEdgeFeatures()
{
   // detect edges in search images
   m_fsearchImageEdges.clear();
   m_fsearchImageEdgesIndices.clear();

   m_fsearchImageEdges.resize(m_searchImages.size());
   m_fsearchImageEdgesIndices.resize(m_searchImages.size());
   for (int i = 0; i < m_searchImages.size(); ++i) {
      extractEdgeFeatures(m_searchImages[i].GetImage(), m_fsearchImageEdges[i]);
      cv::Mat & mt = m_fsearchImageEdgesIndices[i];
      mt = cv::Mat::zeros(m_searchImages[i].GetImage().rows, m_searchImages[i].GetImage().cols, CV_32SC1);
      for (int j = 0; j < m_fsearchImageEdges[i].size(); ++j) {
         const MyFeatureEdge &fEdge = m_fsearchImageEdges[i][j];
         for (int k = 0; k < fEdge.m_points.size(); ++k) {
            mt.at<int>(fEdge.m_points[k].y, fEdge.m_points[k].x) = j + 1;
         }
      }
   }

   for (int i = 0; i < m_fRefEdges.size(); ++i) {
      MyFeatureEdge & edge = m_fRefEdges[i];

      // skip feature finding for edges < 30 degress with epiline (p. 80)
      if (edge.m_line.Cosine(m_refImage.GetEpiLine()) > cos(M_PI/6.0)) {
         continue;
      }

      cv::Point2i p0 = edge.GetDominantPoint(0);
      cv::Point2i p1 = edge.GetDominantPoint(1);

      Gc3Result * & r0 = GetPResultFromCache(p0.y, p0.x);
      Gc3Result * & r1 = GetPResultFromCache(p1.y, p1.x);

      // todo here if (r0&&!r1||r1&&!r0)
      if (!r0 || !r1) {
         continue;
      }

      edge.m_relaxationEdgePoints.clear();
      edge.m_relaxationEdgePoints.push_back(r0);

      // every 2px point matching on edge
      int len = (int)edge.GetLength() / 2.0;
      for (int j = 0; j < len; ++j) {
         double t = (double)j/len;
         cv::Point2d pt = edge.Interpolate(t);
         cv::Point2i pti = cv::Point2i((int)pt.x, (int)pt.y);

         Gc3Result * & r = GetPResultFromCache(pti.y, pti.x);
         if (r) {
            edge.m_relaxationEdgePoints.push_back(r);
            continue;
         }

         r = new Gc3Result();

         if (!MatchGc3(pti, t, edge, *r0, *r1, *r)) {
            delete r;
            r = NULL;
            continue;
         }

         r->m_edgePointIdx = edge.m_relaxationEdgePoints.size() - 1; 
         r->m_edgeIndex = i;

         edge.m_relaxationEdgePoints.push_back(r);
      }

      edge.m_relaxationEdgePoints.push_back(r1);
   }
}


bool MyMatcher::MatchGc3(const cv::Point2i & pnt, double t, const MyFeatureEdge & edge, 
   const Gc3Result & res0, const Gc3Result & res1, Gc3Result & res)
{
   int deltaZ = 5;
   double zStep = GetZStep();
   CorrWindowSizeExpander winExpander;

   double hhfrom = res0.GetBestCandidate().m_worldPoint.z;
   double hhto = res1.GetBestCandidate().m_worldPoint.z;
   double href = hhfrom + (hhto-hhfrom) * t;

   cv::Point2d reflonLat;
   m_refImage.GetCamera().ToGeodetic(pnt.x, pnt.y, href, reflonLat.x, reflonLat.y);

   std::vector<cv::Vec2d> searchVectors(m_searchImages.size());
   std::vector<cv::Vec2d> searchOrientations(m_searchImages.size());
   for (int i = 0; i < m_searchImages.size(); ++i) {
      const cv::Mat &rotMat = m_fsearchImageRotMatrices[i];
      cv::Vec2d v = edge.m_line.ToNormalizedVec();

      double vx = rotMat.at<double>(0,0) * v[0] + rotMat.at<double>(0,1) * v[1];
      double vy = rotMat.at<double>(1,0) * v[0] + rotMat.at<double>(1,1) * v[1];

      searchVectors[i] = cv::Vec2d(vx, vy);

      // todo (use orient instead of vector)
      cv::Vec2d vorient = edge.m_orient;
      vx = rotMat.at<double>(0,0) * vorient[0] + rotMat.at<double>(0,1) * vorient[1];
      vy = rotMat.at<double>(1,0) * vorient[0] + rotMat.at<double>(1,1) * vorient[1];

      searchOrientations[i] = cv::Vec2d(vx, vy);
   }

   for (;;) {
      CorrWindowSize corrSize = winExpander.GetCorrWindowSize();

      double hfrom = hhfrom - zStep;
      double hto = hhto + zStep;

      std::vector<MyVector2d> pParallaxVectors;
      for (int i = 0; i < m_searchImages.size(); ++i) {
         const RPCCamera & camera = m_searchImages[i].GetCamera();

         cv::Point2d pixFrom, pixTo;
         camera.ToPixelCoordinates(reflonLat.x, reflonLat.y, hfrom, pixFrom.x, pixFrom.y);
         camera.ToPixelCoordinates(reflonLat.x, reflonLat.y, hto, pixTo.x, pixTo.y);

         cv::Point2d delta = pixTo - pixFrom;
         MyVector2d epiVec(delta.x, delta.y);
         MyVectorNormalize(epiVec);

         pParallaxVectors.push_back(MyVector2d(-epiVec.p[1], epiVec.p[0]));
      }


      MyCorrWindow refCorrWindow;
      refCorrWindow.InitFromCenter(MyPoint2d(pnt.x, pnt.y), corrSize);

      Gc3Result myRes;
      myRes.m_refPoint = pnt;
      myRes.m_bestWindow = corrSize;

      int numIters = (hto-hfrom) / zStep;
      for (int iter = 0; iter < numIters; ++iter) {
         double hq = hfrom + (hto - hfrom) * (double)iter / numIters;

         Gc3Candidate candidate;
         double & sncc = candidate.m_sncc;

         sncc = 0;
         int i = 0;
         for (; i < m_searchImages.size(); ++i) {
            const RPCCamera & searchCamera = m_searchImages[i].GetCamera();

            MyCorrWindow corrWindow = refCorrWindow;
            corrWindow.ProjectToCamera(hq, m_refImage.GetCamera(), searchCamera);

            // offset from epipolar line (compensate parallax)
            double maxNcc = 0.0;
            cv::Point2i maxNccPoint;

            for (int c = -2; c <= 2; ++c) {
               MyCorrWindow corrCopy = corrWindow;
               MyPoint2d newCenter = corrCopy.GetCenter();

               newCenter.p[0] += c * pParallaxVectors[i].p[0];
               newCenter.p[1] += c * pParallaxVectors[i].p[1];

               corrCopy.RecenterProjected(newCenter);

               MyLine sLine = MyLine::FromPointAndVector(cv::Vec2d(newCenter.p[0], newCenter.p[1]), searchVectors[i]);

               double ncc = 0.0;
               if (!m_searchImages[i].CrossCorrelation(corrCopy, sLine, m_refImage, refCorrWindow, edge.m_line, ncc)) {
                  continue;
               }

               if (maxNcc < ncc) {
                  maxNcc = ncc;
                  maxNccPoint = cv::Point2i(newCenter.p[0], newCenter.p[1]);
               }
            }

            // 2-way consistency check
            {
               MyCorrWindow corrCopy = corrWindow;
               corrCopy.ProjectToCamera(hq, m_searchImages[i].GetCamera(), m_refImage.GetCamera());
               MyPoint2d refReprojected = corrCopy.GetCenter();
               if (abs(refReprojected.p[0] - pnt.x) >= 2 || abs(refReprojected.p[1] - pnt.y) >= 2) {
                  break;
               }
            }

            if (maxNcc == 0) {
               break;
            }

            candidate.m_searchPoints.push_back(maxNccPoint);
            sncc += maxNcc;
         }

         if (i != m_searchImages.size()) {
            continue;
         }

         sncc /= (double)m_searchImages.size();
         candidate.m_sncc = sncc;
         candidate.m_worldPoint = cv::Point3d(reflonLat.x, reflonLat.y, hq);

         if (candidate.GetSncc() < 0.5) {
            continue;
         }

         myRes.m_candidates.push_back(candidate);
      }

      // compute reliability for match point
      for (int i = 0; i < myRes.m_candidates.size(); ++i) {
         double deltaZ, deltaSncc, leftSlopeResult, rightSlopeResult, slopeResult = 0;

         int idx = 0;
         if (i > 0 && myRes.m_candidates[i-1].GetSncc() < myRes.m_candidates[i].GetSncc()) {
            idx = -1;
            deltaZ = myRes.m_candidates[i].m_worldPoint.z - myRes.m_candidates[i-1].m_worldPoint.z;
            deltaSncc = myRes.m_candidates[i].GetSncc() - myRes.m_candidates[i-1].GetSncc();
            slopeResult = leftSlopeResult = Gc3Candidate::GetSlopeResult(deltaZ / (hto - hfrom), deltaSncc);
         }

         if (i + 1 < myRes.m_candidates.size() && myRes.m_candidates[i].GetSncc() > myRes.m_candidates[i+1].GetSncc()) {
            deltaZ = myRes.m_candidates[i].m_worldPoint.z - myRes.m_candidates[i+1].m_worldPoint.z;
            deltaSncc = myRes.m_candidates[i].GetSncc() - myRes.m_candidates[i+1].GetSncc();
            rightSlopeResult = Gc3Candidate::GetSlopeResult(deltaZ / (hto - hfrom), deltaSncc);

            if (idx != 0) {
               if (rightSlopeResult > leftSlopeResult) {
                  idx = 1;
                  slopeResult = rightSlopeResult;
               }
            }
         }

         if (idx != 0) {
            myRes.m_candidates[i].m_snccSlope = slopeResult;
         }
      }

      std::sort(myRes.m_candidates.begin(), myRes.m_candidates.end());

      if (myRes.m_candidates.size() == 1) {
         myRes.m_candidates[0].m_sncc = 1.0 / 0.6; // hack to get 1.0 in GetReliability()
         myRes.m_candidates[0].m_snccSlope = 0.0;
      }

      if (!winExpander.Expand(myRes)) {
         break;
      }
   }

   res = winExpander.GetGc3Result();

   std::vector<Gc3Candidate> newCandidates;
   for (int i = 0; i < res.m_candidates.size(); ++i) {
      Gc3Candidate & cand = res.m_candidates[i];

      assert(cand.m_searchPoints.size() == m_searchImages.size());
      
      bool edgeOnAtLeastOneImage = false;
      bool edgeOnEachImage = true;
      std::vector<int> edgeIndices;
      for (int j = 0; j < cand.m_searchPoints.size(); ++j) {
         const cv::Point2i & p = cand.m_searchPoints[j];
         int edgeIdx = m_fsearchImageEdgesIndices[j].at<int>(p);
         if (edgeIdx == 0) {
            edgeOnEachImage = false;
         } else {
            edgeOnAtLeastOneImage = true;
         }
         edgeIndices.push_back(edgeIdx - 1);
      }

      if (!edgeOnEachImage && cand.GetSncc() < 0.75) {
         continue;
      }

      if (!edgeOnAtLeastOneImage) {
         continue;
      }

      // check sign
      bool signEqual = true;
      for (int j = 0; j < edgeIndices.size(); ++j) {
         int idx = edgeIndices[j];
         if (idx < 0) 
            continue;
         const MyFeatureEdge & sEdge = m_fsearchImageEdges[i][j];
         if (edge.m_sign != sEdge.m_sign ) {
            signEqual = false;
            break;
         }
      }

      if (!signEqual) {
         continue;
      }

      // check orientation
      bool orientEqual = true;
      for (int j = 0; j < edgeIndices.size(); ++j) {
         int idx = edgeIndices[j];
         if (idx < 0) 
            continue;
         const MyFeatureEdge & sEdge = m_fsearchImageEdges[i][j];
         double cosAngle = searchOrientations[i].dot(sEdge.m_orient);

         // orient < 30 accept
         if (cosAngle < cos(M_PI/6.0)) {
            orientEqual = false;
            break;
         }
      }

      if (!orientEqual) {
         continue;
      }

      newCandidates.push_back(cand);
   }

   res.m_candidates.swap(newCandidates);
   return !res.m_candidates.empty();
}


double MyMatcher::GetZStep() const
{
   double res = m_refImage.Resolution();
   for (int i = 0; i < m_searchImages.size(); ++i) {
      // TODO changed to max
      res = std::max(res, m_searchImages[i].Resolution());
   }
   return res;
}


double MyMatcher::GetAltitude(const MyPointCloud & cloud, const MyCorrImage &img, const cv::Point2i & pnt) const
{
   cv::Point2d p = img.GetLonLatByPoint(pnt.y, pnt.x);
   return cloud.GetAltitude(p.x, p.y);
}



bool MyMatcher::MatchGc3(const cv::Point2i & pnt, Gc3Result & res)
{
   // need more deltaZ for smaller scales
   int deltaZ = 10 * (1.0/m_refImage.GetCamera().GetScaleX());
   double zStep = GetZStep();
   CorrWindowSizeExpander winExpander;

   double href = GetAltitude(m_prevCloud, m_refImage, pnt);
   assert(href != 1e30);

   cv::Point2d reflonLat;
   m_refImage.GetCamera().ToGeodetic(pnt.x, pnt.y, href, reflonLat.x, reflonLat.y);

   for (;;) {
      CorrWindowSize corrSize = winExpander.GetCorrWindowSize();

      double hfrom = href - deltaZ * zStep;
      double hto = href + deltaZ * zStep;

      std::vector<MyVector2d> pParallaxVectors;
      for (int i = 0; i < m_searchImages.size(); ++i) {
         const RPCCamera & camera = m_searchImages[i].GetCamera();

         cv::Point2d pixFrom, pixTo;
         camera.ToPixelCoordinates(reflonLat.x, reflonLat.y, hfrom, pixFrom.x, pixFrom.y);
         camera.ToPixelCoordinates(reflonLat.x, reflonLat.y, hto, pixTo.x, pixTo.y);

         cv::Point2d delta = pixTo - pixFrom;
         MyVector2d epiVec(delta.x, delta.y);
         MyVectorNormalize(epiVec);

         pParallaxVectors.push_back(MyVector2d(-epiVec.p[1], epiVec.p[0]));
      }


      MyCorrWindow refCorrWindow;
      refCorrWindow.InitFromCenter(MyPoint2d(pnt.x, pnt.y), corrSize);

      Gc3Result myRes;
      myRes.m_refPoint = pnt;
      myRes.m_bestWindow = corrSize;

      //cv::Point2i prevCoord(-1,-1);
      //int64 maskChanged = (1<<(int)m_searchImages.size()/2)-1;
      //std::vector<int> prevCoordinates, currCoordinates;
      for (int iter = 0; iter < deltaZ * 2; ++iter) {
         double hq = hfrom + (hto - hfrom) * (double)iter / (deltaZ * 2);

         if (hq<0) {
            continue;
         }

         Gc3Candidate candidate;
         double & sncc = candidate.m_sncc;

         //currCoordinates.clear();
         std::vector<MyCorrWindow> searchWindows;
         //bool skipStep = false;
         for (int j = 0; j < m_searchImages.size(); ++j) {
            const RPCCamera & searchCamera = m_searchImages[j].GetCamera();

            MyCorrWindow corrWindow = refCorrWindow;
            corrWindow.ProjectToCamera(hq, m_refImage.GetCamera(), searchCamera);
            searchWindows.push_back(corrWindow);

            //currCoordinates.push_back((int)corrWindow.GetCenter().p[0]);
            //currCoordinates.push_back((int)corrWindow.GetCenter().p[1]);

            //MyPoint2d corrCenter = corrWindow.GetCenter();
            //cv::Point2i curCoord((int)corrCenter.p[0], (int)corrCenter.p[1]);

            //if (m_searchImages[j].IsMostInclined()) {
            //   if (prevCoord.x < 0) {
            //      skipStep = false;
            //      prevCoord = curCoord;
            //   } else {
            //      if (prevCoord.x != curCoord.x || prevCoord.y  != curCoord.y) {
            //         skipStep = false;
            //         prevCoord = curCoord;
            //      } else {
            //         skipStep = true;
            //      }
            //   }
            //}
         }

         //if (prevCoordinates.empty()) {
         //   prevCoordinates = currCoordinates;
         //   skipStep = false;
         //} else {
         //   for (int j = 0; j < prevCoordinates.size(); j+=2) {
         //      if (prevCoordinates[j] != currCoordinates[j] || prevCoordinates[j+1] != currCoordinates[j+1]) {
         //         maskChanged &= ~(1<<(j/2));
         //      }
         //   }

         //   if (maskChanged != 0) {
         //      skipStep = true;
         //   } else {
         //      skipStep = false;
         //      prevCoordinates = currCoordinates;
         //      maskChanged = (1<<(int)m_searchImages.size()/2)-1;
         //   }
         //}



         //bool skipStep = false;
         //if (!prevCoordinates.empty()) {
         //   assert(prevCoordinates.size() == currCoordinates.size());
         //   
         //   skipStep = true;
         //   for (int j = 0; j < prevCoordinates.size(); ++j) {
         //      if (prevCoordinates[j] != currCoordinates[j]) {
         //         skipStep = false;
         //         break;
         //      }
         //   }
         //}

         //prevCoordinates = currCoordinates;

         //if (skipStep) {
         //   continue;
         //}

         sncc = 0;
         int i = 0;
         for (; i < m_searchImages.size(); ++i) {
            const RPCCamera & searchCamera = m_searchImages[i].GetCamera();

            const MyCorrWindow &corrWindow = searchWindows[i];
            //corrWindow.ProjectToCamera(hq, m_refImage.GetCamera(), searchCamera);

            // offset from epipolar line (compensate parallax)
            double maxNcc = 0.0;
            cv::Point2i maxNccPoint;
            
            for (int c = -2; c <= 2; ++c) {
               MyCorrWindow corrCopy = corrWindow;
               MyPoint2d newCenter = corrCopy.GetCenter();

               newCenter.p[0] += c * pParallaxVectors[i].p[0];
               newCenter.p[1] += c * pParallaxVectors[i].p[1];

               corrCopy.RecenterProjected(newCenter);

               double ncc = 0.0;
               if (!m_searchImages[i].CrossCorrelation(corrCopy, m_refImage, refCorrWindow, ncc)) {
                  continue;
               }

               if (maxNcc < ncc) {
                  maxNcc = ncc;
                  maxNccPoint = cv::Point2i(newCenter.p[0], newCenter.p[1]);
               }
            }

            if (maxNcc == 0) {
               break;
            }

            // 2-way consistency check
            {
               MyCorrWindow corrCopy = corrWindow;
               corrCopy.ProjectToCamera(hq, m_searchImages[i].GetCamera(), m_refImage.GetCamera());
               MyPoint2d refReprojected = corrCopy.GetCenter();
               if (abs(refReprojected.p[0] - pnt.x) >= 2 || abs(refReprojected.p[1] - pnt.y) >= 2) {
                  break;
               }
            }

            candidate.m_searchPoints.push_back(maxNccPoint);
            sncc += maxNcc;
         }

         if (i != m_searchImages.size()) {
            continue;
         }

         sncc /= (double)m_searchImages.size();
         candidate.m_sncc = sncc;
         candidate.m_worldPoint = cv::Point3d(reflonLat.x, reflonLat.y, hq);

         //if (candidate.GetSncc() < 0.5) {
         //   continue;
         //}

         myRes.m_candidates.push_back(candidate);
      }

      // compute reliability for match point (more sharp slope)
      for (int i = 0; i < myRes.m_candidates.size(); ++i) {
         double localDeltaZ, deltaSncc, leftSlopeResult, rightSlopeResult, slopeResult = 0;

         int idx = 0;
         if (i > 0 && myRes.m_candidates[i-1].GetSncc() < myRes.m_candidates[i].GetSncc()) {
            idx = -1;
            localDeltaZ = myRes.m_candidates[i].m_worldPoint.z - myRes.m_candidates[i-1].m_worldPoint.z;
            deltaSncc = myRes.m_candidates[i].GetSncc() - myRes.m_candidates[i-1].GetSncc();
            slopeResult = leftSlopeResult = Gc3Candidate::GetSlopeResult(localDeltaZ / (hto - hfrom), deltaSncc);
         }

         if (i + 1 < myRes.m_candidates.size() && myRes.m_candidates[i].GetSncc() > myRes.m_candidates[i+1].GetSncc()) {
            localDeltaZ = myRes.m_candidates[i].m_worldPoint.z - myRes.m_candidates[i+1].m_worldPoint.z;
            deltaSncc = myRes.m_candidates[i].GetSncc() - myRes.m_candidates[i+1].GetSncc();
            rightSlopeResult = Gc3Candidate::GetSlopeResult(localDeltaZ / (hto - hfrom), deltaSncc);

            if (idx != 0) {
               if (rightSlopeResult > leftSlopeResult) {
                  idx = 1;
                  slopeResult = rightSlopeResult;
               }
            }
         }

         if (idx != 0) {
            myRes.m_candidates[i].m_snccSlope = slopeResult;
         }
      }

      std::sort(myRes.m_candidates.begin(), myRes.m_candidates.end());

      if (myRes.m_candidates.size() == 1) {
         myRes.m_candidates[0].m_sncc = 1.0 / 0.6; // hack to get 1.0 in GetReliability()
         myRes.m_candidates[0].m_snccSlope = 0.0;
      }

      if (!winExpander.Expand(myRes)) {
         break;
      }
   }

   res = winExpander.GetGc3Result();
   //return res.GetSnccRatio() > 0;

   bool foundGoodUniqMatch = res.m_candidates.size() == 1 && res.m_candidates[0].GetSncc() > 0.75;
   bool foundGoodRatio = res.m_candidates.size() > 1 && res.m_candidates.back().GetSncc() > 0.6 && res.GetSnccRatio() >= 1.5;

   return foundGoodUniqMatch || foundGoodRatio;
}


const int WSIZE = 3;
const double STDDEV_THRESHOLD = 9.0;
const double STDDEV_GAUSS = 1.0;
double sGauss2 = STDDEV_GAUSS * STDDEV_GAUSS;
double sGauss4 = sGauss2 * sGauss2;

static void extractForstnerFromPoint(const cv::Mat & inp, int x, int y, double & outQ, double & outW)
{
   double fx[WSIZE][WSIZE];
   double fy[WSIZE][WSIZE];

   outQ = outW = 0.0;

   for (int i = 0; i < WSIZE; ++i) {
      for (int j = 0; j < WSIZE; ++j) {

         double v = 0.0;
         for (int ii = -WSIZE / 2; ii <= WSIZE / 2; ++ii) {

            double coef = exp(- ii * ii / (2 * sGauss2));
            for (int jj = -WSIZE / 2; jj <= WSIZE / 2; ++jj) {

               int xx = x + j - jj;
               int yy = y + i - ii;

               double coef1 = - jj / (2 * CV_PI * sGauss4) * exp(- jj * jj / (2 * sGauss2));
               double imgValue = (double)inp.at<uchar>(yy, xx);

               v += coef * coef1 * imgValue;
            }
         }

         fx[i][j] = v;

         v = 0.0;
         for (int jj = -WSIZE / 2; jj <= WSIZE / 2; ++jj) {
            double coef = exp(- jj * jj / (2 * sGauss2));
            for (int ii = -WSIZE / 2; ii <= WSIZE / 2; ++ii) {

               int xx = x + j - jj;
               int yy = y + i - ii;

               double coef1 = - ii / (2 * CV_PI * sGauss4) * exp(- ii * ii / (2 * sGauss2));
               double imgValue = (double)inp.at<uchar>(yy, xx);

               v += coef * coef1 * imgValue;
            }
         }

         fy[i][j] = v;
      }
   }

   double qm[2][2] = {};
   for (int i = 0; i < WSIZE; ++i) {
      for (int j = 0; j < WSIZE; ++j) {
         qm[0][0] += fx[i][j] * fx[i][j];
         qm[1][1] += fy[i][j] * fy[i][j];
         qm[0][1] += fx[i][j] * fy[i][j];
      }
   }

   qm[1][0] = qm[0][1];

   double detqm = qm[0][0] * qm[1][1] - qm[0][1] * qm[1][0];
   if (abs(detqm) < MYEPS) {
      return;
   }

   double tracenm = (qm[0][0] + qm[1][1]) / detqm;
   double detnm = (qm[0][0] * qm[1][1] - qm[0][1] * qm[1][0]) / (detqm * detqm);

   outQ = 4 * detnm / (tracenm * tracenm);
   outW = detnm / tracenm;
}


void extractPointFeatures(const cv::Mat & inp, std::vector<cv::Point2i> & fPoints)
{
   fPoints.clear();
   // TODO
   for (int y = WSIZE + 1; y + 2 * WSIZE + 1 < 200 /*inp.rows*/; y += WSIZE) {
      for (int x = WSIZE + 1; x + 2 * WSIZE + 1 < inp.cols; x += WSIZE) {


         double stdDev = myStdDev<uchar>(inp, cv::Rect(x, y, WSIZE, WSIZE));

         if (stdDev < STDDEV_THRESHOLD) {
            continue;
         }

         int maxX = -1, maxY = -1;
         double maxInterest = 0.0;
         for (int i = 0; i < WSIZE; ++i) {
            for (int j = 0; j < WSIZE; ++j) {

               double q, w;
               extractForstnerFromPoint(inp, x + j, y + i, q, w);

               if (q < 0.5) {
                  continue;
               }

               if (maxInterest < w) {
                  maxInterest = w;
                  maxX = x + j;
                  maxY = y + i;
               }
            }
         }

         if (maxX >= 0) {
            fPoints.push_back(cv::Point2i(maxX, maxY));
         }
      }
   }
}

typedef cv::Point2i PointType;
typedef std::vector<PointType> PointList;
typedef std::vector<PointList> PointPointList;




bool needSplitEdge(int from, int to, const PointList & curEdge, int & splitIdx)
{
   if (to - from <= 1) {
      return false;
   }

   PointType fromPoint = curEdge[from];
   PointType toPoint = curEdge[to];

   MyLine mn = MyLine::FromPoints(fromPoint, toPoint);

   int mid = -1;
   double maxDist = 0.0f;
   double dist = 0.0f;
   for (int i = from + 1; i + 1 < to; ++i) {
      PointType pnt = curEdge[i];
      
      dist = mn.Distance(pnt);

      if (dist > maxDist) {
         maxDist = dist;
         mid = i;
      }
   }

   float ddx = float(fromPoint.x - toPoint.x);
   float ddy = float(fromPoint.y - toPoint.y);
   float dd = sqrt(ddx * ddx + ddy * ddy);
   float ldd = dd >= 1.0f ? (1.0f + log10(dd)) : 1.0f;
   splitIdx = mid;
   return mid != -1 && maxDist > ldd;
}

bool needSplitEdge(const PointList & curEdge, int & splitIdx)
{
   return needSplitEdge(0, (int)curEdge.size() - 1, curEdge, splitIdx);
}

bool needSplitEdge(const PointList & curEdge)
{
   int stub;
   return needSplitEdge(0, (int)curEdge.size() - 1, curEdge, stub);
}


void edgeRecursiveSplit(int from, int to, const PointList & curEdge, PointPointList & ppList)
{
   if (from >= to) {
      return;
   }

   int mid;

   if (needSplitEdge(from, to, curEdge, mid)) {
      edgeRecursiveSplit(from, mid, curEdge, ppList);
      edgeRecursiveSplit(mid, to, curEdge, ppList);
      return;
   }

   PointList toPush;
   std::copy(&curEdge[from], &curEdge[to], std::back_inserter(toPush));
   ppList.push_back(toPush);
}

static void mergePointPointList(PointPointList & ppList);

void detectEdgesRecursive(int row, int col, const cv::Mat & inp, cv::Mat & eState, PointPointList & ppList)
{
   if (eState.at<uchar>(row, col) == 1) {
      return;
   }

   if (inp.at<uchar>(row, col) != 255) {
      return;
   }

   PointList rOthers;
   PointList curEdge;

   int prevOrient = -1;
   int pointCount = 1;

   const int deltaRow[8] = {0,-1,-1,-1,0,1,1,1};
   const int deltaCol[8] = {-1,-1,0,1,1,1,0,-1};

   eState.at<uchar>(row, col) = 1;
   curEdge.push_back(cv::Point2i(col,row));

   for (;;) 
   {
      uchar candCount = 0;
      uchar candidates = 0;
      int lastOrient = 0;

      for (int i = 0; i < 8; ++i) {
         int rrow = row + deltaRow[i];
         int rcol = col + deltaCol[i];

         if (rrow >= 0 && rcol >= 0 && rrow < inp.rows && rcol < inp.cols) {
            uchar es = eState.at<uchar>(rrow, rcol);
            uchar vv = inp.at<uchar>(rrow,rcol);
            if (vv == 255 && es == 0) {
               candidates |= (1<<i);
               lastOrient = i;
               candCount++;
            }
         }
      }

      int dir = lastOrient;
      int pSize = (int)curEdge.size();

      if (candCount > 1) {

         // choose any
         int bestOrient = -1;
         int minDelta = 10;
         for (int i = 0; i < 8; ++i) {
            if (candidates & (1 << i)) {
               if (bestOrient == -1) {
                  bestOrient = i;
                  if (prevOrient == -1) {
                     break;
                  }
                  continue;
               }

               int delta = abs(deltaRow[prevOrient] - deltaRow[i]) + abs(deltaCol[prevOrient] - deltaCol[i]);
               if (minDelta > delta) {
                  minDelta = delta;
                  bestOrient = i;
               }
            }
         }

         dir = bestOrient;

      } else if (candCount == 0) {
         break;
      }

      // push other points for later processing
      for (int i = 0; i < 8; ++i) {
         if (i == dir) {
            continue;
         }
         if (candidates & (1 << i)) {
            rOthers.push_back(cv::Point2i(col+deltaCol[i],row+deltaRow[i]));
         }
      }

      row += deltaRow[dir];
      col += deltaCol[dir];

      eState.at<uchar>(row, col) = 1;
      curEdge.push_back(cv::Point2i(col,row));

      prevOrient = dir;
   }

   if (curEdge.size() >= 15) {
      //pList.push_back(curEdge);
      PointPointList myppList;
      edgeRecursiveSplit(0, (int)curEdge.size() - 1, curEdge, myppList);
      mergePointPointList(myppList);
      std::copy(myppList.begin(), myppList.end(), std::back_inserter(ppList));
   }

   // run rOthers
   for (int i = 0; i < rOthers.size(); ++i) {
      detectEdgesRecursive(rOthers[i].y, rOthers[i].x, inp, eState, ppList);
   }
}

static void mergePointPointList(PointPointList & ppList)
{
   if (ppList.empty()) {
      return;
   }

   PointPointList newppList;
   newppList.push_back(ppList[0]);
   for (int i = 1; i < ppList.size(); ++i) {
      int sz = (int)newppList.back().size();
      std::copy(ppList[i].begin(), ppList[i].end(), std::back_inserter(newppList.back()));
      if (needSplitEdge(newppList.back())) {
         newppList.back().erase(newppList.back().begin()+sz, newppList.back().end());
         newppList.push_back(ppList[i]);
      }
   }
}


static cv::Point2d imageComputeEdgeOrientation(const cv::Mat & inp, MyFeatureEdge & edge)
{
   double grad = (double)inp.at<uchar>(edge.GetDominantPoint(1)) - (double)inp.at<uchar>(edge.GetDominantPoint(0));
   edge.m_line = MyLine::FromPoints(edge.GetDominantPoint(0), edge.GetDominantPoint(1));
   return edge.m_line.ToNormalizedVec() * (grad > 0 ? 1.0 : -1.0);
}


static bool imageComputeEdgeSign(const cv::Mat & inp, MyFeatureEdge & edge)
{
   cv::Vec2d perpend = edge.m_line.ToNormalizedPerpendVec();
   int len = (int)edge.GetLength();
   double sUp = 0.0, sDown = 0.0;
   int nUp = 0, nDown = 0;
   for (int i = 0; i < len; ++i) {
      cv::Point2d pt = edge.Interpolate((double)i/len);
      cv::Point2i ptUp = cv::Point2i(pt.x+perpend[0],pt.y+perpend[1]);
      cv::Point2i ptDown = cv::Point2i(pt.x-perpend[0],pt.y-perpend[1]);
      
      if (myIsInsideMat(inp, ptUp)) {
         sUp += (double)inp.at<uchar>(ptUp); nUp++;
      }
      if (myIsInsideMat(inp, ptDown)) {
         sDown += (double)inp.at<uchar>(ptDown); nDown++;
      }
   }

   if (nUp > 0 && nDown > 0) {
      return sUp / nUp > sDown / nDown;
   } 

   return true;
}



void extractEdgeFeatures(const cv::Mat & inp, std::vector<MyFeatureEdge> & edges)
{
   edges.clear();
   cv::Mat eState = cv::Mat::zeros(inp.rows, inp.cols, CV_8U);

   cv::Mat edgeImg, thrImg;
   double otsuThr = cv::threshold(inp, thrImg, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
   cv::Canny(inp, edgeImg, otsuThr * 0.5, otsuThr, 5);

   PointPointList ppList;
   for (int i = 0; i < inp.rows; ++i) {
      for (int j = 0; j < inp.cols; ++j) {
         detectEdgesRecursive(i, j, edgeImg, eState, ppList);
      }
   }

   for (int i = 0; i < ppList.size(); ++i) {
      if (ppList[i].size() >= 7) {
         MyFeatureEdge edge;
         edge.m_p[0] = ppList[i][0];
         edge.m_p[1] = ppList[i].back();
         edge.m_orient = imageComputeEdgeOrientation(inp, edge);
         edge.m_sign = imageComputeEdgeSign(inp, edge);
         edge.m_points = ppList[i];
         edges.push_back(edge);
      }
   }
}

void Gc3Candidate::Serialize(std::fstream & fs)
{
   fs << m_sncc << " ";
   fs << m_snccSlope << " ";
   fs << m_worldPoint.x  << " " << m_worldPoint.y  << " " << m_worldPoint.z << " ";
   fs << m_searchPoints.size()  << " ";
   for (int i = 0; i < m_searchPoints.size(); ++i) {
      fs << m_searchPoints[i].x << " " << m_searchPoints[i].y << " ";
   }
}

void Gc3Candidate::Deserialize(std::fstream & fs)
{
   fs >> m_sncc;
   fs >> m_snccSlope;
   fs >> m_worldPoint.x >> m_worldPoint.y >> m_worldPoint.z;
   int searchPoints;
   fs >> searchPoints;
   m_searchPoints.resize(searchPoints);
   for (int i = 0; i < searchPoints; ++i) {
      fs >> m_searchPoints[i].x >> m_searchPoints[i].y;
   }
}

void Gc3Result::Serialize(std::fstream & fs)
{
   fs << m_bestWindow.m_deltaX0 << " ";
   fs << m_bestWindow.m_deltaX1 << " ";
   fs << m_bestWindow.m_deltaY0 << " ";
   fs << m_bestWindow.m_deltaY1 << " ";
   fs << m_refPoint.x << " ";
   fs << m_refPoint.y << " ";
   fs << m_candidates.size() << " ";
   for (int i = 0; i < m_candidates.size(); ++i) {
      m_candidates[i].Serialize(fs);
   }
   fs << m_edgeIndex << " ";
}

void Gc3Result::Deserialize(std::fstream & fs)
{
   fs >> m_bestWindow.m_deltaX0;
   fs >> m_bestWindow.m_deltaX1;
   fs >> m_bestWindow.m_deltaY0;
   fs >> m_bestWindow.m_deltaY1;
   fs >> m_refPoint.x;
   fs >> m_refPoint.y;
   int candSize;
   fs >> candSize;
   m_candidates.resize(candSize);
   for (int i = 0; i < candSize; ++i) {
      m_candidates[i].Deserialize(fs);
   }
   fs >> m_edgeIndex;
}

void MyMatcher::LoadIntermediateGc3Results(const std::string & fname)
{
   std::fstream fs(fname, std::ios_base::in);
   int hi, hj;
   while (true) {
      fs >> hi >> hj;
      if (fs.eof()) {
         break;
      }
      Gc3Result * & res = GetPResultFromCache(hi, hj);
      if (res != NULL) {
         delete res;
         res = NULL;
      }
      res = new Gc3Result();
      res->Deserialize(fs);
   }
}

void MyMatcher::SaveIntermediateGc3Results(const std::string & fname)
{
   int hh = m_refImage.GetImage().rows;
   int ww = m_refImage.GetImage().cols;

   std::fstream fs(fname, std::ios_base::out);
   for (int i = 0; i < hh; ++i) {
      for (int j = 0; j < ww; ++j) {
         Gc3Result * & res = GetPResultFromCache(i, j);
         if (res) {
            fs << i << " " << j << " ";
            res->Serialize(fs);
         }
      }
   }
}


//void testPDetection(const std::string &name)
//{
//   cv::Mat img = imread(name, cv::IMREAD_GRAYSCALE);
//
//   std::vector<cv::Point2d> fPoints;
//   extractPointFeatures(img, fPoints);
//
//   cv::namedWindow("myWindow");
//
//   for (int i = 0; i < fPoints.size(); ++i) {
//      cv::circle(img, cv::Point(fPoints[i].x, fPoints[i].y), 2, cv::Scalar(255, 0, 0));
//   }
//
//   cv::imshow("myWindow", img);
//
//   cv::waitKey();
//}
//Gc3Result * & r0 = GetPResultFromCache(p0.y, p0.x);