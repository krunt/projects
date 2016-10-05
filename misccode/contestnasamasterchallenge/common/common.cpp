
#include "common.h"
#include "tinyxml.h"
#include "RPCModel.h"

void myShowImage(const std::string & name)
{
   cv::imshow("winshow", cv::imread(name));
}


KMLBounds myParseKML (const std::string& kml_file) 
{
   KMLBounds res;
   TiXmlDocument doc;
   doc.LoadFile(kml_file.c_str());

   TiXmlNode* document =  doc.FirstChildElement("kml")->FirstChildElement("Document");
   if (!document) {
      throw std::runtime_error("Failed to parse document");
   }

   TiXmlNode *placemark =  document->FirstChildElement("Placemark");
   if (!placemark)
   {
      throw std::runtime_error("Failed to parse placemark");
   }

   TiXmlNode* polygon =  placemark->FirstChildElement("Polygon");
   if (!polygon)
   {
      throw std::runtime_error("Failed to parse polygon");
   }

   TiXmlNode *outer_boundary_is =  polygon->FirstChildElement("outerBoundaryIs");
   if (!outer_boundary_is)
   {
      throw std::runtime_error("Failed to parse outerBoundaryIs");
   }

   TiXmlNode* linear_ring =  outer_boundary_is->FirstChildElement("LinearRing");
   if (!linear_ring)
   {
      throw std::runtime_error("Failed to parse LinearRing");
   }

   TiXmlNode *coords =  linear_ring->FirstChild ();
   if (!coords)
   {
      throw std::runtime_error("Failed to parse coords");
   }

   TiXmlNode *xml_contents = coords->FirstChild();
   if (!xml_contents) {
      throw std::runtime_error("Failed to parse coords contents");
   }

   std::string contents = xml_contents->Value();


   std::stringstream ss;
   ss << contents;

   res.m_minLat = 1e30;
   res.m_minLon = 1e30;
   res.m_maxLat = -1e30;
   res.m_maxLon = -1e30;

   for (;;) {
      std::string value;

      try {
         std::getline(ss, value, ',');
         double v = std::stod(value);
         res.m_minLon = std::min(res.m_minLon, v);
         res.m_maxLon = std::max(res.m_maxLon, v);
         std::getline(ss, value, ',');
         v = std::stod(value);
         res.m_minLat = std::min(res.m_minLat, v);
         res.m_maxLat = std::max(res.m_maxLat, v);
         std::getline(ss, value, ' ');
      } catch (std::invalid_argument& e) {
         break;
      }
   }

   return res;
}


void MyCmdArgsImage::Init(const std::string & baseName, const std::string &idir, 
                          const std::string &rdir, const std::string &odir)
{
   m_baseImageName = baseName;
   m_inputPathDir = idir;
   m_rpcPathDir = rdir;
   m_outputPathDir = odir;
}

std::string MyCmdArgsImage::GetInputImageName(const std::string & ext) const
{
   return m_inputPathDir + "/" + m_baseImageName + "." + ext;
}

std::string MyCmdArgsImage::GetOutputImageName(const std::string & ext) const
{
   return m_outputPathDir + "/" + m_baseImageName + "." + ext;
}

std::string MyCmdArgsImage::GetInputRpcName() const
{
   return m_rpcPathDir + "/rpc_" + m_baseImageName + "." + "txt";
}

void MyCmdArgs::Init(const std::string & kmlPath, const std::string & imagesPath, const std::string & rpcPath,
                    const std::string & outPath, const std::string &ext)
{
   m_kmlPath = kmlPath;
   std::vector<std::string> imagePaths;
   cv::glob(imagesPath + "/*." + ext, imagePaths);
   m_argImages.resize(imagePaths.size());
   for (int i = 0; i < imagePaths.size(); ++i) {
      int from = imagePaths[i].rfind('/');
      if (from == std::string::npos) {
         from = imagePaths[i].rfind('\\');
      }
      if (from == std::string::npos) {
         continue;
      }
      int to = imagePaths[i].rfind('.');
      m_argImages[i].Init(imagePaths[i].substr(from+1,to-from-1), imagesPath, rpcPath, outPath);
   }
   m_outPath = outPath;
   m_resolutionX = m_resolutionY = 0.3; // meters
}

double metricToGeodetic(const KMLBounds & bounds, double meters, bool isX)
{
   double x0,y0,z0,x1,y1,z1;
   geodetic_to_cartesian(bounds.m_minLon, bounds.m_minLat, 0, x0, y0, z0);

   x1 = isX ? (x0+meters) : x0;
   y1 = !isX ? (y0+meters) : y0;
   z1 = z0;
   
   double lon1,lat1,h1;
   cartesian_to_geodetic(x1,y1,z1,lon1,lat1,h1);

   return abs(lon1-bounds.m_minLon);
}

void common_init()
{
   geodetic_to_cartesian_init();
}