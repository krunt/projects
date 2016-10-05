
#include "RPCModel.h"
#include <gdal.h>
#include <gdal_priv.h>
#include <gdal_alg.h>

#include <ogr_spatialref.h>

#include <sstream>
#include <fstream>
#include <math.h>
#include <float.h>

#include "tinyxml.h""

static void readDouble(TiXmlNode *elem, double *b, int sz=1)
{
    const std::string s = elem->FirstChild()->ToText()->Value();
    std::stringstream ss;
    ss << s;
    for (int i = 0; i < sz; ++i)
        ss >> b[i];
}

void RPCModel::Parse(const std::string &filename) 
{
   std::fstream fd(filename, std::ios_base::in);
   fd.seekg(0, fd.end);
   int bytesCount = fd.tellg();
   fd.seekg(0, fd.beg);
   std::string content;
   content.resize(bytesCount);
   fd.read(&content[0], bytesCount);

   GDALRPCInfo & sRPC = rpcInfo;

   sscanf(&content[0],
      "%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf",
      &sRPC.dfLINE_OFF, 
      &sRPC.dfSAMP_OFF, 
      &sRPC.dfLAT_OFF, 
      &sRPC.dfLONG_OFF, 
      &sRPC.dfHEIGHT_OFF, 
      &sRPC.dfLINE_SCALE, 
      &sRPC.dfSAMP_SCALE, &sRPC.dfLAT_SCALE, &sRPC.dfLONG_SCALE, &sRPC.dfHEIGHT_SCALE,
      &sRPC.adfLINE_NUM_COEFF[0],&sRPC.adfLINE_NUM_COEFF[1],&sRPC.adfLINE_NUM_COEFF[2],&sRPC.adfLINE_NUM_COEFF[3],&sRPC.adfLINE_NUM_COEFF[4],&sRPC.adfLINE_NUM_COEFF[5],&sRPC.adfLINE_NUM_COEFF[6],&sRPC.adfLINE_NUM_COEFF[7],&sRPC.adfLINE_NUM_COEFF[8],&sRPC.adfLINE_NUM_COEFF[9],
      &sRPC.adfLINE_NUM_COEFF[10],&sRPC.adfLINE_NUM_COEFF[11],&sRPC.adfLINE_NUM_COEFF[12],&sRPC.adfLINE_NUM_COEFF[13],&sRPC.adfLINE_NUM_COEFF[14],&sRPC.adfLINE_NUM_COEFF[15],&sRPC.adfLINE_NUM_COEFF[16],&sRPC.adfLINE_NUM_COEFF[17],&sRPC.adfLINE_NUM_COEFF[18],&sRPC.adfLINE_NUM_COEFF[19],
      &sRPC.adfLINE_DEN_COEFF[0],&sRPC.adfLINE_DEN_COEFF[1],&sRPC.adfLINE_DEN_COEFF[2],&sRPC.adfLINE_DEN_COEFF[3],&sRPC.adfLINE_DEN_COEFF[4],&sRPC.adfLINE_DEN_COEFF[5],&sRPC.adfLINE_DEN_COEFF[6],&sRPC.adfLINE_DEN_COEFF[7],&sRPC.adfLINE_DEN_COEFF[8],&sRPC.adfLINE_DEN_COEFF[9],
      &sRPC.adfLINE_DEN_COEFF[10],&sRPC.adfLINE_DEN_COEFF[11],&sRPC.adfLINE_DEN_COEFF[12],&sRPC.adfLINE_DEN_COEFF[13],&sRPC.adfLINE_DEN_COEFF[14],&sRPC.adfLINE_DEN_COEFF[15],&sRPC.adfLINE_DEN_COEFF[16],&sRPC.adfLINE_DEN_COEFF[17],&sRPC.adfLINE_DEN_COEFF[18],&sRPC.adfLINE_DEN_COEFF[19],
      &sRPC.adfSAMP_NUM_COEFF[0],&sRPC.adfSAMP_NUM_COEFF[1],&sRPC.adfSAMP_NUM_COEFF[2],&sRPC.adfSAMP_NUM_COEFF[3],&sRPC.adfSAMP_NUM_COEFF[4],&sRPC.adfSAMP_NUM_COEFF[5],&sRPC.adfSAMP_NUM_COEFF[6],&sRPC.adfSAMP_NUM_COEFF[7],&sRPC.adfSAMP_NUM_COEFF[8],&sRPC.adfSAMP_NUM_COEFF[9],
      &sRPC.adfSAMP_NUM_COEFF[10],&sRPC.adfSAMP_NUM_COEFF[11],&sRPC.adfSAMP_NUM_COEFF[12],&sRPC.adfSAMP_NUM_COEFF[13],&sRPC.adfSAMP_NUM_COEFF[14],&sRPC.adfSAMP_NUM_COEFF[15],&sRPC.adfSAMP_NUM_COEFF[16],&sRPC.adfSAMP_NUM_COEFF[17],&sRPC.adfSAMP_NUM_COEFF[18],&sRPC.adfSAMP_NUM_COEFF[19],
      &sRPC.adfSAMP_DEN_COEFF[0],&sRPC.adfSAMP_DEN_COEFF[1],&sRPC.adfSAMP_DEN_COEFF[2],&sRPC.adfSAMP_DEN_COEFF[3],&sRPC.adfSAMP_DEN_COEFF[4],&sRPC.adfSAMP_DEN_COEFF[5],&sRPC.adfSAMP_DEN_COEFF[6],&sRPC.adfSAMP_DEN_COEFF[7],&sRPC.adfSAMP_DEN_COEFF[8],&sRPC.adfSAMP_DEN_COEFF[9],
      &sRPC.adfSAMP_DEN_COEFF[10],&sRPC.adfSAMP_DEN_COEFF[11],&sRPC.adfSAMP_DEN_COEFF[12],&sRPC.adfSAMP_DEN_COEFF[13],&sRPC.adfSAMP_DEN_COEFF[14],&sRPC.adfSAMP_DEN_COEFF[15],&sRPC.adfSAMP_DEN_COEFF[16],&sRPC.adfSAMP_DEN_COEFF[17],&sRPC.adfSAMP_DEN_COEFF[18],&sRPC.adfSAMP_DEN_COEFF[19],
      &sRPC.dfMIN_LONG, &sRPC.dfMIN_LAT, &sRPC.dfMAX_LONG, &sRPC.dfMAX_LAT, &offsx, &offsy);
}

void RPCModel::ParseXML(const std::string &filename) 
{
    TiXmlDocument doc(filename);
    doc.LoadFile();

    TiXmlNode *root = doc.FirstChildElement("isd");
    TiXmlNode *image = root->FirstChildElement("RPB")->FirstChildElement("IMAGE");
    readDouble(image->FirstChildElement("SAMPOFFSET"), &rpcInfo.dfSAMP_OFF);
    readDouble(image->FirstChildElement("LINEOFFSET"), &rpcInfo.dfLINE_OFF);
    readDouble(image->FirstChildElement("SAMPSCALE"), &rpcInfo.dfSAMP_SCALE);
    readDouble(image->FirstChildElement("LINESCALE"), &rpcInfo.dfLINE_SCALE);
    readDouble(image->FirstChildElement("LONGOFFSET"), &rpcInfo.dfLONG_OFF);
    readDouble(image->FirstChildElement("LATOFFSET"), &rpcInfo.dfLAT_OFF);
    readDouble(image->FirstChildElement("HEIGHTOFFSET"), &rpcInfo.dfHEIGHT_OFF);
    readDouble(image->FirstChildElement("LONGSCALE"), &rpcInfo.dfLONG_SCALE);
    readDouble(image->FirstChildElement("LATSCALE"), &rpcInfo.dfLAT_SCALE);
    readDouble(image->FirstChildElement("HEIGHTSCALE"), &rpcInfo.dfHEIGHT_SCALE);

    readDouble(image->FirstChildElement("LINENUMCOEFList")->FirstChildElement("LINENUMCOEF"), rpcInfo.adfLINE_NUM_COEFF, 20);
    readDouble(image->FirstChildElement("LINEDENCOEFList")->FirstChildElement("LINEDENCOEF"), rpcInfo.adfLINE_DEN_COEFF, 20);
    readDouble(image->FirstChildElement("SAMPNUMCOEFList")->FirstChildElement("SAMPNUMCOEF"), rpcInfo.adfSAMP_NUM_COEFF, 20);
    readDouble(image->FirstChildElement("SAMPDENCOEFList")->FirstChildElement("SAMPDENCOEF"), rpcInfo.adfSAMP_DEN_COEFF, 20);

    TiXmlNode *bbox = root->FirstChildElement("IMD")->FirstChildElement("BAND_P");
    readDouble(bbox->FirstChildElement("ULLON"), &rpcInfo.dfMIN_LONG);
    readDouble(bbox->FirstChildElement("URLON"), &rpcInfo.dfMAX_LONG);
    readDouble(bbox->FirstChildElement("ULLAT"), &rpcInfo.dfMAX_LAT);
    readDouble(bbox->FirstChildElement("LLLAT"), &rpcInfo.dfMIN_LAT);

    TiXmlNode *addoffs = root->FirstChildElement("ADDOFFS");
    readDouble(addoffs->FirstChildElement("OFFSX"), &offsx);
    readDouble(addoffs->FirstChildElement("OFFSY"), &offsy);
}

void RPCCamera::Init(const std::string &filename)
{ 
   rpcModel.Parse(filename); 
   scaleX = scaleY = 1.0;
   CreateTransforms();
}

void RPCCamera::CreateTransforms()
{
   forwardTransform = GDALCreateRPCTransformer(&rpcModel.rpcInfo, false, 0.1,  nullptr);
   inverseTransform = GDALCreateRPCTransformer(&rpcModel.rpcInfo, true, 0.1,  nullptr);
}

void RPCCamera::Init(const RPCModel & model, double sx, double sy)
{
   rpcModel = model;
   scaleX = sx;
   scaleY = sy;
   CreateTransforms();
}

bool RPCCamera::ToPixelCoordinates(double lon, double lat, double h, double &pix_x, double &pix_y) const
{
    int success = 0;
    GDALRPCTransform(forwardTransform, true, 1, &lon, &lat, &h, &success);
    pix_x = (lon - rpcModel.offsx) * scaleX;
    pix_y = (lat - rpcModel.offsy) * scaleY;
    return true;
}

bool RPCCamera::ToGeodetic(double pix_x, double pix_y, double h, double &lon, double &lat) const
{
    int success = 0;
    lon = pix_x/scaleX + rpcModel.offsx;
    lat = pix_y/scaleY + rpcModel.offsy;
    GDALRPCTransform(inverseTransform, true, 1, &lon, &lat, &h, &success);
    return true;
}


double RPCCamera::GetNadirAngle() const
{
   MyPoint3d camPnt; MyVector3d camVec;
   ToVector(100, 100, camVec, camPnt);
   MyVector3d vpnt = -camPnt;
   MyVectorNormalize(vpnt);
   return acos(MyVectorDot(vpnt, camVec));
}


bool RPCCamera::ToVector(double pix_x, double pix_y, MyVector3d &v, MyPoint3d &p) const
{
    GDALRPCInfo rpcInfo = rpcModel.rpcInfo;
    double hup = rpcInfo.dfHEIGHT_OFF + rpcInfo.dfHEIGHT_SCALE * 0.9;
    double hdown = rpcInfo.dfHEIGHT_OFF - rpcInfo.dfHEIGHT_SCALE * 0.9;
    double hup_lon, hup_lat, hdown_lon, hdown_lat;
    ToGeodetic(pix_x, pix_y, hup, hup_lon, hup_lat);
    ToGeodetic(pix_x, pix_y, hdown, hdown_lon, hdown_lat);
    double hup_x, hup_y, hup_z, hdown_x, hdown_y, hdown_z;
    geodetic_to_cartesian(hup_lon, hup_lat, hup, hup_x, hup_y, hup_z);
    geodetic_to_cartesian(hdown_lon, hdown_lat, hdown, hdown_x, hdown_y, hdown_z);
    v = MyVectorSub(MyVector3d(hdown_x, hdown_y, hdown_z),
        MyVector3d(hup_x, hup_y, hup_z));
    MyVectorNormalize(v);

    p.p[0] = hup_x - 10000 * v.p[0];
    p.p[1] = hup_y - 10000 * v.p[1];
    p.p[2] = hup_z - 10000 * v.p[2];

    return true;
}

static double meridian_offset;
static double semi_major_axis;
static double semi_minor_axis;

void geodetic_to_cartesian_init()
{
   OGRSpatialReference gdal_spatial_ref;
   if (gdal_spatial_ref.SetFromUserInput( "+proj=longlat +datum=WGS84 +no_defs" ))
      throw std::runtime_error("failed to initialized gdal_spatial_ref");
   OGRErr e1, e2;
   semi_major_axis = gdal_spatial_ref.GetSemiMajor(&e1);
   semi_minor_axis = gdal_spatial_ref.GetSemiMinor(&e2);
   assert(e1 != OGRERR_FAILURE && e2 != OGRERR_FAILURE);
   meridian_offset = gdal_spatial_ref.GetPrimeMeridian();
}

void geodetic_to_cartesian(double lon, double lat, double h, 
        double &x, double &y, double &z)
{
    double a  = semi_major_axis;
    double b  = semi_minor_axis;
    double a2 = a * a;
    double b2 = b * b;
    double e2 = (a2 - b2) / a2;
  
    if ( lat < -90 ) lat = -90;
    if ( lat >  90 ) lat = 90;
  
    double rlon = (lon + meridian_offset) * (M_PI/180);
    double rlat = lat * (M_PI/180);
    double slat = sin( rlat );
    double clat = cos( rlat );
    double slon = sin( rlon );
    double clon = cos( rlon );
    double radius = a / sqrt(1.0-e2*slat*slat);
  
    x = (radius+h) * clat * clon;
    y = (radius+h) * clat * slon;
    z = (radius*(1-e2)+h) * slat;
}

void cartesian_to_geodetic(double x, double y, double z, double &lon, double &lat, double &h) 
{
   const double a2 = semi_major_axis * semi_major_axis;
   const double b2 = semi_minor_axis * semi_minor_axis;
   const double e2 = 1 - b2 / a2;
   const double e4 = e2 * e2;

   double xy_dist = sqrt( x * x + y * y );
   double p = ( x * x + y * y ) / a2;
   double q = ( 1 - e2 ) * z * z / a2;
   double r = ( p + q - e4 ) / 6.0;
   double r3 = r * r * r;

   double evolute = 8 * r3 + e4 * p * q;
   double u = std::numeric_limits<double>::quiet_NaN();
   if ( evolute > 0 ) {
      // outside the evolute
      double right_inside_pow = sqrt(e4 * p * q);
      double sqrt_evolute = sqrt( evolute );
      u = r + 0.5 * pow(sqrt_evolute + right_inside_pow,2.0/3.0) +
         0.5 * pow(sqrt_evolute - right_inside_pow,2.0/3.0);
   } else if ( fabs(z) < std::numeric_limits<double>::epsilon() ) {
      // On the equator plane
      lat = 0;
      h = sqrt(x*x+y*y+z*z) - semi_major_axis;
   } else if ( evolute < 0 && fabs(q) > std::numeric_limits<double>::epsilon() ) {
      // On or inside the evolute
      double atan_result = atan2( sqrt( e4 * p * q ), sqrt( -evolute ) + sqrt(-8 * r3) );
      u = -4 * r * sin( 2.0 / 3.0 * atan_result ) *
         cos( M_PI / 6.0 + 2.0 / 3.0 * atan_result );
   } else if ( fabs(q) < std::numeric_limits<double>::epsilon() && p <= e4 ) {
      // In the singular disc
      h = -semi_major_axis * sqrt(1 - e2) * sqrt(e2 - p) / sqrt(e2);
      lat = 2 * atan2( sqrt(e4 - p), sqrt(e2*(e2 - p)) + sqrt(1-e2) * sqrt(p) );
   } else {
      // Near the cusps of the evolute
      double inside_pow = sqrt(evolute) + sqrt(e4 * p * q);
      u = r + 0.5 * pow(inside_pow,2.0/3.0) +
         2 * r * r * pow(inside_pow,-2.0/3.0);
   }

   {
      double v   = sqrt( u * u + e4 * q );
      double u_v = u + v;
      double w   = e2 * ( u_v - q ) / ( 2 * v );
      double k   = u_v / ( w + sqrt( w * w + u_v ) );
      double D   = k * xy_dist / ( k + e2 );
      double dist_2 = D * D + z * z;
      h = ( k + e2 - 1 ) * sqrt( dist_2 ) / k;
      lat = 2 * atan2( z, sqrt( dist_2 ) + D );
   }

   if ( xy_dist + x > ( sqrt(2) - 1 ) * y ) {
      // Longitude is between -135 and 135
      lon = 360.0 * atan2( y, xy_dist + x ) / M_PI;
   } else if ( xy_dist + y < ( sqrt(2) + 1 ) * x ) {
      // Longitude is between -225 and 45
      lon = - 90.0 + 360.0 * atan2( x, xy_dist - y ) / M_PI;
   } else {
      // Longitude is between -45 and 225
      lon = 90.0 - 360.0 * atan2( x, xy_dist + y ) / M_PI;
   }
   lon -= meridian_offset;
   lat *= 180.0 / M_PI;
}