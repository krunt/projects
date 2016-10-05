

#include "common.h"
#include <numeric>



int printHelp() 
{
   std::cout << "preprocess.exe -i=<input-image> -o=<output-image>" << std::endl;
   return 1;
}


std::set<std::pair<int,int>> g_processed;

void determineHomogenuousRegionsRecursive(int row, int col, const cv::Mat & inp, cv::Mat & processed, std::vector<double> &v, const cv::Mat & homMask)
{
   int dx[4] = { -1,  1,  0, 0 };
   int dy[4] = {  0,  0, -1, 1 };

   if (!(row>=0 && col>=0 && col<inp.cols && row<inp.rows)) {
      return;
   }

   double mn = myMean(v);
   double sdev = myStdDev(v);
   
   for (int i = 0; i < 4; ++i) {
      int y = row + dy[i];
      int x = col + dx[i];

      if (x>=0 && y>=0 && x<inp.cols &&y<inp.rows && g_processed.count(std::make_pair(y, x)) == 0 && homMask.at<uchar>(y, x) == 0) {
         //processed.at<uchar>(y, x) = 1;
         g_processed.insert(std::make_pair(y, x));
         double inpValue = (double)inp.at<uchar>(y,x);
         if (abs(mn-inpValue) <= sdev) {
            v.push_back(inpValue);
            determineHomogenuousRegionsRecursive(y, x, inp, processed, v, homMask);
         }
      }
   }
}

void determineHomogenuousRegions(const cv::Mat & inp, cv::Mat & homMask)
{
   homMask = cv::Mat::zeros(inp.rows, inp.cols, CV_8U);
   cv::Mat processed = cv::Mat::zeros(inp.rows, inp.cols, CV_8U);

   for (int i = 3; i < 50 /*inp.rows - 3*/; i += 7) {

      std::cout << i << std::endl;

      for (int j = 3; j < inp.cols - 3; j += 7) {
         if (homMask.at<uchar>(i, j) != 0) {
            continue;
         }

         std::vector<double> v;

         g_processed.clear();

         int yp = CLAMP(i-3,0,inp.rows);
         int yn = CLAMP(i+3+1,0,inp.rows);
         int xp = CLAMP(j-3,0,inp.cols);
         int xn = CLAMP(j+3+1,0,inp.cols);

         for (int y = yp; y < yn; ++y) {
            for (int x = xp; x < xn; ++x) {
               g_processed.insert(std::make_pair(y, x));
               v.push_back((double)inp.at<uchar>(y, x));
            }
         }

         if (xp > 0 && v.size()) {
            for (int y = yp; y < yn; ++y) {
               if (g_processed.count(std::make_pair(y, xp - 1)) == 0)
                  determineHomogenuousRegionsRecursive(y, xp-1, inp, processed, v, homMask);
            }
         }

         if (xn + 1 < inp.cols) {
            for (int y = yp; y < yn; ++y) {
               if (g_processed.count(std::make_pair(y, xn + 1)) == 0)
                  determineHomogenuousRegionsRecursive(y, xn + 1, inp, processed, v, homMask);
            }
         }

         if (yp > 0 && v.size()) {
            for (int x = xp; x < xn; ++x) {
               if (g_processed.count(std::make_pair(yp - 1, x)) == 0)
                  determineHomogenuousRegionsRecursive(yp - 1, x, inp, processed, v, homMask);
            }
         }

         if (yn + 1 < inp.rows) {
            for (int x = xp; x < xn; ++x) {
               if (g_processed.count(std::make_pair(yn + 1, x)) == 0)
                  determineHomogenuousRegionsRecursive(yn + 1, x, inp, processed, v, homMask);
            }
         }

         if (v.size() >= 128) {
            //homMask |= processed;
            for (auto it = g_processed.begin(); it != g_processed.end(); ++it) {
               std::pair<int,int> pt = *it;
               homMask.at<uchar>(pt.first, pt.second) = 255;
            }
         }
      }
   }
}


#define MY_BIN_COUNT 16
struct BinData
{
   BinData() { memset(bins, 0, sizeof(bins)); }
   int getBinForIntensity(double v) const { return (int)v / MY_BIN_COUNT; }
   double getDeviationForIntesity(double v) const { return bins[getBinForIntensity(v)]; }
   double bins[MY_BIN_COUNT];
   std::vector<double> deviations[MY_BIN_COUNT];
};


void analyzeNoise(const cv::Mat & inp, BinData & bd)
{
   for (int i = 1; i < inp.rows - 1; i += 3) {
      for (int j = 1; j < inp.cols - 1; j += 3) {
         std::vector<double> tv;
         for (int i1 = -1; i1 <= 1; ++i1)
            for (int j1 = -1; j1 <= 1; ++j1)
               tv.push_back((double)inp.at<uchar>(i+i1,j+j1));
         double mn = myMean(tv);
         double sdev = myStdDev(tv);
         bd.deviations[bd.getBinForIntensity(mn)].push_back(sdev);
      }
   }

   for (int i = 0; i < MY_BIN_COUNT; ++i) {
      if (bd.deviations[i].size() >= 20) {
         std::sort(bd.deviations[i].begin(), bd.deviations[i].end());
         bd.deviations[i].erase(bd.deviations[i].begin() + bd.deviations[i].size() / 20, bd.deviations[i].end());
         bd.bins[i] = myMean(bd.deviations[i]);
      }
   }
}


#define MY_NUMBER_GRIDS 2

static void mySmoothImageSaintMarc(cv::Mat & inp, const BinData &bd, int numIterations=10)
{
   auto wxy = [&inp, &bd](int row, int col) {
      double gx = 0.5 * ((double)inp.at<short>(row, col - 1) - (double)inp.at<short>(row, col + 1));
      double gy = 0.5 * ((double)inp.at<short>(row - 1, col) - (double)inp.at<short>(row + 1, col));
      double sigma = 2.0 * bd.getDeviationForIntesity((double)inp.at<short>(row, col));
      if (sigma < MYEPS) { return 0.0; }
      return exp(-(gx * gx + gy * gy) / (2 * sigma * sigma));
   };

   for (int iterationIndex = 0; iterationIndex < numIterations; ++iterationIndex) {
      for (int i = 4; i + 4 < inp.rows; i += 3) {
         for (int j = 4; j + 4 < inp.cols; j += 3) {
            double intensity = (double)inp.at<short>(i, j);
            double sigma = bd.getDeviationForIntesity(intensity);
            if (sigma < MYEPS) { continue; }

            double numer = 0.0;
            double denom = 0.0;
            for (int ii = -1; ii <= 1; ++ii) {
               for (int jj = -1; jj <= 1; ++jj) {

                  int y = i + ii;
                  int x = j + jj;
               
                  double w = wxy(y, x);
                  numer += (double)inp.at<short>(y, x) * w;
                  denom += w;
               }
            }

            if (denom > 0) {
               inp.at<short>(i, j) = cv::saturate_cast<short>(numer / denom);
            }
         }
      }
   }
}

static void myMultiGridSmoothing(cv::Mat & inp, const BinData &bd, int pyrIndex, int *numIterations)
{
   if (pyrIndex > MY_NUMBER_GRIDS) {
      return;
   }

   cv::Mat dst, dstm;
   cv::pyrDown(inp, dst); // cv::Size(inp.cols/2, inp.rows/2));

   dstm = dst;
   mySmoothImageSaintMarc(dstm, bd, numIterations[pyrIndex]);
   myMultiGridSmoothing(dstm, bd, pyrIndex + 1, numIterations);

   cv::Mat err, errNew;
   err = dstm - dst;

   cv::pyrUp(err, errNew, cv::Size(inp.cols, inp.rows));

   inp += errNew;
}

// saint-marc multigrid implementation
static void adaptiveSmoothing(cv::Mat & inp, const BinData &bd)
{
   int numIterations[1+MY_NUMBER_GRIDS] = { 20, 50, 100 };

   cv::Mat tmp;
   inp.convertTo(tmp, CV_16S);

   mySmoothImageSaintMarc(tmp, bd, numIterations[0]);
   myMultiGridSmoothing(tmp, bd, 1, numIterations);
   mySmoothImageSaintMarc(tmp, bd, 10);

   tmp.convertTo(inp, CV_8U);
}

void wallisFilter(cv::Mat & inp)
{
   const double c = 0.6;
   const double b = 0.75;
   const double sf = 50.0;
   const double mf = 72.0;
   const int wsize = 21;
   const double limit = 10.0;
   const double todev = 50.0;

   cv::Mat tmp = inp;

   for (int i = 0; i < inp.rows; ++i) {
      for (int j = 0; j < inp.cols; ++j) {
         double mg = myMean<uchar>(tmp, cv::Rect(j - wsize / 2, i - wsize / 2, wsize, wsize));
         double sg = myStdDev<uchar>(tmp, cv::Rect(j - wsize / 2, i - wsize / 2, wsize, wsize),mg);

         //double r1 = c * sf / (c * sg + sf / c);
         //double r0 = /*b * mf +*/ (1 - b - r1) * mg;

         //inp.at<uchar>(i, j) = cv::saturate_cast<uchar>((double)inp.at<uchar>(i, j) * r1 + r0);
         double curVal = (double)tmp.at<uchar>(i, j);
         uchar v = cv::saturate_cast<uchar>(mg + (curVal - mg) * todev / (todev/limit + sg));
         tmp.at<uchar>(i, j) = v;
      }
   }

   inp = tmp;
}


int main(int argc, char **argv) {
   //cv::CommandLineParser parser(argc, argv, "{i||}{o||}{help||}");
   //if (!parser.get<std::string>("help").empty()) {
   //   parser.printParams();
   //   return 1;
   //}

   //std::string inputImage = parser.get<std::string>("i");
   //std::string outputImage = parser.get<std::string>("o");

   common_init();

   if (argc != 5) {
      return 1;
   }

   MyCmdArgs args;
   args.Init(argv[1], argv[2], argv[3], argv[4]);

   for (int i = 0; i < args.GetImageCount(); ++i) {
      cv::Mat inpImage = cv::imread(args.GetImageByIndex(i).GetInputImageName("tif"), cv::IMREAD_GRAYSCALE);
      BinData bd, bd2;
      analyzeNoise(inpImage, bd);
      adaptiveSmoothing(inpImage, bd);
      wallisFilter(inpImage);
      cv::equalizeHist(inpImage, inpImage);
      cv::imwrite(args.GetImageByIndex(i).GetOutputImageName("jpg"), inpImage);
   }

   //cv::namedWindow("winshow");
   //myShowImage(inputImage);
   /*cv::Mat inpImage = cv::imread(inputImage, cv::IMREAD_GRAYSCALE);*/
   //std::cout << img.type() << std::endl;
   //cv::imshow("winshow", img);
   //cv::waitKey();

   //cv::Mat homogMask;
   //determineHomogenuousRegions(inpImage, homogMask);
   //cv::imwrite("homogMask.jpg", homogMask);

   //BinData bd, bd2;
   //analyzeNoise(inpImage, bd);
   //adaptiveSmoothing(inpImage, bd);
   ////analyzeNoise(inpImage, bd2);
   //wallisFilter(inpImage);
   //cv::equalizeHist(inpImage, inpImage);
   //cv::imwrite(outputImage, inpImage);

   //cv::imshow("winshow", inpImage);
   //cv::waitKey();

   return 0;
}