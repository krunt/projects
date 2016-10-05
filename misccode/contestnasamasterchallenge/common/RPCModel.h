
#ifndef RPC_MODEL_
#define RPC_MODEL_

#include "gdal.h"
#include <string>
#include "MyMath.h"

void geodetic_to_cartesian(double lon, double lat, double h, 
        double &x, double &y, double &z);
void geodetic_to_cartesian_init();
void cartesian_to_geodetic(double x, double y, double z, double &lon, double &lat, double &h);

struct RPCModel {
   void  Parse(const std::string &filename);
    void ParseXML(const std::string &filename);
    GDALRPCInfo rpcInfo;
    double offsx;
    double offsy;
};

class RPCCamera {
public:
    void Init(const std::string &filename);
    void Init(const RPCModel & model, double sx, double sy);

    bool ToPixelCoordinates(double lon, double lat, double h, double &pix_x, double &pix_y) const;
    bool ToGeodetic(double pix_x, double pix_y, double h, double &lon, double &lat) const;
    bool ToVector(double pix_x, double pix_y, MyVector3d &v, MyPoint3d &p) const;

    RPCModel GetRpcModel() const { return rpcModel; }
    double GetScaleX() const { return scaleX; }
    double GetScaleY() const { return scaleY; }
    double GetNadirAngle() const; //radians
    void SetScale(double sx, double sy) { scaleX = sx; scaleY = sy; }

private:
   void CreateTransforms();

    RPCModel rpcModel;
    void *forwardTransform;
    void *inverseTransform;
    double scaleX, scaleY; // 1,0.5,0.25
};

#endif
