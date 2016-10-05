


#include "common.h"
#include "RPCModel.h"

#include "Matcher.h"

#include "nr3.h"
#include "interp_1d.h"
#include "pointbox.h"
#include "Kdtree.h"

#include "Matcher.h"

int main(int argc, char **argv) {

   common_init();

   if (argc != 5) {
      return 1;
   }

   MyCmdArgs args;
   args.Init(argv[1], argv[2], argv[3], argv[4], "jpg");

   MyMatcher matcher;
   matcher.Init(args);

   matcher.Match();

   return 0;
}

//struct triangulateio in, out;
//
//memset(&in,0,sizeof(in));
//memset(&out,0,sizeof(out));
//
//double w = 512.0;
//double h = 512.0;
//
//in.numberofpoints = 512;
//in.pointlist = (REAL*)malloc(in.numberofpoints * 2 * sizeof(REAL));
//in.numberofpointattributes = 1;
//in.pointattributelist = (REAL*)malloc(in.numberofpoints * sizeof(REAL));
//
//for (int i = 0; i < in.numberofpoints; ++i) {
//   in.pointlist[i*2+0] = w*((double)rand()/RAND_MAX);
//   in.pointlist[i*2+1] = h*((double)rand()/RAND_MAX);
//   in.pointattributelist[i] = 1;
//}
//
//in.pointmarkerlist = NULL;
//in.segmentmarkerlist = NULL;
//in.holelist = NULL;
//in.numberofholes = 0;
//
////in.numberoftriangles = 3;
////in.trianglelist = (int*)malloc(3*3*sizeof(int));
////in.numberofcorners = 3;
////in.numberoftriangleattributes = 0;
//
////for (int i = 0; i < 3; ++i) {
////   in.trianglelist[i*3+0] = i*10+0;
////   in.trianglelist[i*3+1] = i*10+1;
////   in.trianglelist[i*3+2] = i*10+2;
////}
//
////in.segmentlist = NULL;
////in.numberofsegments = 0;
////in.segmentmarkerlist = NULL;
//int nn = 1;
//in.segmentlist = (int*)malloc(nn*2*sizeof(int));
//in.numberofsegments = nn;
//in.segmentmarkerlist = (int*)malloc(nn*sizeof(int));
//
//for (int i = 0; i < nn; ++i) {
//   in.segmentlist[2*i+0] = i;
//   in.segmentlist[2*i+1] = i+1;
//}
//
//for (int i = 0; i < nn; ++i) {
//   in.segmentmarkerlist[i] = i+2;
//}
//
//out.pointlist = NULL;
//out.pointattributelist = NULL;
//out.trianglelist = NULL;
//out.neighborlist = NULL;
//out.segmentlist = NULL;
//out.segmentmarkerlist = NULL;
//out.edgelist = NULL;
//out.edgemarkerlist = NULL;
//
//struct mesh * meshStruct = NULL;
//struct behavior * behavStruct = NULL;
//
//triangulate("ecpz", &in, &out, NULL, &meshStruct, &behavStruct);
//
//cv::namedWindow("myWin");
//
//cv::Mat img = cv::Mat::zeros(h, w, CV_8UC3);
////for (int i = 0; i < out.numberofpoints; ++i) {
////   cv::circle(img, cv::Point(out.pointlist[i*2+0],out.pointlist[i*2+1]), 2, cv::Scalar(255,255,255));
////}
////for (int i = 0; i < out.numberoftriangles; ++i) {
////   for (int j = 1; j <= 3; ++j) {
////      int from = j - 1;
////      int to = j % 3;
////
////      from = out.trianglelist[i*3+from];
////      to = out.trianglelist[i*3+to];
////
////      cv::Point fromPoint = cv::Point(out.pointlist[from*2+0],out.pointlist[from*2+1]);
////      cv::Point toPoint = cv::Point(out.pointlist[to*2+0],out.pointlist[to*2+1]);
////
////      cv::line(img, fromPoint, toPoint, cv::Scalar(255,255,255));
////   }
////}
//
////for (int i = 0; i < 16; ++i) {
////   int from = in.segmentlist[2*i+0];
////   int to = in.segmentlist[2*i+1];
//
////   cv::Point fromPoint = cv::Point(in.pointlist[from*2+0],in.pointlist[from*2+1]);
////   cv::Point toPoint = cv::Point(in.pointlist[to*2+0],in.pointlist[to*2+1]);
//
////   cv::line(img, fromPoint, toPoint, cv::Scalar(0,255,255));
////}
//
//int c = 0;
//for (int i = 0; i < 5; ++i)
//   for (int j = 0; j < 5; ++j)
//   {
//      REAL queryPoint[2];
//
//      //queryPoint[0] = 256;
//      //queryPoint[1] = 128;
//
//      queryPoint[0] = 32+(w-64)*((double)rand()/RAND_MAX);
//      queryPoint[1] = 32+(h-64)*((double)rand()/RAND_MAX);
//
//      int outindices[3];
//      //REAL outattrs[3];
//      //REAL outtri[6];
//      bool found = false;
//      if (delaunaylocate(meshStruct, behavStruct, queryPoint, outindices) == 0) {
//         for (int i = 0; i < 3; ++i) {
//            REAL * from = &out.pointlist[2*outindices[i]];
//            REAL * to = &out.pointlist[2*outindices[(i+1)%3]];
//            bool fromAttr = out.pointmarkerlist[outindices[i]] > 1;
//            bool toAttr = out.pointmarkerlist[outindices[(i+1)%3]] > 1;
//
//            cv::Point fromPoint = cv::Point(from[0], from[1]);
//            cv::Point toPoint = cv::Point(to[0], to[1]);
//
//            cv::line(img, fromPoint, toPoint, (fromAttr && toAttr) ? cv::Scalar(255,0,255) : cv::Scalar(0,255,255));
//         }
//         found = true;
//      } else {
//         c++;
//      }
//
//      cv::circle(img, cv::Point(queryPoint[0], queryPoint[1]), 1, found ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255));
//   }
//
//   cv::imshow("myWin", img);
//   cv::waitKey();
//
//   free(in.pointlist);
//   free(out.pointlist);
//   free(out.trianglelist);