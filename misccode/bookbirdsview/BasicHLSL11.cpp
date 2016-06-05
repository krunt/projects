//--------------------------------------------------------------------------------------
// File: BasicHLSL11.cpp
//
// This sample shows a simple example of the Microsoft Direct3D's High-Level 
// Shader Language (HLSL) using the Effect interface. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include "Camera.h"
#include "BookStand.h"

#include "d3lib/Lib.h"
#include "d3lib/Model_ase.h"

#include <vector>
#include <sstream>
#include <algorithm>

DXGI_FORMAT g_depthStencilFormat;
UINT g_backbufferWidth, g_backbufferHeight;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
//CModelViewerCamera          g_Camera;               // A model viewing camera
CCamera                     g_Camera;               // A model viewing camera
CModelViewerCamera          g_FlipCamera;               // A model viewing camera
CDXUTDirectionWidget        g_LightControl;
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
D3DXMATRIXA16               g_mCenterMesh;
float                       g_fLightScale;
int                         g_nNumActiveLights;
int                         g_nActiveLight;
bool                        g_bShowHelp = false;    // If true, it renders the UI control text

float                       g_window_width, g_window_height;

float g_zFar= 128000.0f;
float g_fAspectRatio=1.0f;
float g_field_of_view= D3DX_PI / 2;

D3DXVECTOR4 g_vecEye2(600.0f, 600.0f, 650.0f, 0.0f);

// Direct3D9 resources
CDXUTTextHelper*            g_pTxtHelper = NULL;

CDXUTSDKMesh                g_Mesh11;

const  int NUM_PAGES = 1024;
BookStand                   g_bookStand(NUM_PAGES);

ID3D11InputLayout*          g_pVertexLayout11 = NULL, *g_pEdgeVertexLayout11= NULL;
ID3D11Buffer*               g_pVertexBuffer = NULL;
ID3D11Buffer*               g_pIndexBuffer = NULL;
ID3D11VertexShader*         g_pVertexShader = NULL;
ID3D11PixelShader*          g_pPixelShader = NULL;
ID3D11VertexShader*         g_pEdgeVertexShader = NULL;
ID3D11PixelShader*          g_pEdgePixelShader = NULL;
ID3D11VertexShader*         g_pLightVertexShader = NULL;
ID3D11PixelShader*          g_pLightPixelShader = NULL;
ID3D11SamplerState*         g_pSamLinear = NULL;
ID3D11RasterizerState *     g_pRasterizerState=NULL;
ID3D11DepthStencilState  *  g_pDepthStencilState= NULL;
ID3D11VertexShader*         g_pSSAODepthVertexShader= NULL;
ID3D11PixelShader*          g_pSSAODepthPixelShader= NULL;
ID3D11VertexShader*         g_pSSAOPassVertexShader= NULL;
ID3D11PixelShader*          g_pSSAOPassPixelShader= NULL;

ID3D11Texture2D* g_renderTargetTextureMap= NULL;
ID3D11RenderTargetView* g_renderTargetViewMap= NULL;
ID3D11DepthStencilView* g_depthStencilTargetViewMap= NULL;
ID3D11ShaderResourceView* g_shaderResourceViewMap= NULL;

ID3D11ShaderResourceView* g_modelTextureRV= NULL;

D3DXMATRIX g_mWorldViewProjection;
D3DXMATRIX g_WorldInvTransposeView;
D3DXMATRIX g_mWorldView;
D3DXMATRIX g_ViewToTexSpace;
D3DXMATRIX g_mWorld;
D3DXMATRIX g_mView;
D3DXMATRIX g_mProj;
D3DXMATRIX g_mViewProj;

float g_figureAngle= 0;

bool g_needPhysicsUpdate= false;

struct CB_VS_PER_OBJECT
{
    D3DXMATRIX m_WorldViewProj;
    D3DXMATRIX m_WorldView;
    D3DXMATRIX m_WorldInvTransposeView;
    D3DXMATRIX m_ViewToTexSpace;
    D3DXVECTOR4 m_CameraOrigin;
};
UINT                        g_iCBVSPerObjectBind = 0;

struct CB_PS_PER_OBJECT
{
    D3DXVECTOR4 m_vObjectColor;
};
UINT                        g_iCBPSPerObjectBind = 0;

struct CB_PS_PER_FRAME
{
    D3DXVECTOR4 m_vLightDirAmbient;
};
UINT                        g_iCBPSPerFrameBind = 1;

ID3D11Buffer*               g_pcbVSPerObject = NULL;
ID3D11Buffer*               g_pcbPSPerObject = NULL;
ID3D11Buffer*               g_pcbPSPerFrame = NULL;

enum e_curve_type
{
   _polynom_curve_type,
   _bezier_curve_type,
   _bspline_curve_type,
};

enum e_surface_type
{
   _polynom_surface_type,
   _bezier_surface_type,
   _bspline_surface_type,
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice,
                                            const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                           void* pUserContext );
extern void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime,
                                        void* pUserContext );
extern void CALLBACK OnD3D9LostDevice( void* pUserContext );
extern void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

struct CRay
{
   static CRay FromPoints(const D3DXVECTOR3 &v1, const D3DXVECTOR3 &v2)
   {
      CRay res;

      res.m_origin= v1;
      res.m_direction= v2-v1;
      res.m_maxt= D3DXVec3Length(&res.m_direction);

      D3DXVec3Normalize(&res.m_direction, &res.m_direction);

      return res;
   }

   static CRay FromOriginDirection(const D3DXVECTOR3 &origin, const D3DXVECTOR3 &direction)
   {
      CRay res;

      res.m_origin= origin;
      res.m_direction= direction;
      res.m_maxt= D3DXVec3Length(&direction);

      D3DXVec3Normalize(&res.m_direction, &res.m_direction);

      return res;
   }

   static CRay FromOriginDirection(const idVec3 &origin, const idVec3 &direction)
   {
      CRay res;

      res.m_origin= D3DXVECTOR3(origin.x,origin.y,origin.z);
      res.m_direction= D3DXVECTOR3(direction.x,direction.y,direction.z);
      res.m_maxt= D3DXVec3Length(&res.m_direction);

      D3DXVec3Normalize(&res.m_direction, &res.m_direction);

      return res;
   }

   bool IntersectsWithTriangle(float &rayt, const D3DXVECTOR3 &v1, const D3DXVECTOR3 &v2, const D3DXVECTOR3 &v3) const
   {
      D3DXVECTOR3 e1, e2, s1;

      e1= v2-v1;
      e2= v3-v1;

      D3DXVec3Cross(&s1, &m_direction, &e2);

      float divisor= D3DXVec3Dot(&s1, &e1);

      if (divisor==0.)
         return false;

      float inv_divisor= 1/divisor;

      D3DXVECTOR3 s= m_origin-v1;
      float b1= D3DXVec3Dot(&s, &s1) * inv_divisor;
      if (b1<0.||b1>1.)
         return false;

      D3DXVECTOR3 s2;
      D3DXVec3Cross(&s2, &s, &e1);
      float b2= D3DXVec3Dot(&m_direction, &s2) * inv_divisor;
      if (b2<0.||b1+b2>1.)
         return false;

      float t= D3DXVec3Dot(&e2, &s2) * inv_divisor;
      if (t<0||t>m_maxt)
         return false;

      rayt= t;
      return true;
   }

   bool IntersectsWithTriangle(float &rayt, const D3DXVECTOR4 &v1, const D3DXVECTOR4 &v2, const D3DXVECTOR4 &v3) const
   {
      D3DXVECTOR3 v11, v22, v33;

      v11= D3DXVECTOR3(v1.x, v1.y, v1.z);
      v22= D3DXVECTOR3(v2.x, v2.y, v2.z);
      v33= D3DXVECTOR3(v3.x, v3.y, v3.z);

      bool res= IntersectsWithTriangle(rayt, v11, v22, v33);

      return res;
   }

   idVec3 GetOrigin() const 
   {
      return idVec3(m_origin.x, m_origin.y, m_origin.z);
   }

   idVec3 GetDirection() const 
   {
      return idVec3(m_direction.x, m_direction.y, m_direction.z);
   }

   float GetMaxT() const
   {
      return m_maxt;
   }

   D3DXVECTOR3 m_origin;
   D3DXVECTOR3 m_direction;
   float m_maxt;
};

struct CVertex
{
   CVertex() 
   {
      SetVertices(0,0,0);
      SetNormal(0,0,0);
      SetTexCoord(0,0);
      SetColor(1,1,1,1);
   }

   float v[3], n[3], t[2], c[3];

   void SetVertices(float x, float y, float z)
   {
      v[0]= x; v[1]= y; v[2]= z;
   }

   void SetNormal(float x, float y, float z)
   {
      n[0]= x; n[1]= y; n[2]= z;
   }

   void SetTexCoord(float x, float y)
   {
      t[0]= x; t[1]= y;
   }

   void SetColor(float r, float g, float b,float a)
   {
      c[0]= r; c[1]= g; c[2]= b;
   }

   void GetColor(float *rgb)
   {
      rgb[0]= c[0];
      rgb[1]= c[1];
      rgb[2]= c[2];
   }
};

struct STetrahedron
{
   long index[4];
   long neighbors[4];

   void SetVertices(int i0, int i1, int i2, int i3)
   {
      index[0]= i0;
      index[1]= i1;
      index[2]= i2;
      index[3]= i3;
   }

   void SetNeighbors(int i0, int i1, int i2, int i3)
   {
      neighbors[0]= i0;
      neighbors[1]= i1;
      neighbors[2]= i2;
      neighbors[3]= i3;
   }
};

class CMeshNode;

class CTetGenMeshNodeSerializer
{
public:
   bool Serialize(const CMeshNode &mesh, const char *out_path);
};

class CTetGenEleDeserializer
{
public:
   bool Deserialize(CMeshNode &mesh, const char *in_path, const char *neighbor_in_path);
};

class CTetGenNodeDeserializer
{
public:
   bool Deserialize(CMeshNode &mesh, const char *in_path);
};

class CBaseCurve
{
public:
   CBaseCurve()
   {
      m_dimension= 3;

      m_arr[0]= idVec3(1.0f,0.0f,0.0f);
      m_arr[1]= idVec3(1.0f,1.0f,0.0f);
      m_arr[2]= idVec3(0.0f,1.0f,0.0f);

      m_weights[0]= 1.0f;
      m_weights[1]= 1.0f;
      m_weights[2]= 6.0f;
   }

   virtual idVec3 Evaluate(float t) = 0;

   virtual void Evaluate(std::vector<idVec3> &points) 
   {
      assert(points.size()>=2);

      float step= 1.0f/(points.size()-1);
      float t=0.0f;

      for (int i=0; i<points.size(); ++i, t=min(t+step,1.0f))
      {
         points[i]= Evaluate(t);
      }
   }

   idVec3 m_arr[128];
   int m_dimension;
   float m_weights[128];
};

class CPolynomCurve: public CBaseCurve
{
public:

   virtual idVec3 Evaluate(float t) 
   {
      idVec3 res= m_arr[m_dimension-1];
      for (int i=m_dimension-2; i>=0; --i)
      {
         res= res*t+m_arr[i];
      }
      return res;
   }


};

class CBezierCurve: public CBaseCurve
{
public:
   virtual idVec3 Evaluate(float t) 
   {
      idVec3 res(0.0f,0.0f,0.0f);

      /*
      SetupBernstein(t);

      for (int i=0; i<m_dimension; ++i)
      {
         res+= m_bm[m_dimension-1][i] * m_arr[i];
      }

      */

      //res= EvaluateCasteljau(t);
      //return res;


      SetupBernstein(t);

      float denom= 0.0f;

      for (int i=0; i<m_dimension; ++i)
      {
         float k= m_bm[m_dimension-1][i] * m_weights[i];

         res+= k * m_arr[i];
         denom+= k;
      }
      
      return res/denom;
   }

   void SetupBernstein(float t)
   {
      memset(m_bm,0,sizeof(m_bm));

      m_bm[0][0]= 1.0f;
      m_bm[1][0]= 1-t;
      m_bm[1][1]= t;

      for (int n=2;n<10;++n)
      {
         m_bm[n][0]= (1-t)*m_bm[n-1][0];

         for (int i=1;i<=n;++i)
         {
            m_bm[n][i]= (1-t)*m_bm[n-1][i]+t*m_bm[n-1][i-1];
         }
      }
   }

   idVec3 EvaluateCasteljau(float t)
   {
      idVec3 points[128];

      memcpy(&points[0], m_arr, 128*sizeof(idVec3));

      for (int k=1;k<m_dimension;++k)
      {
         for (int i=0; i<m_dimension-k; ++i)
         {
            points[i]= (1-t)*points[i] + t*points[i+1];
         }
      }
      return points[0];
   }

private:
   float m_bm[10][10];
};

class CBSplineCurve: public CBaseCurve
{
public:
   CBSplineCurve()
   {
      const int nk= 3;
      float u[nk]= {0.25f,0.5f,0.75f};

      m_p= 3;
      m_m= 2*(m_p+1)+nk;
      m_n= m_m-(m_p+1);

      memset(m_spans, 0, sizeof(m_spans));

      float *ptr= m_spans;
      for (int i=0; i<m_p+1; ++i)
      {
         *ptr++= 0.0f;
      }

      for (int i= m_p+1; i<m_m-m_p-1; ++i)
      {
         *ptr++= u[i-m_p-1];
      }

      for (int i=0; i<m_p+1; ++i)
      {
         *ptr++= 1.0f;
      }

      m_dimension= m_n;
      m_arr[0]= idVec3(0.0f, 0.3f, 0.0f);
      m_arr[1]= idVec3(0.2f, 0.3f, 0.0f);
      m_arr[2]= idVec3(0.2f, 0.6f, 0.0f);
      m_arr[3]= idVec3(0.6f, 0.6f, 0.0f);
      m_arr[4]= idVec3(0.8f, 0.25f, 0.0f);
      m_arr[5]= idVec3(0.5f, 0.0f, 0.0f);
      m_arr[6]= idVec3(0.2f, 0.1f, 0.0f);
   }

   virtual idVec3 Evaluate(float t) 
   {
      idVec3 res(0.0f,0.0f,0.0f);

      std::vector<float> bval(m_p+1,0.0f);
      int span= FindSpan(t);
      BasisFuns(span, t, bval);
      for (int i=0;i <=m_p; ++i)
      {
         assert(span-m_p+i>=0);
         res+= bval[i]*m_arr[span-m_p+i];
      }
      return res;
   }

   int FindSpan(float u)
   {
      int low, high, mid;

      if (u>=m_spans[m_n-1])
         return m_n-1;

      low= 0; high= m_n;
      mid= (low+high)>>1;
      while (u<m_spans[mid]||u>=m_spans[mid+1])
      {
         if (u<m_spans[mid]) high= mid;
         else                    low= mid;

         mid= (low+high)>>1;
      }
      return mid;
   }

   void BasisFuns(int i, float t, std::vector<float> &bval)
   {
      float left[128]={0};
      float right[128]={0};
      bval.resize(m_p+1,0.0f);

      bval[0]=  1.0f;
      for (int j=1;j<=m_p;++j)
      {
         left[j]= t-m_spans[i+1-j];
         right[j]= m_spans[i+j]-t;

         float saved= 0.0f;
         for (int r=0;r<j;++r)
         {
            float temp= bval[r]/(right[r+1]+left[j-r]);
            bval[r]= saved+right[r+1]*temp;
            saved= left[j-r]*temp;
         }
         bval[j]= saved;
      }
   }

   float MultiplicityForKnot(int span) const
   {
      return max(0, m_p - (span-(m_p+1)));
   }



   void CurveKnotInsert(float t, int r)
   {
      // s - multiplicity
      // r - number of insertions
      // np - m_n
      // p - m_p
      // k - span
      // u - t                        
      // Pw - m_arr
      // UP - m_spans
      idVec3 new_arr[128];   // Qw
      float  new_spans[128]; // UQ
      idVec3  rw[128];        // Rw

      memset((void*)rw, 0, sizeof(rw));

      int span= FindSpan(t);
      int s= MultiplicityForKnot(span);
      int mp= m_m;
      int nq= m_n+r;

      for (int i=0; i<=span; ++i) { new_spans[i]= m_spans[i]; }
      for (int i=1; i<=r; ++i) { new_spans[span+i]= t; }
      for (int i=span+1; i<mp; ++i) { new_spans[i+r]= m_spans[i]; }

      for (int i=0; i<=span-m_p; ++i) { new_arr[i]= m_arr[i]; }
      for (int i=span-s; i<=m_n; ++i) { new_arr[i+r]= m_arr[i]; }

      for (int i=0; i<=m_p-s; ++i) { rw[i]= m_arr[span-m_p+i]; }

      int ll= 0;
      for (int j=1; j<=r; ++j)
      {
         ll= span-m_p+j;
         for (int i=0; i<=m_p-j-s; ++j)
         {
            float alpha= (t-m_spans[ll+i])/(m_spans[i+span+1]-m_spans[ll+i]);
            rw[i]= alpha*rw[i+1] + (1-alpha)*rw[i];
         }
         new_arr[ll]= rw[0];
         //new_arr[span+r-j-s]= rw[m_p-j-s];
      }

      for (int i=ll+1; i<span-s; ++i)
         new_arr[i]= rw[i-ll];

      memcpy(m_arr, new_arr, sizeof(new_arr));
      memcpy(m_spans, new_spans, sizeof(new_spans));

      m_n= m_n+r;
      m_m= m_n+(m_p+1);
   }

   idVec3 CurvePntByCornerCut(float t)
   {
      idVec3 res(0,0,0);
      idVec3 rw[128];

      if (t<= m_spans[0])
      {
         return res= m_arr[0];
      }

      if (t>= m_spans[m_m-1])
      {
         return res= m_arr[m_m-1];
      }

      int span= FindSpan(t);
      int s= MultiplicityForKnot(span);
      int r= m_p-s;

      for (int i=0; i<=r; ++i) { rw[i]= m_arr[span-m_p+i]; }

      for (int j=1; j<=r; ++j) 
      for (int i=0; i<=r-j; ++i)
      {
         float alpha= (t-m_spans[span-m_p+j+i])/(m_spans[i+span+1]-m_spans[span-m_p+j+i]);
         rw[i]= alpha*rw[i+1] + (1-alpha)*rw[i];
      }

      return rw[0];
   }

   // to np derivatives
   void CurveDerivsAlg1(float t, int np, std::vector<idVec3> &bval)
   {
      std::vector<std::vector<float> > ders;

      bval.resize(np+1,idVec3(0.0f,0.0f,0.0f));

      int du= min(m_p,np);

      int span= FindSpan(t);
      DersBasisFuns(span, t, np, ders);
      for (int k=0; k<=du; ++k)
      {
         bval[k]= idVec3(0.0f,0.0f,0.0f);
         for (int j=0; j<=m_p; ++j)
         {
            bval[k]+= ders[k][j]*m_arr[span-m_p+j];
         }
      }
   }

   // up to np derivatives
   void DersBasisFuns(int i, float t, int np, std::vector<std::vector<float> > &ders)
   {
      assert(np<=m_p);

      float ndu[128][128]= {0};
      float left[128]={0}, right[128]={0};
      float a[2][128]= {0};

      ders.resize(m_p+1,std::vector<float>(m_p+1,0.0f));

      ndu[0][0]= 1.0f;
      for (int j=1; j<=m_p; ++j)
      {
         left[j]= t-m_spans[i+1-j];
         right[j]= m_spans[i+j]-t;

         float saved= 0.0f;
         for (int r=0; r<j; ++r)
         {
            ndu[j][r]= right[r+1]+left[j-r];
            float temp= ndu[r][j-1]/ndu[j][r];

            ndu[r][j]= saved+right[r+1]*temp;
            saved= left[j-r]*temp;
         }
         ndu[j][j]= saved;
      }

      for (int j=0;j<m_p;++j)
         ders[0][j]= ndu[j][m_p];

      int j1=0, j2=0;
      int s1=0, s2=1;
      for (int r=0; r<=m_p; ++r)
      {
         s1=0; s2=1;

         a[0][0]= 1.0f;
         for (int k=1; k<=np; ++k)
         {
            float d=0.0f;

            int rk= r-k;
            int pk= m_p-k;

            if (r>=k)
            {
               a[s2][0]= a[s1][0]/ndu[pk+1][rk];
               d= a[s2][0]*ndu[rk][pk];
            }

            if (rk>=-1) j1= 1;
            else        j1= -rk;

            if (r-1<=pk) j2= k-1;
            else         j2= m_p-r;

            for (int j=j1; j<=j2; ++j)
            {
               a[s2][j]= (a[s1][j]- a[s1][j-1])/ndu[rk+j][pk];
               d+= a[s2][j]*ndu[rk+j][pk];
            }
            if (r<=pk)
            {
               a[s2][k]= -a[s1][k-1]/ndu[pk+1][r];
               d+= a[s2][k]*ndu[r][pk];
            }
            ders[k][r]= d;
            std::swap(s1,s2);
         }
      }

      int r=m_p;
      for (int k=1;k<=np;++k)
      {
         for (int j=0;j<=m_p;++j) ders[k][j]*= r;
         r*= (m_p-k);
      }
   }

private:
   int m_n, m_p, m_m;
   float m_spans[128];
};

class CBaseSurface
{
public:
   CBaseSurface()
   {
      m_dimensionx= 3;
      m_dimensiony= 3;

      m_arr[0][0]= idVec3(0.0f,0.0f,0.0f);
      m_arr[0][1]= idVec3(0.5f,0.0f,0.0f);
      m_arr[0][2]= idVec3(1.0f,0.0f,0.0f);

      m_arr[1][0]= idVec3(0.0f,0.0f,0.5f);
      m_arr[1][1]= idVec3(0.5f,0.8f,0.5f);
      m_arr[1][2]= idVec3(1.0f,0.0f,0.5f);

      m_arr[2][0]= idVec3(0.0f,0.0f,1.0f);
      m_arr[2][1]= idVec3(0.5f,0.0f,1.0f);
      m_arr[2][2]= idVec3(1.0f,0.0f,1.0f);
   }

   virtual idVec3 Evaluate(float u, float v) = 0;

   virtual void Evaluate(std::vector<idVec3> &points) 
   {
      assert(points.size()>=4);
      assert(m_dimensiony==m_dimensionx);

      int size= (int)sqrt((float)points.size());

      float step= 1.0f/(float)size;
      float u=0.0f;

      for (int i=0; i<size; ++i, u=min(u+step,1.0f))
      {
         float v=0.0f;
         for (int j=0; j<size; ++j, v=min(v+step,1.0f))
         {
            points[i*size+j]= Evaluate(u,v);
         }
      }
   }

   idVec3 m_arr[128][128];
   int m_dimensionx, m_dimensiony;
   float m_weights[128][128];
};

class CPolynomSurface: public CBaseSurface
{
public:
   idVec3 Evaluate(float u, float v)
   {
      idVec3 arr[128], res;
      for (int i=0;i<m_dimensiony; ++i)
      {
         res= m_arr[i][m_dimensionx-1];
         for (int j=m_dimensionx-2;j>=0; --j)
         {
            res= res*v+m_arr[i][j];
         }

         arr[i]= res;
      }

      res= arr[m_dimensiony-1];
      for (int i=m_dimensiony-2;i>=0; --i) 
      {
         res= res*u+arr[i];
      }

      return res;
   }
};

class CBezierSurface: public CBaseSurface
{
public:
   idVec3 Evaluate(float u, float v)
   {
      idVec3 points[128];

      for (int i=0; i<m_dimensiony; ++i)
      {
         points[i]= EvaluateCasteljau1D(m_arr[i],v);
      }

      return EvaluateCasteljau1D(points, u);
   }

   idVec3 EvaluateCasteljau1D(idVec3 *arr, float t)
   {
      idVec3 points[128];

      assert(m_dimensiony==m_dimensionx);

      memcpy(&points[0], arr, 128*sizeof(idVec3));

      for (int k=1;k<m_dimensionx;++k)
      {
         for (int i=0; i<m_dimensionx-k; ++i)
         {
            points[i]= (1-t)*points[i] + t*points[i+1];
         }
      }
      return points[0];
   }

private:
   float m_bm[10][10];
};

class CBSplineSurface: public CBaseSurface
{
public:
   CBSplineSurface()
   {
      const int nk[2]= {2,2}; //{3,3};
      float u[2][128]= {{0.35f,0.75f},{0.35f,0.75f}}; //{{0.25f,0.5f,0.75f},{0.25f,0.5f,0.75f}};
      const int pp[2]= {2,2}; //{3,3};

      memset((void*)m_spans, 0, sizeof(m_spans));

      for (int ind=0; ind<2; ++ind)
      {

         m_p[ind]= pp[ind];
         m_m[ind]= 2*(m_p[ind]+1)+nk[ind];
         m_n[ind]= m_m[ind]-(m_p[ind]+1);

         float *ptr= m_spans[ind];
         for (int j=0; j<m_p[ind]+1; ++j)
         {
            *ptr++= 0.0f;
         }

         for (int j= m_p[ind]+1; j<m_m[ind]-m_p[ind]-1; ++j)
         {
            *ptr++= u[ind][j-m_p[ind]-1];
         }

         for (int j=0; j<m_p[ind]+1; ++j)
         {
            *ptr++= 1.0f;
         }
      }

      m_dimensionx= m_n[0];
      m_dimensiony= m_n[1];

      m_arr[0][0]= idVec3(0.8f,0.2f,0.1f);
      m_arr[1][0]= idVec3(0.8f,0.2f,0.3f);
      m_arr[2][0]= idVec3(0.8f,0.0f,0.5f);
      m_arr[3][0]= idVec3(0.8f,0.0f,0.7f);
      m_arr[4][0]= idVec3(0.8f,0.0f,0.9f);

      m_arr[0][1]= idVec3(0.5f,0.2f,0.1f);
      m_arr[1][1]= idVec3(0.5f,0.2f,0.3f);
      m_arr[2][1]= idVec3(0.5f,0.0f,0.5f);
      m_arr[3][1]= idVec3(0.5f,0.0f,0.7f);
      m_arr[4][1]= idVec3(0.5f,0.0f,0.9f);

      m_arr[0][2]= idVec3(0.35f,0.8f,0.1f);
      m_arr[1][2]= idVec3(0.35f,0.8f,0.3f);
      m_arr[2][2]= idVec3(0.35f,0.2f,0.5f);
      m_arr[3][2]= idVec3(0.35f,0.2f,0.7f);
      m_arr[4][2]= idVec3(0.35f,0.2f,0.9f);

      m_arr[0][3]= idVec3(0.1f,0.8f,0.1f);
      m_arr[1][3]= idVec3(0.1f,0.8f,0.3f);
      m_arr[2][3]= idVec3(0.1f,0.2f,0.5f);
      m_arr[3][3]= idVec3(0.1f,0.2f,0.7f);
      m_arr[4][3]= idVec3(0.1f,0.2f,0.9f);

      m_arr[0][4]= idVec3(0.0f,0.8f,0.1f);
      m_arr[1][4]= idVec3(0.0f,0.8f,0.3f);
      m_arr[2][4]= idVec3(0.0f,0.2f,0.5f);
      m_arr[3][4]= idVec3(0.0f,0.2f,0.7f);
      m_arr[4][4]= idVec3(0.0f,0.2f,0.9f);
   }

   idVec3 Evaluate(float u, float v) 
   {
      idVec3 res(0.0f,0.0f,0.0f);

      int uspan= FindSpan(0, u);
      int vspan= FindSpan(1, v);

      std::vector<float> bval[2];

      BasisFuns(0, uspan, u, bval[0]);
      BasisFuns(1, vspan, v, bval[1]);

      int uind= uspan-m_p[0];

      for (int l=0; l<=m_p[1]; ++l)
      {
         idVec3 temp(0.0f,0.0f,0.0f);

         int vind= vspan-m_p[1]+l;

         for (int k=0; k<=m_p[0]; ++k)
         {
            temp+= bval[0][k]*m_arr[uind+k][vind];
         }

         res+= bval[1][l]*temp;
      }

      return res;
   }

   int FindSpan(int ind, float u)
   {
      int low, high, mid;

      if (u>=m_spans[ind][m_n[ind]-1])
         return m_n[ind]-1;

      low= 0; high= m_n[ind];
      mid= (low+high)>>1;
      while (u<m_spans[ind][mid]||u>=m_spans[ind][mid+1])
      {
         if (u<m_spans[ind][mid]) high= mid;
         else                    low= mid;

         mid= (low+high)>>1;
      }
      return mid;
   }

   void BasisFuns(int ind, int i, float t, std::vector<float> &bval)
   {
      float left[128]={0};
      float right[128]={0};
      bval.resize(m_p[ind]+1,0.0f);

      bval[0]=  1.0f;
      for (int j=1;j<=m_p[ind];++j)
      {
         left[j]= t-m_spans[ind][i+1-j];
         right[j]= m_spans[ind][i+j]-t;

         float saved= 0.0f;
         for (int r=0;r<j;++r)
         {
            float temp= bval[r]/(right[r+1]+left[j-r]);
            bval[r]= saved+right[r+1]*temp;
            saved= left[j-r]*temp;
         }
         bval[j]= saved;
      }
   }

private:
   int m_n[2], m_p[2], m_m[2];
   float m_spans[2][128];
};

struct CKdAccelNode {
   // KdAccelNode Methods
   CKdAccelNode() { }
   void initLeaf(int *primNums, int np);
   void initInterior(int axis, int ac, float s) {
      split = s;
      flags = axis;
      aboveChild |= (ac << 2);
   }
   float SplitPos() const { return split; }
   int nPrimitives() const { return nPrims >> 2; }
   int SplitAxis() const { return flags & 3; }
   bool IsLeaf() const { return (flags & 3) == 3; }
   int AboveChild() const { return aboveChild >> 2; }
   union {
      float split;            // Interior
      int onePrimitive;  // Leaf
      int *primitives;   // Leaf
   };

private:
   union {
      int flags;         // Both
      int nPrims;        // Leaf
      int aboveChild;    // Interior
   };
};


struct CBoundEdge {
   // BoundEdge Public Methods
   CBoundEdge() { }
   CBoundEdge(float tt, int pn, bool starting) {
      t = tt;
      primNum = pn;
      type = starting ? START : END;
   }
   bool operator<(const CBoundEdge &e) const {
      if (t == e.t)
         return (int)type < (int)e.type;
      else return t < e.t;
   }
   float t;
   int primNum;
   enum { START, END } type;
};

class CKdTree
{
   friend class CMeshNode;

public:
   CKdTree() { m_mesh= NULL; }

   void Build(const CMeshNode &mesh);
   void BuildInternal(const idBounds &nodeBounds,
      const std::vector<idBounds> &allPrimBounds, int *primNums,
      int nPrimitives, int depth, CBoundEdge *edges[3],
      int *prims0, int *prims1, int badRefines);


   bool Intersect(const CRay &ray);

private:
   std::vector<CKdAccelNode> m_nodes;
   idBounds m_bounds;
   const CMeshNode *m_mesh;
   int m_maxPrims;
   int m_maxDepth;
};


class CObb
{
public:
   CObb()
   {}

   void Build(const std::vector<idVec3> &points)
   {
      //BuildAsAABB(points);
      BuildAsOBB(points);
   }

   void BuildAsAABB(const std::vector<idVec3> &points)
   {
      /*idBounds bounds;

      bounds.Clear();

      for (int i=0; i<points.size(); ++i)
      {
         bounds.AddPoint(points[i]);
      }

      m_center= bounds.GetCenter();*/
      m_axis= mat3_identity;

      idVec3 minE, maxE;

      minE.Set(1e9,1e9,1e9);
      maxE.Set(-1e9,-1e9,-1e9);

      m_axis[0].Normalize();
      m_axis[1].Normalize();
      m_axis[2].Normalize();

      for (int i=0; i<points.size(); ++i)
      {
         idVec3 proj;

         proj.x= m_axis[0]*points[i];
         proj.y= m_axis[1]*points[i];
         proj.z= m_axis[2]*points[i];

         minE.x= min(minE.x,proj.x);
         minE.y= min(minE.y,proj.y);
         minE.z= min(minE.z,proj.z);

         maxE.x= max(maxE.x,proj.x);
         maxE.y= max(maxE.y,proj.y);
         maxE.z= max(maxE.z,proj.z);
      }


      //idVec3 v= bounds.GetMax()-m_center;
      //idVec3 v= (maxE+minE)*0.5f;
      //m_extent= idVec3(fabs(v.x),fabs(v.y),fabs(v.z));
      m_center= (maxE+minE)*0.5f;;
      m_extent= (maxE-m_center);
   }

   void GetVertices(std::vector<CVertex> &vertices, std::vector<int> *edges) const
   {
      for (int i=0; i<8; ++i)
      {
         idVec3 p= m_center;

         for (int j=0; j<3; ++j)
         {
            float sign= i&(1<<(2-j))?1.0f:-1.0f;

            p+= sign*m_extent[j]*m_axis[j];
         }

         CVertex v;
         v.SetVertices(p.x,p.y,p.z);

         vertices.push_back(v);
      }

      if (edges)
      {
         int edg[12][2]= {{0,4},{2,6},{1,5},{3,7},
            {0,2},{1,3},{5,7},{4,6},{0,1},{4,5},{2,3},{6,7}};

         for (int i=0;i<12;++i)
         {
            edges->push_back(edg[i][0]);
            edges->push_back(edg[i][1]);
         }
      }
   }

private:

   void BuildAsOBB(const std::vector<idVec3> &points)
   {
      if (points.empty())
      {
         return;
      }

      static idVec3 avg;
      static idMat3 covMat;

      avg.Zero();
      for (int i=0; i<points.size(); ++i)
      {
         avg+= points[i];
      }

      avg/= points.size();


      covMat.Zero();
      for (int i=0; i<points.size(); ++i)
      {
         covMat[0][0]+= (points[i].x-avg.x)*(points[i].x-avg.x);
         covMat[0][1]+= (points[i].x-avg.x)*(points[i].y-avg.y);
         covMat[0][2]+= (points[i].x-avg.x)*(points[i].z-avg.z);

         covMat[1][1]+= (points[i].y-avg.y)*(points[i].y-avg.y);
         covMat[1][2]+= (points[i].y-avg.y)*(points[i].z-avg.z);

         covMat[2][3]+= (points[i].z-avg.z)*(points[i].z-avg.z);
      }

      float invN= 1.0f/points.size();
      covMat[0][0]*= invN;
      covMat[0][1]*= invN; covMat[1][0]= covMat[0][1];
      covMat[1][1]*= invN;
      covMat[1][2]*= invN; covMat[2][1]= covMat[1][2];
      covMat[0][2]*= invN; covMat[2][0]= covMat[0][2];
      covMat[2][2]*= invN;

      CalculateEigensystem3x3(covMat, m_axis);

      idVec3 minE, maxE;

      minE.Set(1e9,1e9,1e9);
      maxE.Set(-1e9,-1e9,-1e9);

      m_axis[0].Normalize();
      m_axis[1].Normalize();
      m_axis[2].Normalize();

      for (int i=0; i<points.size(); ++i)
      {
         idVec3 proj;

         proj.x= m_axis[0]*points[i];
         proj.y= m_axis[1]*points[i];
         proj.z= m_axis[2]*points[i];

         minE.x= min(minE.x,proj.x);
         minE.y= min(minE.y,proj.y);
         minE.z= min(minE.z,proj.z);

         maxE.x= max(maxE.x,proj.x);
         maxE.y= max(maxE.y,proj.y);
         maxE.z= max(maxE.z,proj.z);
      }

      m_center= (minE+maxE)/2.0f;
      m_extent= maxE-m_center;
   }

   void CalculateEigensystem3x3(const idMat3 &m, idMat3 &res)
   {
      float m11= m[0][0];
      float m12= m[0][1];
      float m13= m[0][2];
      float m22= m[1][1];
      float m23= m[1][2];
      float m33= m[2][2];

      res= mat3_identity;

      const float k_eps= 1e-6;
      const int k_maxSweeps= 4096;
      for (int a=0; a<k_maxSweeps; ++a)
      {
         if (fabs(m12)<k_eps && fabs(m13)<k_eps && fabs(m23)<k_eps)
         {
            break;
         }

         if (m12!=0.0f)
         {
            float u= (m22-m11)*0.5f/m12;
            float u2= u*u;
            float u2p1= u2+1.0f;
            float t=(u2p1!=u2)?
               ((u<0.0f)?-1.0f:1.0f)*(sqrt(u2p1)-fabs(u)) : 0.5f/u;
            float c= 1.0f/sqrt(t*t+1.0f);
            float s= c*t;

            m11-= t*m12;
            m22+= t*m12;
            m12= 0.0f;

            float temp= c*m13-s*m23;
            m23= s*m13 + c*m23;
            m13= temp;

            for (int i=0; i<3; ++i)
            {
               float temp= c*res[i][0] - s*res[i][1];
               res[i][1]= s*res[i][0] + c*res[i][1];
               res[i][0]= temp;
            }
         }

         if (m13!=0.0f)
         {
            float u= (m33-m11)*0.5f/m13;
            float u2= u*u;
            float u2p1= u2+1.0f;
            float t=(u2p1!=u2)?
               ((u<0.0f)?-1.0f:1.0f)*(sqrt(u2p1)-fabs(u)) : 0.5f/u;
            float c= 1.0f/sqrt(t*t+1.0f);
            float s= c*t;

            m11-= t*m13;
            m33+= t*m13;
            m13= 0.0f;

            float temp= c*m12-s*m23;
            m23= s*m12 + c*m23;
            m13= temp;

            for (int i=0; i<3; ++i)
            {
               float temp= c*res[i][0] - s*res[i][2];
               res[i][2]= s*res[i][0] + c*res[i][2];
               res[i][0]= temp;
            }
         }

         if (m23!=0.0f)
         {
            float u= (m33-m22)*0.5f/m23;
            float u2= u*u;
            float u2p1= u2+1.0f;
            float t=(u2p1!=u2)?
               ((u<0.0f)?-1.0f:1.0f)*(sqrt(u2p1)-fabs(u)) : 0.5f/u;
            float c= 1.0f/sqrt(t*t+1.0f);
            float s= c*t;

            m22-= t*m23;
            m33+= t*m23;
            m23= 0.0f;

            float temp= c*m12-s*m13;
            m23= s*m12 + c*m13;
            m13= temp;

            for (int i=0; i<3; ++i)
            {
               float temp= c*res[i][1] - s*res[i][2];
               res[i][2]= s*res[i][1] + c*res[i][2];
               res[i][1]= temp;
            }
         }
      }

      res.TransposeSelf();
   }

   idVec3 m_center;
   idMat3 m_axis;
   idVec3 m_extent;
};

class CMeshNode
{
   friend class CTetGenMeshNodeSerializer;
   friend class CTetGenEleDeserializer;

public:

   CMeshNode() : m_staticSystemMatrixK(10000,10000)
   {
      m_vertexBuffer= NULL;
      m_edgesIndexBuffer= NULL;
      m_faceIndexBuffer= NULL;
      m_meshAuxData= NULL;
      m_selectedFaceIndex= -1;
      m_modelDiffuseRV= NULL;
      m_subdivision_count= 0;
      m_curve= NULL;
      m_surface= NULL;
      m_renderTriangles= true;
      m_renderEdges= false;
   }

   ~CMeshNode()
   {
      Destroy();
      if (m_curve)
         delete m_curve;
      if (m_surface)
         delete m_surface;
   }

   void Destroy()
   {
      SAFE_RELEASE(m_vertexBuffer);
      SAFE_RELEASE(m_edgesIndexBuffer);
      SAFE_RELEASE(m_faceIndexBuffer);
      SAFE_RELEASE(m_modelDiffuseRV);
   }

   void ZeroForces()
   {
      std::fill(m_forces.begin(), m_forces.end(), idVec3(0.0f, 0.0f, 0.0f));
   }

   int GetSelectedFaceIndex()
   {
      return m_selectedFaceIndex;
   }

   void MoveFaceAlongNormal(int face_index, float how_far)
   {
      D3DXVECTOR3 *v[4];
      D3DXVECTOR3 e1, e2, normal;

      v[0]= (D3DXVECTOR3*)m_vertices[m_faces[face_index*4+0]].v;
      v[1]= (D3DXVECTOR3*)m_vertices[m_faces[face_index*4+1]].v;
      v[2]= (D3DXVECTOR3*)m_vertices[m_faces[face_index*4+2]].v;
      v[3]= (D3DXVECTOR3*)m_vertices[m_faces[face_index*4+3]].v;

      e1= *v[3]-*v[0];
      e2= *v[1]-*v[0];

      D3DXVec3Cross(&normal, &e1, &e2);

      D3DXVec3Normalize(&normal, &normal);

      normal*= how_far;

      for (int i=0;i<4;++i)
      {
         D3DXVec3Add(v[i], v[i], &normal);
      }
   }

   bool SerializeToTetgen(const char *out_path)
   {
      CTetGenMeshNodeSerializer serializer;
      return serializer.Serialize(*this, out_path);
   }

   bool SetupFromTetGen(ID3D11Device* pd3dDevice, const D3DMATRIX &transform, const std::string &path)
   {
      CTetGenEleDeserializer eleDeserializer;
      CTetGenNodeDeserializer nodeDeserializer;

      m_transform= transform;

      //m_edges.resize(1,0);

      if (nodeDeserializer.Deserialize(*this, (path+".node").c_str())
         && eleDeserializer.Deserialize(*this, (path+".ele").c_str(), (path+".neigh").c_str()))
      {
         int vsz= m_vertices.size()*3;
         m_staticSystemMatrixK.SetDimensions(vsz,vsz);
         BuildK(m_staticSystemMatrixK);
         SetupRenderData(pd3dDevice);
         return true;
      }

      return false;
   }
   

   // polyIndex - polyhedron Index
   void BuildPe(idMat4 &pe, int tetIndex)
   {
      STetrahedron &tetr= m_tetrahedrons[tetIndex];

      CVertex &v1(m_vertices[tetr.index[0]]),
              &v2(m_vertices[tetr.index[1]]),
              &v3(m_vertices[tetr.index[2]]),
              &v4(m_vertices[tetr.index[3]]);

      pe.Zero();
      
      pe[0][0]= 1.0f; pe[0][1]= 1.0f; pe[0][2]= 1.0f; pe[0][3]= 1.0f;
      pe[1][0]= v1.v[0]; pe[1][1]= v2.v[0]; pe[1][2]= v3.v[0]; pe[1][3]= v4.v[0];
      pe[2][0]= v1.v[1]; pe[2][1]= v2.v[1]; pe[2][2]= v3.v[1]; pe[2][3]= v4.v[1];
      pe[3][0]= v1.v[2]; pe[3][1]= v2.v[2]; pe[3][2]= v3.v[2]; pe[3][3]= v4.v[2];
   }

   float BuildBe(idMatX &be, int tetIndex)
   {
      idMat4 pe;

      BuildPe(pe, tetIndex);

      float determinant= pe.Determinant();

      assert(pe.InverseSelf());

      be= idMatX(6, 12);
      be.Zero();

      be[0][0]= pe[0][1]; be[0][3]= pe[1][1]; be[0][6]= pe[2][1]; be[0][9]= pe[3][1];
      be[1][1]= pe[0][2]; be[1][4]= pe[1][2]; be[1][7]= pe[2][2]; be[1][10]= pe[3][2];
      be[2][2]= pe[0][3]; be[2][5]= pe[1][3]; be[2][8]= pe[2][3]; be[2][11]= pe[3][3];
      be[3][0]= pe[0][2]/2.0f; be[3][1]= pe[0][1]/2.0f; be[3][3]= pe[1][2]/2.0f; be[3][4]= pe[1][1]/2.0f; be[3][6]= pe[2][2]/2.0f; be[3][7]= pe[2][1]/2.0f; be[3][9]= pe[3][2]/2.0f; be[3][10]= pe[3][1]/2.0f;
      be[4][1]= pe[0][3]/2.0f; be[4][2]= pe[0][2]/2.0f; be[4][4]= pe[1][3]/2.0f; be[4][5]= pe[1][2]/2.0f; be[4][7]= pe[2][3]/2.0f; be[4][8]= pe[2][2]/2.0f; be[4][10]= pe[3][3]/2.0f; be[4][11]= pe[3][2]/2.0f;
      be[5][0]= pe[0][3]/2.0f; be[5][2]= pe[0][1]/2.0f; be[5][3]= pe[1][3]/2.0f; be[5][5]= pe[1][1]/2.0f; be[5][6]= pe[2][3]/2.0f; be[5][8]= pe[2][1]/2.0f; be[5][9]= pe[3][3]/2.0f; be[5][11]= pe[3][1]/2.0f;

      return determinant;
   }

   void BuildKe(idMatX &ke, int tetIndex)
   {
      idMatX be;
      float determinant= BuildBe(be, tetIndex);
      float volume= 1/(6*determinant);

      const float k_poisson_ratio= 0.35f;
      const float k_young_coeff= 0.3f;

      float lambda= k_poisson_ratio*k_young_coeff/((1+k_poisson_ratio)*(1-2*k_poisson_ratio));
      float mu= k_young_coeff/(2*(1+k_poisson_ratio));

      idMatX el(6,6);
      el.Zero();

      el[0][0]= lambda+2*mu; el[0][1]= lambda; el[0][2]= lambda;
      el[1][0]= lambda; el[1][1]= lambda+2*mu; el[1][2]= lambda;
      el[2][0]= lambda; el[2][1]= lambda; el[2][2]= lambda+2*mu;
      el[3][3]= mu;
      el[4][4]= mu;
      el[5][5]= mu;
         
      ke= volume*be.Transpose()*el*be;
   }

   void BuildK(idSparseMatX &kmat)
   {
      int vsz= m_vertices.size();

      assert(vsz>0);

      for (int tetr_index=0; tetr_index<m_tetrahedrons.size(); ++tetr_index)
      {
         STetrahedron &tetr= m_tetrahedrons[tetr_index];

         idMatX ke;
         BuildKe(ke, tetr_index);

         for (int i=0;i<4;++i) 
            for (int j=0;j<4;++j)
         {
            int dst_start_i= tetr.index[i]*3;
            int dst_start_j= tetr.index[j]*3;

            for (int ki=0;ki<3;++ki)
               for (int kj=0; kj<3;++kj)
            {
               kmat.InsertGet(dst_start_i+ki, dst_start_j+kj)+=ke[i*3+ki][j*3+kj];
            }
         }
      }
   }

   void SolveStaticSystem()
   {
      int vsz= m_vertices.size();

      // random lb
      assert(vsz>8);

      // freeze some last nodes
      const int k_freeze_vcnt=4;
      int new_vsz= (vsz-k_freeze_vcnt)*3;
      m_staticSystemMatrixK.SetDimensions(new_vsz, new_vsz);
      if (m_staticSystemMatrixK.GetDimension()!=new_vsz)
      {
         m_staticSystemMatrixK.OptimizeRepr();
      }

      //idSparseMatX sqMat(new_vsz,new_vsz);
      //sqMat.SquareSubMatrix(kmat, new_vsz);

      idVecX force(new_vsz);
      force.Zero();

      for(int i=0;i<force.GetDimension();i+=3)
      {
         force[i+0]= m_forces[i/3].x;
         force[i+1]= m_forces[i/3].y;
         force[i+2]= m_forces[i/3].z;
      }

      idVecX displacements(new_vsz);
      displacements.Zero();

      idSparseMatX::ConjugateGradient(m_staticSystemMatrixK, force, displacements, 20, 1e-6);

      std::fill(m_vertices_displacements.begin(), m_vertices_displacements.end(), 0.0f);
      for (int i=0;i<displacements.GetDimension();i+=3)
      {
         m_vertices_displacements[i+0]= displacements[i+0];
         m_vertices_displacements[i+1]= displacements[i+1];
         m_vertices_displacements[i+2]= displacements[i+2];
      }

      //memcpy(m_vertices_displacements.data(), displacements.ToFloatPtr(), displacements.GetDimension()*sizeof(float));

      //m_vertices.swap(new_vertices);
   }


   //typedef struct {
   //   int						vertexNum[3];
   //   int						tVertexNum[3];
   //   idVec3					faceNormal;
   //   idVec3					vertexNormals[3];
   //   byte					vertexColors[3][4];
   //} aseFace_t;

   //typedef struct {
   //   int						timeValue;

   //   int						numVertexes;
   //   int						numTVertexes;
   //   int						numCVertexes;
   //   int						numFaces;
   //   int						numTVFaces;
   //   int						numCVFaces;

   //   idVec3					transform[4];			// applied to normals

   //   bool					colorsParsed;
   //   bool					normalsParsed;
   //   idVec3 *				vertexes;
   //   idVec2 *				tvertexes;
   //   idVec3 *				cvertexes;
   //   aseFace_t *				faces;
   //} aseMesh_t;

   //typedef struct {
   //   char					name[128];
   //   float					uOffset, vOffset;		// max lets you offset by material without changing texCoords
   //   float					uTiling, vTiling;		// multiply tex coords by this
   //   float					angle;					// in clockwise radians
   //} aseMaterial_t;

   //typedef struct {
   //   char					name[128];
   //   int						materialRef;

   //   aseMesh_t				mesh;

   //   // frames are only present with animations
   //   std::vector<aseMesh_t*>		frames;			// aseMesh_t
   //} aseObject_t;

   //typedef struct aseModel_s {
   //   int					timeStamp;
   //   std::vector<aseMaterial_t *>	materials;
   //   std::vector<aseObject_t *>	objects;
   //} aseModel_t;



   void SetupFromAse(const char *file_name, ID3D11Device *pd3dDevice, const D3DMATRIX &transform)
   {
      m_transform= transform;

      aseModel_t *model= ASE_Load(file_name);
      assert(model);

      for (int i=0; i<model->objects.Num(); ++i)
      {
         aseObject_t *obj= model->objects[i];

         if (!obj)
            continue;

         aseMesh_t *mesh= &obj->mesh;

         assert(mesh->normalsParsed);

         m_vertices.resize(m_vertices.size()+mesh->numVertexes);
         for (int i=0; i<mesh->numVertexes; ++i)
         {
            m_vertices[i].SetVertices(mesh->vertexes[i].x, mesh->vertexes[i].y, mesh->vertexes[i].z);
            m_vertices[i].SetTexCoord(mesh->tvertexes[i].x, mesh->tvertexes[i].y);
         }

         m_triangles.resize(m_triangles.size()+3*mesh->numFaces);
         for (int i=0; i<mesh->numFaces; ++i)
         {
            aseFace_t *face= &mesh->faces[i];

            m_triangles[i*3+0]= face->vertexNum[0];
            m_triangles[i*3+1]= face->vertexNum[1];
            m_triangles[i*3+2]= face->vertexNum[2];

            m_vertices[face->vertexNum[0]].SetNormal(face->vertexNormals[0].x,face->vertexNormals[0].y,face->vertexNormals[0].z);
            m_vertices[face->vertexNum[1]].SetNormal(face->vertexNormals[1].x,face->vertexNormals[1].y,face->vertexNormals[1].z);
            m_vertices[face->vertexNum[2]].SetNormal(face->vertexNormals[2].x,face->vertexNormals[2].y,face->vertexNormals[2].z);
         }

         aseMaterial_t *material= model->materials[obj->materialRef];
         m_matname_diffuseRV= material->name;

         //break;
      }

      // fake
      m_edges.resize(1, 0);

      


      SetupRenderData(pd3dDevice);

      ASE_Free(model);
   }

   void SetupFromObb(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, const CObb &obb)
   {
      m_transform= transform;

      m_vertices.clear();
      m_edges.clear();

      obb.GetVertices(m_vertices, &m_edges);

      m_triangles.resize(3,0);
      m_faces.resize(1,0);

      m_triangles[0]= 0;
      m_triangles[1]= 3;
      m_triangles[2]= 1;

      SetupRenderData(pd3dDevice);

      m_renderEdges= true;
      m_renderTriangles= false;
   }

   void SetupQuad(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, float w= 20.0f)
   {
      m_transform= transform;

      m_vertices.resize(4);
      m_vertices[0].SetVertices(-w,  w, 0);
      m_vertices[1].SetVertices( w,  w, 0);
      m_vertices[2].SetVertices( w,  -w, 0);
      m_vertices[3].SetVertices(-w, -w, 0);

      m_vertices[0].SetTexCoord(0,0);
      m_vertices[1].SetTexCoord(1,0);
      m_vertices[2].SetTexCoord(1,1);
      m_vertices[3].SetTexCoord(0,1);

      float halfHeight= g_zFar*tanf(0.5f*g_field_of_view);
      float halfWidth= g_fAspectRatio*halfHeight;

      m_vertices[0].SetNormal(-halfWidth,halfHeight,g_zFar);
      m_vertices[1].SetNormal(halfWidth,halfHeight,g_zFar);
      m_vertices[2].SetNormal(halfWidth,-halfHeight,g_zFar);
      m_vertices[3].SetNormal(-halfWidth,-halfHeight,g_zFar);

      m_edges.resize(4*2);
      m_edges[0]= 0; m_edges[1]= 3;
      m_edges[2]= 3; m_edges[3]= 2;
      m_edges[4]= 2; m_edges[5]= 1;
      m_edges[6]= 1; m_edges[7]= 0;

      m_faces.resize(4*1);
      m_faces[0]= 0;
      m_faces[1]= 3;
      m_faces[2]= 2;
      m_faces[3]= 1;

      m_triangles.resize(2*3);
      m_triangles[0]= 0;
      m_triangles[1]= 2;
      m_triangles[2]= 3;
      m_triangles[3]= 0;
      m_triangles[4]= 1;
      m_triangles[5]= 2;

      SetupRenderData(pd3dDevice);
   }

   void GetQuadHelperVertices(std::vector<CVertex> & vertices, float ratio, float texoffset, int tex, float hoffset, float voffset)
   {
      vertices.clear();

      float fw = ratio * m_width;
      float fh = m_height;

      vertices.resize(4);      
      vertices[0].SetVertices(-fw + hoffset,  voffset, 0);
      vertices[1].SetVertices(  0 + hoffset,  voffset, 0);
      vertices[2].SetVertices(  0 + hoffset,  voffset-fh, 0);
      vertices[3].SetVertices(-fw + hoffset,  voffset-fh, 0);

#if 1
      if (texoffset > 0.0f) {
         assert(texoffset <= 1.0f);
         vertices[0].SetTexCoord(0,0);
         vertices[1].SetTexCoord(texoffset,0);
         vertices[2].SetTexCoord(texoffset,1);
         vertices[3].SetTexCoord(0,1);
      } else {
         texoffset = fabs(texoffset);
         vertices[0].SetTexCoord(texoffset,0);
         vertices[1].SetTexCoord(1,0);
         vertices[2].SetTexCoord(1,1);
         vertices[3].SetTexCoord(texoffset,1);         
      }
#endif

      float ftex = (float)tex;
      vertices[0].SetColor(ftex, ftex, ftex, ftex);
      vertices[1].SetColor(ftex, ftex, ftex, ftex);
      vertices[2].SetColor(ftex, ftex, ftex, ftex);
      vertices[3].SetColor(ftex, ftex, ftex, ftex);
   }

   void SetupBookShelf(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, int num_pages, float w= 20.0f)
   {
      m_transform= transform;
      m_width = w;
      m_height = (((float)TEXTURE_BOOK_HEIGHT / TEXTURE_BOOK_WIDTH) * m_width);

      float offset = 0.0f;

      m_vertices.clear();

      for (int i = 0; i < num_pages; ++i) {
         int verticesOffset = m_vertices.size();
         int edgesOffset = m_edges.size();
         int facesOffset = m_faces.size();
         int trianglesOffset = m_triangles.size();

         std::vector<CVertex> v1, v2;
         GetQuadHelperVertices(v1, 0.5f, 0.5f, 0, offset, 0.0f);
         GetQuadHelperVertices(v2, 0.5f, 0.5f, 0, offset - 0.5f * w, 0.0f);

         std::copy(v1.begin(), v1.end(), std::back_inserter(m_vertices));
         std::copy(v2.begin(), v2.end(), std::back_inserter(m_vertices));

         m_edges.resize(edgesOffset + 2*4*2);
         m_edges[edgesOffset+0]= verticesOffset+0; m_edges[edgesOffset+1]= verticesOffset+3;
         m_edges[edgesOffset+2]= verticesOffset+3; m_edges[edgesOffset+3]= verticesOffset+2;
         m_edges[edgesOffset+4]= verticesOffset+2; m_edges[edgesOffset+5]= verticesOffset+1;
         m_edges[edgesOffset+6]= verticesOffset+1; m_edges[edgesOffset+7]= verticesOffset+0;
         m_edges[edgesOffset+8+0]= verticesOffset+4+0; m_edges[edgesOffset+8+1]= verticesOffset+4+3;
         m_edges[edgesOffset+8+2]= verticesOffset+4+3; m_edges[edgesOffset+8+3]= verticesOffset+4+2;
         m_edges[edgesOffset+8+4]= verticesOffset+4+2; m_edges[edgesOffset+8+5]= verticesOffset+4+1;
         m_edges[edgesOffset+8+6]= verticesOffset+4+1; m_edges[edgesOffset+8+7]= verticesOffset+4+0;

         m_faces.resize(facesOffset+2*4*1);
         m_faces[facesOffset+0]= verticesOffset+0;
         m_faces[facesOffset+1]= verticesOffset+3;
         m_faces[facesOffset+2]= verticesOffset+2;
         m_faces[facesOffset+3]= verticesOffset+1;
         m_faces[facesOffset+4+0]= verticesOffset+4+0;
         m_faces[facesOffset+4+1]= verticesOffset+4+3;
         m_faces[facesOffset+4+2]= verticesOffset+4+2;
         m_faces[facesOffset+4+3]= verticesOffset+4+1;

         m_triangles.resize(trianglesOffset+2*2*3);
         m_triangles[trianglesOffset+0]= verticesOffset+0;
         m_triangles[trianglesOffset+1]= verticesOffset+2;
         m_triangles[trianglesOffset+2]= verticesOffset+3;
         m_triangles[trianglesOffset+3]= verticesOffset+0;
         m_triangles[trianglesOffset+4]= verticesOffset+1;
         m_triangles[trianglesOffset+5]= verticesOffset+2;
         m_triangles[trianglesOffset+6+0]= verticesOffset+4+0;
         m_triangles[trianglesOffset+6+1]= verticesOffset+4+2;
         m_triangles[trianglesOffset+6+2]= verticesOffset+4+3;
         m_triangles[trianglesOffset+6+3]= verticesOffset+4+0;
         m_triangles[trianglesOffset+6+4]= verticesOffset+4+1;
         m_triangles[trianglesOffset+6+5]= verticesOffset+4+2;

         SetupRenderData(pd3dDevice);
      }
   }

   void SetupCube(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, float w= 20.0f)
   {
      m_transform= transform;

      m_vertices.resize(4*6);
      m_vertices[0*4+0].SetVertices(-w,  w, -w);
      m_vertices[0*4+1].SetVertices( w,  w, -w);
      m_vertices[0*4+2].SetVertices( w,  -w, -w);
      m_vertices[0*4+3].SetVertices(-w, -w, -w);

      m_vertices[1*4+0].SetVertices(w,  w, -w);
      m_vertices[1*4+1].SetVertices(w,  w, w);
      m_vertices[1*4+2].SetVertices(w, -w, w);
      m_vertices[1*4+3].SetVertices(w, -w, -w);

      m_vertices[2*4+0].SetVertices(w,  w, w);
      m_vertices[2*4+1].SetVertices(-w,  w, w);
      m_vertices[2*4+2].SetVertices(-w, -w, w);
      m_vertices[2*4+3].SetVertices(w, -w, w);

      m_vertices[3*4+0].SetVertices(-w,  w, w);
      m_vertices[3*4+1].SetVertices(-w,  w, -w);
      m_vertices[3*4+2].SetVertices(-w, -w, -w);
      m_vertices[3*4+3].SetVertices(-w, -w, w);

      m_vertices[4*4+0].SetVertices(-w,  w, w);
      m_vertices[4*4+1].SetVertices(w,  w, w);
      m_vertices[4*4+2].SetVertices(w, w, -w);
      m_vertices[4*4+3].SetVertices(-w, w, -w);

      m_vertices[5*4+0].SetVertices(w,  -w, w);
      m_vertices[5*4+1].SetVertices(-w,  -w, w);
      m_vertices[5*4+2].SetVertices(-w, -w, -w);
      m_vertices[5*4+3].SetVertices(w, -w, -w);

      for (int i=0;i <6;++i)
      {
         m_vertices[4*i+0].SetColor(1.0f,1.0f,1.0f,1.0f);
         m_vertices[4*i+1].SetColor(1.0f,1.0f,1.0f,1.0f);
         m_vertices[4*i+2].SetColor(1.0f,1.0f,1.0f,1.0f);
         m_vertices[4*i+3].SetColor(1.0f,1.0f,1.0f,1.0f);
      }

      m_edges.resize(2*4*6);
      for (int i=0; i<6; ++i)
      {
         m_edges[i*6+0]= i*4+0; m_edges[i*6+1]= i*4+3;
         m_edges[i*6+2]= i*4+3; m_edges[i*6+3]= i*4+2;
         m_edges[i*6+4]= i*4+2; m_edges[i*6+5]= i*4+1;
         m_edges[i*6+6]= i*4+1; m_edges[i*6+7]= i*4+0;
      }

      m_faces.resize(4*6);
      for (int i=0; i<6; ++i)
      {
         m_faces[i*4+0]= i*4+0;
         m_faces[i*4+1]= i*4+3;
         m_faces[i*4+2]= i*4+2;
         m_faces[i*4+3]= i*4+1;
      }

      m_triangles.resize(2*3*6);
      for (int i=0; i<6; ++i)
      {
         m_triangles[6*i+0]= 4*i+0;
         m_triangles[6*i+1]= 4*i+2;
         m_triangles[6*i+2]= 4*i+3;
         m_triangles[6*i+3]= 4*i+0;
         m_triangles[6*i+4]= 4*i+1;
         m_triangles[6*i+5]= 4*i+2;
      }

      SetupRenderData(pd3dDevice);
   }

   CBaseCurve *GetCurve() const
   {
      return m_curve;
   }

   void SetupCurve(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, 
      e_curve_type curve_type,
      int subdivision_count= 128)
   {
      m_transform= transform;
      m_subdivision_count= subdivision_count;

      switch (curve_type)
      {
      case _polynom_curve_type:
         m_curve= new CPolynomCurve();
         break;
      case _bezier_curve_type:
         m_curve= new CBezierCurve();
         break;
      case _bspline_curve_type:
         m_curve= new CBSplineCurve();
         break;
      default:
         assert(0&&"unknown curve type");
         break;
      };

      m_vertices.resize(subdivision_count+1);

      m_edges.resize(2*subdivision_count);
      for (int i=0; i<subdivision_count; ++i)
      {
         m_edges[2*i+0]= i;
         m_edges[2*i+1]= i+1;
      }

      m_faces.resize(1);
      m_triangles.resize(3,0);

      SetupRenderData(pd3dDevice);
   }

   void SetupSurface(ID3D11Device *pd3dDevice, const D3DMATRIX &transform, 
      e_surface_type surface_type,
      int subdivision_count= 128)
   {
      m_transform= transform;
      m_subdivision_count= subdivision_count;

      switch (surface_type)
      {
      case _polynom_surface_type:
         m_surface= new CPolynomSurface();
         break;
      case _bezier_surface_type:
         m_surface= new CBezierSurface();
         break;
      case _bspline_surface_type:
         m_surface= new CBSplineSurface();
         break;
      default:
         assert(0&&"unknown surface type");
         break;
      };

      m_vertices.resize((subdivision_count+1)*(subdivision_count+1));

      m_edges.clear();
      for (int i=0; i<=subdivision_count; ++i)
      {
         for (int j=0; j<subdivision_count; ++j)
         {
            m_edges.push_back(i*(subdivision_count+1)+j);
            m_edges.push_back(i*(subdivision_count+1)+j+1);
         }
      }

      for (int j=0; j<=subdivision_count; ++j)
      {
         for (int i=0; i<subdivision_count; ++i)
         {
            m_edges.push_back(i*(subdivision_count+1)+j);
            m_edges.push_back((i+1)*(subdivision_count+1)+j);
         }
      }

      m_faces.resize(1);
      m_triangles.resize(3,0);

      SetupRenderData(pd3dDevice);
   }

   void SetupKdTree()
   {
      m_kdtree.Build(*this);
   }

   bool TriangleIntersectsMeshSlow(const CRay &ray)
   {
      for (int i=0; i<m_triangles.size(); i+=3)
      {
         D3DXVECTOR4 v[3];

         v[0]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+0]].v, 1.0f);
         v[1]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+1]].v, 1.0f);
         v[2]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+2]].v, 1.0f);

         D3DXVec4Transform(&v[0], &v[0], &m_transform);
         D3DXVec4Transform(&v[1], &v[1], &m_transform);
         D3DXVec4Transform(&v[2], &v[2], &m_transform);

         float rayt;
         if (ray.IntersectsWithTriangle(rayt, v[0], v[1], v[2]))
         {
            return true;
         }
      }

      return false;
   }

   bool TriangleIntersectsMeshFast(const CRay &ray)
   {
      if (m_kdtree.Intersect(ray))
      {
         return true;
      }
      return false;
   }

   inline D3DXVECTOR4 InterpolatePoint(const D3DXVECTOR4 &left, const D3DXVECTOR4 &right, float t) const
   {
      D3DXVECTOR4 out;
      out.x= t*left.x+(1-t)*right.x;
      out.y= t*left.y+(1-t)*right.y;
      out.z= t*left.z+(1-t)*right.z;
      out.w= 1;
      return out;
   }

   inline D3DXVECTOR4 UniformSampleHemisphere(float u1, float u2) const
   {
      float z = u1;
      float r = sqrtf(max(0.f, 1.f - z*z));
      float phi = 2 * idMath::PI * u2;
      float x = r * cosf(phi);
      float y = r * sinf(phi);
      return D3DXVECTOR4(x, y, z, 0);
   }

   void SetupAmbientOcclusionCPU()
   {
      std::vector<float> occlusion(m_vertices.size(), 0.0f);

      SetupKdTree();

      for (int i=0; i<m_triangles.size(); i+=3)
      {
         D3DXVECTOR4 v[3], v01, v02, vc;

         v[0]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+0]].v, 1.0f);
         v[1]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+1]].v, 1.0f);
         v[2]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i+2]].v, 1.0f);

         D3DXVec4Transform(&v[0], &v[0], &m_transform);
         D3DXVec4Transform(&v[1], &v[1], &m_transform);
         D3DXVec4Transform(&v[2], &v[2], &m_transform);



         idVec3 forward, left, up;
         forward= idVec3(v[1].x-v[0].x,v[1].y-v[0].y,v[1].z-v[0].z);
         forward= idVec3(v[2].x-v[0].x,v[2].y-v[0].y,v[2].z-v[0].z);
         forward.Normalize(); left.Normalize();
         up.Cross(forward, left);



         const int nSamples= 8;
         float occlusion= 0.0f;
         for (int j=0; j<nSamples; ++j)
         {
            float t1=(float)rand()/RAND_MAX;
            float t2=(1-t1)*((float)rand()/RAND_MAX);
            float t3=1-t1-t2;

            v01= InterpolatePoint(v[0],v[1],t1);
            v02= InterpolatePoint(v[0],v[2],t2);
            vc= InterpolatePoint(v01,v02,t3);

            idVec3 vcc(vc.x,vc.y,vc.z);
            vcc+=0.001f*up;

            D3DXVECTOR4 localVector= UniformSampleHemisphere((float)rand()/RAND_MAX,(float)rand()/RAND_MAX);
            idVec3 sampleVector= localVector.x*forward + localVector.y*left + localVector.z*up;

            const float kRadius= 128.0f;
            //if (TriangleIntersectsMeshSlow(CRay::FromOriginDirection(vcc,sampleVector*kRadius)))
            if (TriangleIntersectsMeshFast(CRay::FromOriginDirection(vcc,sampleVector*kRadius)))
            {
               occlusion+= 1.0f/(3.0f*nSamples);
            }
         }

         if (occlusion!=0.0f)
         {
            float colors[3][3];
            m_vertices[m_triangles[i+0]].GetColor(colors[0]);
            m_vertices[m_triangles[i+1]].GetColor(colors[1]);
            m_vertices[m_triangles[i+2]].GetColor(colors[2]);

            m_vertices[m_triangles[i+0]].SetColor(max(0,colors[0][0]-occlusion),max(0,colors[0][1]-occlusion),max(0,colors[0][2]-occlusion),1);
            m_vertices[m_triangles[i+1]].SetColor(max(0,colors[1][0]-occlusion),max(0,colors[1][1]-occlusion),max(0,colors[1][2]-occlusion),1);
            m_vertices[m_triangles[i+2]].SetColor(max(0,colors[2][0]-occlusion),max(0,colors[2][1]-occlusion),max(0,colors[2][2]-occlusion),1);
         }
      }
   }
   
   void UpdateBuffers(ID3D11Device *pd3dDevice, ID3D11DeviceContext *pd3dImmediateContext)
   {
      HRESULT hr;
      D3D11_MAPPED_SUBRESOURCE MappedResource;

      /*

      V(pd3dImmediateContext->Map(m_meshAuxData, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
      s_mesh_aux_data *mesh_aux_data= (s_mesh_aux_data*)MappedResource.pData;
      memcpy(mesh_aux_data, m_meshAuxData, sizeof(m_meshAuxData));
      pd3dImmediateContext->Unmap(m_meshAuxData, 0);
      */


      V(pd3dImmediateContext->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
      CVertex *vertex_data= (CVertex*)MappedResource.pData;
      
      /*
      std::vector<CVertex> vertices_to_render(m_vertices);
      for (int i=0; i<vertices_to_render.size(); ++i)
      {
         vertices_to_render[i].v[0]+= m_vertices_displacements[i*3+0];
         vertices_to_render[i].v[1]+= m_vertices_displacements[i*3+1];
         vertices_to_render[i].v[2]+= m_vertices_displacements[i*3+2];
      }
      memcpy(vertex_data, (const void *)vertices_to_render.data(), vertices_to_render.size()*sizeof(CVertex));
      */

      m_vertices.clear();

      int numColumns = 16;

      float hoffset = (m_width * numColumns) / 2.0f;
      float voffset = m_height / 2.0f;
      float margin = 0.0f;
      for (int i = 0; i < g_bookStand.GetPages(); ++i) {
         float splitRatio;
         int texIndex1, texIndex2;
         g_bookStand.GetOffset(i, splitRatio, texIndex1, texIndex2);

         texIndex1 = texIndex1 + 1;
         texIndex2 = texIndex2 + 1;

         voffset = m_height * (i / numColumns) + m_height / 2.0f;

         if (i % numColumns == 0) {
            hoffset = (m_width * numColumns) / 2.0f;
         }

         std::vector<CVertex> v1, v2;
         GetQuadHelperVertices(v1,        splitRatio,  splitRatio, texIndex1, hoffset, voffset);
         GetQuadHelperVertices(v2, 1.0f - splitRatio, -splitRatio, texIndex2, hoffset - splitRatio * m_width, voffset);

         std::copy(v1.begin(), v1.end(), std::back_inserter(m_vertices));
         std::copy(v2.begin(), v2.end(), std::back_inserter(m_vertices));

         hoffset -= m_width + margin;
      }

      //if (m_curve)
      //{
      //   std::vector<idVec3> points(m_subdivision_count+1);

      //   m_curve->Evaluate(points);

      //   for (int i=0; i<m_vertices.size(); ++i)
      //   {
      //      m_vertices[i].SetVertices(points[i].x, points[i].y, points[i].z);
      //   }
      //}

      //if (m_surface)
      //{
      //   std::vector<idVec3> points((m_subdivision_count+1)*(m_subdivision_count+1));

      //   m_surface->Evaluate(points);

      //   for (int i=0; i<=m_subdivision_count; ++i)
      //   {
      //      for (int j=0; j<=m_subdivision_count; ++j)
      //      {
      //         const idVec3 &vec3= points[i*(m_subdivision_count+1)+j];
      //         m_vertices[i*(m_subdivision_count+1)+j].SetVertices(vec3.x,vec3.y,vec3.z);
      //      }
      //   }
      //}
      
      memcpy(vertex_data, (const void *)m_vertices.data(), m_vertices.size()*sizeof(CVertex));

      pd3dImmediateContext->Unmap(m_vertexBuffer, 0);

      g_mWorldView= m_transform * g_mWorld * g_mView;
      g_WorldInvTransposeView = g_mWorldView;
      g_mWorldViewProjection = g_mWorldView * g_mProj;
      g_ViewToTexSpace= g_mProj;

      D3DXMatrixInverse(&g_WorldInvTransposeView,NULL,&g_WorldInvTransposeView);
      D3DXMatrixTranspose(&g_WorldInvTransposeView,&g_WorldInvTransposeView);

      V( pd3dImmediateContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
      CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;
      D3DXMatrixTranspose( &pVSPerObject->m_WorldViewProj, &g_mWorldViewProjection );
      D3DXMatrixTranspose( &pVSPerObject->m_WorldView, &g_mWorldView );
      D3DXMatrixTranspose( &pVSPerObject->m_WorldInvTransposeView, &g_WorldInvTransposeView );
      D3DXMatrixTranspose( &pVSPerObject->m_ViewToTexSpace, &g_ViewToTexSpace );
      
      //D3DXMatrixTranspose( &pVSPerObject->m_viewProj, &g_mViewProj );
      //D3DXMatrixTranspose( &pVSPerObject->m_World, &mWorld );
      //pVSPerObject->m_CameraOrigin= g_vecEye2;
      pd3dImmediateContext->Unmap( g_pcbVSPerObject, 0 );
   }

   void Display(ID3D11Device *pd3dDevice, ID3D11DeviceContext *pd3dImmediateContext)
   {
      UpdateBuffers(pd3dDevice, pd3dImmediateContext);

      if (m_renderTriangles) 
      {
         pd3dImmediateContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );
         pd3dImmediateContext->PSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

         pd3dImmediateContext->IASetInputLayout( g_pVertexLayout11 );

         pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

     /* if (m_modelDiffuseRV)
      {
         pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_modelDiffuseRV );
         pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamLinear );
      }*/

         // Set the shaders
         pd3dImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );
         pd3dImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );
      

         UINT stride= sizeof(CVertex); 
         UINT offset= 0;

      
         pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
         pd3dImmediateContext->IASetIndexBuffer(m_faceIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
         pd3dImmediateContext->DrawIndexed(m_triangles.size(), 0, 0);
      }
      
      if (m_renderEdges)
      {

         pd3dImmediateContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );
         pd3dImmediateContext->PSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

         // drawing edges
         pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

         pd3dImmediateContext->IASetInputLayout( g_pEdgeVertexLayout11 );

         pd3dImmediateContext->VSSetShader( g_pEdgeVertexShader, NULL, 0 );
         pd3dImmediateContext->PSSetShader( g_pEdgePixelShader, NULL, 0 );

         UINT stride= sizeof(CVertex); 
         UINT offset= 0;

         pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
         pd3dImmediateContext->IASetIndexBuffer(m_edgesIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
         pd3dImmediateContext->DrawIndexed(m_edges.size(), 0, 0);
      }
   }

   void SetupRenderData(ID3D11Device *pd3dDevice)
   {
      HRESULT hr;

      D3D11_BUFFER_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.Usage= D3D11_USAGE_DYNAMIC;
      desc.ByteWidth= m_vertices.size()*sizeof(CVertex);
      desc.BindFlags= D3D11_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags= D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags= 0;

      V( pd3dDevice->CreateBuffer(&desc, NULL, &m_vertexBuffer) );

      ZeroMemory(&desc, sizeof(desc));
      desc.Usage= D3D11_USAGE_IMMUTABLE;
      desc.ByteWidth= sizeof(int)*m_triangles.size();
      desc.BindFlags= D3D11_BIND_INDEX_BUFFER;
      desc.CPUAccessFlags= 0;
      desc.MiscFlags= 0;

      D3D11_SUBRESOURCE_DATA indexInitDesc;
      ZeroMemory(&indexInitDesc, sizeof(indexInitDesc));
      indexInitDesc.pSysMem= (const void *)m_triangles.data();
      indexInitDesc.SysMemPitch= 0;
      indexInitDesc.SysMemSlicePitch= 0;

      V( pd3dDevice->CreateBuffer(&desc, &indexInitDesc, &m_faceIndexBuffer) );

      ZeroMemory(&desc, sizeof(desc));
      desc.Usage= D3D11_USAGE_IMMUTABLE;
      desc.ByteWidth= sizeof(int)*m_edges.size();
      desc.BindFlags= D3D11_BIND_INDEX_BUFFER;
      desc.CPUAccessFlags= 0;
      desc.MiscFlags= 0;

      ZeroMemory(&indexInitDesc, sizeof(indexInitDesc));
      indexInitDesc.pSysMem= (const void *)m_edges.data();
      indexInitDesc.SysMemPitch= 0;
      indexInitDesc.SysMemSlicePitch= 0;

      V( pd3dDevice->CreateBuffer(&desc, &indexInitDesc, &m_edgesIndexBuffer) );

      ZeroMemory(&desc, sizeof(desc));
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags = 0;
      desc.ByteWidth = sizeof(s_mesh_aux_data);

      //V( pd3dDevice->CreateBuffer(&desc, NULL, &m_meshAuxData) );

      if (!m_matname_diffuseRV.empty())
      {
         //V( D3DX11CreateShaderResourceViewFromFileA( pd3dDevice, m_matname_diffuseRV.c_str(), NULL, NULL, &m_modelDiffuseRV, NULL ) );
      }
   }

   /*
   bool IsOnBoundary(int index) const
   {
      return m_boundary_vertices_bitmask[(index)>>3]&(1<<((index)&7))!=0;
   }
   */

   void IntersectTrianglesWithRay(const CRay &ray, float force_amplitude= 1.0f)
   {
      int best_face_index= -1;
      float best_rayt= 1e30f;

      for (int i=0; i<(int)m_triangles.size()/3; ++i)
      {

         /*
         if (!(IsOnBoundary(m_triangles[i*3+0]) &&
            IsOnBoundary(m_triangles[i*3+1]) &&
            IsOnBoundary(m_triangles[i*3+2])))
         {
            continue;
         }
         */

         D3DXVECTOR4 v[3];

         v[0]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i*3+0]].v, 1.0f);
         v[1]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i*3+1]].v, 1.0f);
         v[2]= D3DXVECTOR4(*(D3DVECTOR*)m_vertices[m_triangles[i*3+2]].v, 1.0f);

         D3DXVec4Transform(&v[0], &v[0], &m_transform);
         D3DXVec4Transform(&v[1], &v[1], &m_transform);
         D3DXVec4Transform(&v[2], &v[2], &m_transform);

         float rayt;
         if (ray.IntersectsWithTriangle(rayt, v[0], v[1], v[2]))
         {
            if (best_face_index==-1||rayt<best_rayt)
            {
               best_face_index= i;
               best_rayt= rayt;
            }
         }
      }

      if (best_face_index!=-1)
      {
         CVertex &v0= m_vertices[m_triangles[best_face_index*3+0]];
         CVertex &v1= m_vertices[m_triangles[best_face_index*3+1]];
         CVertex &v2= m_vertices[m_triangles[best_face_index*3+2]];

         v0.SetColor(1.0f, 0.0f, 0.0f, 1.0f);
         v1.SetColor(1.0f, 0.0f, 0.0f, 1.0f);
         v2.SetColor(1.0f, 0.0f, 0.0f, 1.0f);

         idVec3 force(ray.m_direction.x, ray.m_direction.y, ray.m_direction.z);
         //force*= 10.1f;
         //force*= 0.1f;
         force*= force_amplitude;

         m_forces[m_triangles[best_face_index*3+0]]+= force;
         m_forces[m_triangles[best_face_index*3+1]]+= force;
         m_forces[m_triangles[best_face_index*3+2]]+= force;

         g_needPhysicsUpdate= true;
      }
   }

   const std::vector<CVertex> & GetVertices() const
   {
      return m_vertices;
   }

   const D3DXMATRIX &GetTransform() const
   {
      return m_transform;
   }

   std::vector<CVertex> m_vertices;
   std::vector<int> m_edges;
   std::vector<int> m_faces;
   std::vector<int> m_triangles;
   std::vector<STetrahedron> m_tetrahedrons;
   std::vector<byte> m_boundary_vertices_bitmask;
   std::vector<float> m_vertices_displacements;
   std::vector<idVec3> m_forces;

   struct s_mesh_aux_data
   {
      float m_borderColorRgb[3];
      float m_faceColorRgb[3];
      float m_selectedFaceRgb[3];
   };

   int   m_selectedFaceIndex;


   ID3D11Buffer *m_vertexBuffer;
   ID3D11Buffer *m_faceIndexBuffer;
   ID3D11Buffer *m_edgesIndexBuffer;

   ID3D11ShaderResourceView *m_modelDiffuseRV;
   std::string m_matname_diffuseRV;

   ID3D11Buffer *m_meshAuxData;

   D3DXMATRIX m_transform;

   idSparseMatX m_staticSystemMatrixK;

   int m_subdivision_count;
   CBaseCurve *m_curve;
   CBaseSurface *m_surface;

   CKdTree m_kdtree;

   bool m_renderTriangles;
   bool m_renderEdges;

   float m_width, m_height;
};

void CKdAccelNode::initLeaf(int *primNums, int np)
{
   flags = 3;
   nPrims |= (np << 2);
   // Store primitive ids for leaf node
   if (np == 0)
      onePrimitive = 0;
   else if (np == 1)
      onePrimitive = primNums[0];
   else {
      primitives = new int[np];
      for (int i = 0; i < np; ++i)
         primitives[i] = primNums[i];
   }
}

void CKdTree::Build(const CMeshNode &mesh)
{
   m_mesh= &mesh;
   m_maxPrims= 5;
   m_maxDepth= 32;

   m_bounds.Clear();

   int nPrimitives=  m_mesh->m_triangles.size()/3;

   std::vector<idBounds> primBounds;

   for (int i = 0; i <nPrimitives; ++i) {
      D3DXVECTOR4 v[3];

      v[0]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*i+0]].v, 1.0f);
      v[1]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*i+1]].v, 1.0f);
      v[2]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*i+2]].v, 1.0f);

      idBounds primBound;
      primBound.Clear();

      for (int j=0;j<3;++j)
      {
         D3DXVec4Transform(&v[j], &v[j], &m_mesh->m_transform);

         idVec3 vv= idVec3(v[j].x, v[j].y, v[j].z);
         primBound.AddPoint(vv);
         m_bounds.AddPoint(vv);
      }

      primBounds.push_back(primBound);
   }

   m_nodes.reserve(1024*1024);

   std::vector<int> primNums(nPrimitives, 0);
   for (int i=0;i<primNums.size(); ++i)
   {
      primNums[i]= i;
   }

   CBoundEdge *edges[3];
   for (int i = 0; i < 3; ++i)
   {
      edges[i] = new CBoundEdge[2*nPrimitives];
   }

   int *prims0 = new int[nPrimitives];
   int *prims1 = new int[(m_maxDepth+1) * nPrimitives];

   BuildInternal(m_bounds, primBounds, &primNums[0], nPrimitives, m_maxDepth, edges, prims0, prims1, 0);

   for (int i = 0; i < 3; ++i)
      delete[] edges[i];
   delete[] prims0;
   delete[] prims1;
}


void CKdTree::BuildInternal(
   const idBounds &nodeBounds,
   const std::vector<idBounds> &allPrimBounds, int *primNums,
   int nPrimitives, int depth, CBoundEdge *edges[3],
   int *prims0, int *prims1, int badRefines
   )
{
   // Initialize leaf node if termination criteria met
   if (nPrimitives <= m_maxPrims || depth == 0) {
      m_nodes.push_back(CKdAccelNode());
      m_nodes.back().initLeaf(primNums, nPrimitives);
      return;
   }

   // Initialize interior node and continue recursion

   // Choose split axis position for interior node
   int bestAxis = -1, bestOffset = -1;
   float bestCost = idMath::INFINITY;
   float oldCost = 80 * float(nPrimitives);
   float totalSA = nodeBounds.SurfaceArea();
   float invTotalSA = 1.f / totalSA;
   idVec3 d = nodeBounds.GetMax() - nodeBounds.GetMin();

   // Choose which axis to split along
   int axis = nodeBounds.GetMaximumExtentIndex();
   int retries = 0;
retrySplit:


   // Initialize edges for _axis_
   for (int i = 0; i < nPrimitives; ++i) {
      int pn = primNums[i];
      const idBounds &bbox = allPrimBounds[pn];
      edges[axis][2*i] =   CBoundEdge(bbox.GetMin()[axis], pn, true);
      edges[axis][2*i+1] = CBoundEdge(bbox.GetMax()[axis], pn, false);
   }
   std::sort(&edges[axis][0], &edges[axis][2*nPrimitives]);

   // Compute cost of all splits for _axis_ to find best
   int nBelow = 0, nAbove = nPrimitives;
   for (int i = 0; i < 2*nPrimitives; ++i) {
      if (edges[axis][i].type == CBoundEdge::END) --nAbove;
      float edget = edges[axis][i].t;
      if (edget > nodeBounds.GetMin()[axis] &&
         edget < nodeBounds.GetMax()[axis]) {
            // Compute cost for split at _i_th edge
            int otherAxis0 = (axis + 1) % 3, otherAxis1 = (axis + 2) % 3;
            float belowSA = 2 * (d[otherAxis0] * d[otherAxis1] +
               (edget - nodeBounds.GetMin()[axis]) *
               (d[otherAxis0] + d[otherAxis1]));
            float aboveSA = 2 * (d[otherAxis0] * d[otherAxis1] +
               (nodeBounds.GetMax()[axis] - edget) *
               (d[otherAxis0] + d[otherAxis1]));
            float pBelow = belowSA * invTotalSA;
            float pAbove = aboveSA * invTotalSA;
            float eb = (nAbove == 0 || nBelow == 0) ? 0.5f : 0.f;
            float cost = 1 +
               80 * (1.f - eb) * (pBelow * nBelow + pAbove * nAbove);

            // Update best split if this is lowest cost so far
            if (cost < bestCost)  {
               bestCost = cost;
               bestAxis = axis;
               bestOffset = i;
            }
      }
      if (edges[axis][i].type == CBoundEdge::START) ++nBelow;
   }
   assert(nBelow == nPrimitives && nAbove == 0);

   // Create leaf if no good splits were found
   if (bestAxis == -1 && retries < 2) {
      ++retries;
      axis = (axis+1) % 3;
      goto retrySplit;
   }
   if (bestCost > oldCost) ++badRefines;
   if ((bestCost > 4.f * oldCost && nPrimitives < 16) ||
      bestAxis == -1 || badRefines == 3) 
   {
      m_nodes.push_back(CKdAccelNode());
      m_nodes.back().initLeaf(primNums, nPrimitives);
      return;
   }

   // Classify primitives with respect to split
   int n0 = 0, n1 = 0;
   for (int i = 0; i < bestOffset; ++i)
      if (edges[bestAxis][i].type == CBoundEdge::START)
         prims0[n0++] = edges[bestAxis][i].primNum;
   for (int i = bestOffset+1; i < 2*nPrimitives; ++i)
      if (edges[bestAxis][i].type == CBoundEdge::END)
         prims1[n1++] = edges[bestAxis][i].primNum;

   // Recursively initialize children nodes
   float tsplit = edges[bestAxis][bestOffset].t;
   idBounds bounds0 = nodeBounds, bounds1 = nodeBounds;
   bounds0.GetMax()[bestAxis] = bounds1.GetMin()[bestAxis] = tsplit;

   m_nodes.push_back(CKdAccelNode());
   CKdAccelNode *splitNode= &m_nodes.back();

   BuildInternal(bounds0,
      allPrimBounds, prims0, n0, depth-1, edges,
      prims0, prims1 + nPrimitives, badRefines);

   splitNode->initInterior(bestAxis, m_nodes.size(), tsplit);

   BuildInternal(bounds1, allPrimBounds, prims1, n1,
      depth-1, edges, prims0, prims1 + nPrimitives, badRefines);
}

struct KdToDo {
   const CKdAccelNode *node;
   float tmin, tmax;
};

bool CKdTree::Intersect(const CRay &ray) 
{
   idVec3 rayOrigin(ray.GetOrigin());
   idVec3 rayDir(ray.GetDirection());

   // Compute initial parametric range of ray inside kd-tree extent
   float tmin=0, tmax=0;
   //if (!m_bounds.RayIntersection(rayOrigin,rayDir,tmin,tmax))
   if (!m_bounds.IntersectRay(rayOrigin, rayDir, ray.GetMaxT(), &tmin, &tmax))
   {
      return false;
   }

   // Prepare to traverse kd-tree for ray
   idVec3 invDir(1.f/rayDir.x, 1.f/rayDir.y, 1.f/rayDir.z);
#define MAX_TODO 64
   KdToDo todo[MAX_TODO];
   int todoPos = 0;

   // Traverse kd-tree nodes in order for ray
   bool hit = false;
   const CKdAccelNode *node = &m_nodes[0];
   while (node != NULL) {
      // Bail out if we found a hit closer than the current node
      if (ray.GetMaxT() < tmin) break;
      if (!node->IsLeaf()) {
         // Process kd-tree interior node

         // Compute parametric distance along ray to split plane
         int axis = node->SplitAxis();
         float tplane = (node->SplitPos() - rayOrigin[axis]) * invDir[axis];

         // Get node children pointers for ray
         const CKdAccelNode *firstChild, *secondChild;
         int belowFirst = (rayOrigin[axis] <  node->SplitPos()) ||
            (rayOrigin[axis] == node->SplitPos() && rayDir[axis] <= 0);
         if (belowFirst) {
            firstChild = node + 1;
            secondChild = &m_nodes[node->AboveChild()];
         }
         else {
            firstChild = &m_nodes[node->AboveChild()];
            secondChild = node + 1;
         }

         // Advance to next child node, possibly enqueue other child
         if (tplane > tmax || tplane <= 0)
            node = firstChild;
         else if (tplane < tmin)
            node = secondChild;
         else 
         {
            // Enqueue _secondChild_ in todo list
            todo[todoPos].node = secondChild;
            todo[todoPos].tmin = tplane;
            todo[todoPos].tmax = tmax;
            ++todoPos;
            node = firstChild;
            tmax = tplane;
         }
      }
      else 
      {
         // Check for intersections inside leaf node
         int nPrimitives = node->nPrimitives();
         int primArr[1];
         int *prims= NULL;

         if (nPrimitives == 1) 
         {
            primArr[0]= node->onePrimitive;
            prims= primArr;
         }
         else 
         {
            prims = node->primitives;
         }


         for (int i = 0; i < nPrimitives; ++i) {
            D3DXVECTOR4 v[3];

            v[0]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*prims[i]+0]].v, 1.0f);
            v[1]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*prims[i]+1]].v, 1.0f);
            v[2]= D3DXVECTOR4(*(D3DVECTOR*)m_mesh->m_vertices[m_mesh->m_triangles[3*prims[i]+2]].v, 1.0f);

            D3DXVec4Transform(&v[0], &v[0], &m_mesh->m_transform);
            D3DXVec4Transform(&v[1], &v[1], &m_mesh->m_transform);
            D3DXVec4Transform(&v[2], &v[2], &m_mesh->m_transform);

            float rayt;
            if (ray.IntersectsWithTriangle(rayt, v[0], v[1], v[2]))
            {
               hit= true;
            }
         }

         // Grab next node to process from todo list
         if (todoPos > 0) {
            --todoPos;
            node = todo[todoPos].node;
            tmin = todo[todoPos].tmin;
            tmax = todo[todoPos].tmax;
         }
         else
            break;
      }
   }
   return hit;
}

static void FormatPrintf(std::string &str, const char *format, ...)
{

}

bool CTetGenMeshNodeSerializer::Serialize(const CMeshNode &mesh, const char *out_path)
{
   std::string buf;

   // vertices
   FormatPrintf(buf, "%d %d %d %d\n", mesh.m_vertices.size(), 3, 0, 1);
   for (int i=0;i<mesh.m_vertices.size(); ++i)
   {
      FormatPrintf(buf, "%d %f %f %f 1\n", i+1, mesh.m_vertices[i].v[0], mesh.m_vertices[i].v[1], mesh.m_vertices[i].v[2]);
   }

   // faces
   FormatPrintf(buf, "%d %d\n", mesh.m_triangles.size()/3, 1);
   for (int i=0;i<mesh.m_triangles.size()/3; ++i)
   {
      FormatPrintf(buf, "%d %d %d\n", 1, 0, 1);
      FormatPrintf(buf, "%d %d %d %d\n", 3, 1+mesh.m_triangles[i*3+0], 1+mesh.m_triangles[i*3+1], 1+mesh.m_triangles[i*3+2]);
   }

   // holes
   FormatPrintf(buf, "%d\n", 0);

   idWriteBloatFile(out_path, buf.c_str(), buf.size());

   return true;
}

bool CTetGenEleDeserializer::Deserialize(CMeshNode &mesh, const char *in_path, const char *neighbor_in_path)
{
   mesh.m_tetrahedrons.clear();

   char *buf= (char*)idBloatFile(in_path, NULL);

   if (!buf)
      return false;

   int number_of_tetrahedrons, vertices_per_tetrahedron, region;
   sscanf(buf, "%d %d %d", &number_of_tetrahedrons, &vertices_per_tetrahedron, &region);

   assert(vertices_per_tetrahedron==4);
   assert(region==0);

   mesh.m_tetrahedrons.resize(number_of_tetrahedrons);

   char *p= buf;
   while ((p=strchr(p, '\n')))
   {
      ++p;

      if (*p=='#')
      {
         continue;
      }

      STetrahedron tetr;

      int index, i0, i1, i2, i3;
      sscanf(p, "%d %d %d %d %d", &index, &i0, &i1, &i2, &i3);

      tetr.SetVertices(i0-1, i1-1, i2-1, i3-1);
      tetr.SetNeighbors(-1,-1,-1,-1);

      mesh.m_tetrahedrons[index-1]= tetr;
   }

   free(buf);

   buf= (char*)idBloatFile(neighbor_in_path, NULL);

   if (!buf)
      return false;

   sscanf(buf, "%d 4", &number_of_tetrahedrons);

   p= buf;
   while ((p=strchr(p, '\n')))
   {
      ++p;

      if (*p=='#')
      {
         continue;
      }

      int index, i0, i1, i2, i3;
      sscanf(p, "%d %d %d %d %d", &index, &i0, &i1, &i2, &i3);

      mesh.m_tetrahedrons[index-1].SetNeighbors(i0>0?i0-1:-1, i1>0?i1-1:-1, i2>0?i2-1:-1, i3>0?i3-1:-1);
   }

   free(buf);

   mesh.m_triangles.clear();
   for (int i=0;i<mesh.m_tetrahedrons.size();++i)
   {
      const STetrahedron &tetr= mesh.m_tetrahedrons[i];

      bool b0= tetr.neighbors[0]==-1;
      bool b1= tetr.neighbors[1]==-1;
      bool b2= tetr.neighbors[2]==-1;
      bool b3= tetr.neighbors[3]==-1;

      if (b0)
      {
         mesh.m_triangles.push_back(tetr.index[1]);
         mesh.m_triangles.push_back(tetr.index[2]);
         mesh.m_triangles.push_back(tetr.index[3]);

         mesh.m_edges.push_back(tetr.index[1]); mesh.m_edges.push_back(tetr.index[2]);
         mesh.m_edges.push_back(tetr.index[2]); mesh.m_edges.push_back(tetr.index[3]);
         mesh.m_edges.push_back(tetr.index[3]); mesh.m_edges.push_back(tetr.index[1]);
      }

      if (b1)
      {
         mesh.m_triangles.push_back(tetr.index[0]);
         mesh.m_triangles.push_back(tetr.index[2]);
         mesh.m_triangles.push_back(tetr.index[3]);

         mesh.m_edges.push_back(tetr.index[0]); mesh.m_edges.push_back(tetr.index[2]);
         mesh.m_edges.push_back(tetr.index[2]); mesh.m_edges.push_back(tetr.index[3]);
         mesh.m_edges.push_back(tetr.index[3]); mesh.m_edges.push_back(tetr.index[0]);
      }

      if (b2)
      {
         mesh.m_triangles.push_back(tetr.index[0]);
         mesh.m_triangles.push_back(tetr.index[1]);
         mesh.m_triangles.push_back(tetr.index[3]);

         mesh.m_edges.push_back(tetr.index[0]); mesh.m_edges.push_back(tetr.index[1]);
         mesh.m_edges.push_back(tetr.index[1]); mesh.m_edges.push_back(tetr.index[3]);
         mesh.m_edges.push_back(tetr.index[3]); mesh.m_edges.push_back(tetr.index[0]);
      }

      if (b3)
      {
         mesh.m_triangles.push_back(tetr.index[0]);
         mesh.m_triangles.push_back(tetr.index[1]);
         mesh.m_triangles.push_back(tetr.index[2]);

         mesh.m_edges.push_back(tetr.index[0]); mesh.m_edges.push_back(tetr.index[1]);
         mesh.m_edges.push_back(tetr.index[1]); mesh.m_edges.push_back(tetr.index[2]);
         mesh.m_edges.push_back(tetr.index[2]); mesh.m_edges.push_back(tetr.index[0]);
      }
   }

   return true;
}

bool CTetGenNodeDeserializer::Deserialize(CMeshNode &mesh, const char *in_path)
{
   mesh.m_vertices.clear();

   char *buf= (char*)idBloatFile(in_path, NULL);

   if (!buf)
      return false;

   int number_of_vertices, dimension, number_of_attributes, markers;
   sscanf(buf, "%d %d %d %d", &number_of_vertices, &dimension, &number_of_attributes, &markers);

   assert(dimension==3);
   assert(markers==1);

   mesh.m_vertices.resize(number_of_vertices);
   mesh.m_boundary_vertices_bitmask.resize((number_of_vertices+7)/8, 0);
   mesh.m_vertices_displacements.resize(number_of_vertices*3, 0.0f);
   mesh.m_forces.resize(number_of_vertices, idVec3(0.0f, 0.0f, 0.0f));

   char *p= buf;
   while ((p=strchr(p, '\n')))
   {
      ++p;

      if (*p=='#')
      {
         continue;
      }

      CVertex vert;

      int index, is_boundary; float f0, f1, f2;
      sscanf(p, "%d %f %f %f %d", &index, &f0, &f1, &f2, &is_boundary);

      mesh.m_boundary_vertices_bitmask[(index-1)/8]|= ((is_boundary?1:0)<<(index&7));

      vert.SetVertices(f0, f1, f2);

      mesh.m_vertices[index-1]= vert;
   }

   free(buf);
   return true;
}

CMeshNode g_quad[2];
//CMeshNode g_cube;
CMeshNode g_model;
CMeshNode g_curve;
CMeshNode g_surf;
CMeshNode g_obb;
CMeshNode g_bookStandMesh;

CRay g_cached_ray;
float g_force_amplitude= 0.1f;
bool g_rightButtonDown= false;

void CALLBACK OnMouseClick( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext )
{
#if 1
   if (!bRightButtonDown)
   {
      g_rightButtonDown= false;
   }

   if (bRightButtonDown)
   {
      g_rightButtonDown= true;

      //D3DXVECTOR3 pos;
      //

      //pos.x= (float)xPos/g_window_width;
      //pos.y= (float)yPos/g_window_height;
      //pos.z= 1.0f;

      //pos.x= 2.0f*pos.x-1.0f;
      //pos.y= 2.0f*pos.y-1.0f;

      //g_mWorld = *g_Camera.GetWorldMatrix();
      g_mProj = *g_Camera.GetProjMatrix();
      //g_mView = *g_Camera.GetViewMatrix();

      //g_mWorldViewProjection = g_mWorld * g_mView * g_mProj;

      //D3DXMATRIX worldViewProjectionTranspose;
      //D3DXMatrixTranspose(&worldViewProjectionTranspose, g_mWorldViewProjection);

      //D3DXVECTOR4 dir;
      //D3DXVec3Transform(&dir, &pos, &worldViewProjectionTranspose);
      //D3DXVec3Normalize(&dir, &dir);


      D3DXVECTOR3 v;
      v.x = ( ( ( 2.0f * xPos ) / g_window_width ) - 1 ) / g_mProj._11;
      v.y = -( ( ( 2.0f * yPos ) / g_window_height ) - 1 ) / g_mProj._22;
      v.z = 1.0f;

      // Get the inverse view matrix
      const D3DXMATRIX matView = *g_Camera.GetViewMatrix();
      const D3DXMATRIX matWorld = *g_Camera.GetWorldMatrix();
      D3DXMATRIX mWorldView = matWorld * matView;
      D3DXMATRIX m;
      D3DXMatrixInverse( &m, NULL, &mWorldView );

      D3DXVECTOR3 vPickRayDir, vPickRayOrig;

      // Transform the screen space Pick ray into 3D space
      vPickRayDir.x = v.x * m._11 + v.y * m._21 + v.z * m._31;
      vPickRayDir.y = v.x * m._12 + v.y * m._22 + v.z * m._32;
      vPickRayDir.z = v.x * m._13 + v.y * m._23 + v.z * m._33;
      vPickRayOrig.x = m._41;
      vPickRayOrig.y = m._42;
      vPickRayOrig.z = m._43;

      vPickRayDir*= 4096.0f;

      g_cached_ray= CRay::FromOriginDirection(vPickRayOrig, vPickRayDir);

      //g_model.ZeroForces();

      //g_cube.IntersectFacesWithRay(ray);
      //g_force_amplitude= 0.1f;
      //g_model.IntersectTrianglesWithRay(g_cached_ray, g_force_amplitude);
   }
#endif
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    idLib::Init();

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackMouse( OnMouseClick );


    /*
    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    */


    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"BasicHLSL11" );
    DXUTCreateDevice (D3D_FEATURE_LEVEL_9_2, true, 800, 600 );
    //DXUTCreateDevice(true, 640, 480);
    DXUTMainLoop(); // Enter into the DXUT render loop

    idLib::ShutDown();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    D3DXVECTOR3 vLightDir( -1, 1, -1 );
    D3DXVec3Normalize( &vLightDir, &vLightDir );
    g_LightControl.SetLightDirection( vLightDir );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

        g_depthStencilFormat= pDeviceSettings->d3d11.AutoDepthStencilFormat;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    //g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
            DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        g_pTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 20 * 6 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 20 * 5 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate light: Right mouse button\n"
                                    L"Rotate camera: Middle mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );

        g_pTxtHelper->SetInsertionPos( 550, nBackBufferHeight - 20 * 5 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Quit: ESC\n" );
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
#if 1
    if( bKeyDown )
    {
        switch( nChar )
        {
            /*case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
                break;*/

            case VK_LEFT:
               //g_figureAngle-= 0.01f;
               g_bookStand.MoveLeft(0.1f);
               break;
            case VK_RIGHT:
               //g_figureAngle+= 0.01f;
               g_bookStand.MoveRight(0.1f);
               break;
            case VK_UP:
               //g_figureAngle-= 0.01f;
               g_bookStand.MoveLeft(1.0f);
               break;
            case VK_DOWN:
               //g_figureAngle+= 0.01f;
               g_bookStand.MoveRight(1.0f);
               break;


   /*         case VK_UP:
            case VK_DOWN:
               float step= (nChar==VK_UP?1.0f:-1.0f)*20.0f;
               if (g_cube.GetSelectedFaceIndex()!=-1)
               {
                  g_cube.MoveFaceAlongNormal(g_cube.GetSelectedFaceIndex(), step);
               }
               break;*/
        }
    }
#endif
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
    }

}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Find and compile the specified shader
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

FLOAT fObjectRadius = 378.15607f;

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;


    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    if (!g_bookStand.Init(pd3dDevice, pd3dImmediateContext, "F:\\knuth3.pak")) {
       assert(0);
       return S_FALSE;
    }

    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    D3DXVECTOR3 vCenter( 0, 0, 0 ); // 0.25767413f, -28.503521f, 111.00689f );
    
    g_backbufferWidth= pBackBufferSurfaceDesc->Width;
    g_backbufferHeight= pBackBufferSurfaceDesc->Height;

    D3DXMatrixTranslation( &g_mCenterMesh, -vCenter.x, -vCenter.y, -vCenter.z );
    D3DXMATRIXA16 m;
    //D3DXMatrixRotationY( &m, D3DX_PI );
    //g_mCenterMesh *= m;
    //D3DXMatrixRotationX( &m, D3DX_PI / 2.0f );
    //g_mCenterMesh *= m;

    // Compile the shaders using the lowest possible profile for broadest feature level support
    ID3DBlob* pVertexShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"BasicHLSL11_VS.hlsl", "VSMain", "vs_4_0", &pVertexShaderBuffer ) );

    ID3DBlob* pPixelShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"BasicHLSL11_PS.hlsl", "PSMain", "ps_4_0", &pPixelShaderBuffer ) );

    // Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), NULL, &g_pVertexShader ) );
    DXUT_SetDebugName( g_pVertexShader, "VSMain" );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pPixelShader ) );
    DXUT_SetDebugName( g_pPixelShader, "PSMain" );

    

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
       { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
       { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
       { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
       { "COLOR",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }

        //{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        //{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        //{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        //{ "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVertexShaderBuffer->GetBufferPointer(),
                                             pVertexShaderBuffer->GetBufferSize(), &g_pVertexLayout11 ) );
    DXUT_SetDebugName( g_pVertexLayout11, "Primary" );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    pVertexShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"BasicEdgeHLSL11_VS.hlsl", "VSMain", "vs_4_0_level_9_1", &pVertexShaderBuffer ) );

    pPixelShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"BasicEdgeHLSL11_PS.hlsl", "PSMain", "ps_4_0_level_9_1", &pPixelShaderBuffer ) );


    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
       pVertexShaderBuffer->GetBufferSize(), NULL, &g_pEdgeVertexShader ) );
    DXUT_SetDebugName( g_pEdgeVertexShader, "VSMain" );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
       pPixelShaderBuffer->GetBufferSize(), NULL, &g_pEdgePixelShader ) );
    DXUT_SetDebugName( g_pEdgePixelShader, "PSMain" );

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVertexShaderBuffer->GetBufferPointer(),
       pVertexShaderBuffer->GetBufferSize(), &g_pEdgeVertexLayout11 ) );
    DXUT_SetDebugName( g_pEdgeVertexLayout11, "PrimaryEdge" );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );


    pVertexShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"SSAODepthVertexHLSL11_VS.hlsl", "VSMain", "vs_4_0_level_9_1", &pVertexShaderBuffer ) );

    pPixelShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"SSAODepthPixelHLSL11_PS.hlsl", "PSMain", "ps_4_0_level_9_1", &pPixelShaderBuffer ) );


    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
       pVertexShaderBuffer->GetBufferSize(), NULL, &g_pSSAODepthVertexShader ) );
    DXUT_SetDebugName( g_pSSAODepthVertexShader, "VSMain" );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
       pPixelShaderBuffer->GetBufferSize(), NULL, &g_pSSAODepthPixelShader ) );
    DXUT_SetDebugName( g_pSSAODepthPixelShader, "PSMain" );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    pVertexShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"SSAOPassVertexHLSL11_VS.hlsl", "VSMain", "vs_5_0", &pVertexShaderBuffer ) );

    pPixelShaderBuffer = NULL;
    V_RETURN( CompileShaderFromFile( L"SSAOPassPixelHLSL11_PS.hlsl", "PSMain", "ps_5_0", &pPixelShaderBuffer ) );


    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
       pVertexShaderBuffer->GetBufferSize(), NULL, &g_pSSAOPassVertexShader ) );
    DXUT_SetDebugName( g_pSSAODepthVertexShader, "VSMain" );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
       pPixelShaderBuffer->GetBufferSize(), NULL, &g_pSSAOPassPixelShader ) );
    DXUT_SetDebugName( g_pSSAODepthPixelShader, "PSMain" );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );



    //pVertexShaderBuffer = NULL;
    //V_RETURN( CompileShaderFromFile( L"BasicHLSL11_VS_Light.hlsl", "VSMain", "vs_4_0_level_9_1", &pVertexShaderBuffer ) );

    //pPixelShaderBuffer = NULL;
    //V_RETURN( CompileShaderFromFile( L"BasicHLSL11_PS_Light.hlsl", "PSMain", "ps_4_0_level_9_1", &pPixelShaderBuffer ) );


    //V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
    //   pVertexShaderBuffer->GetBufferSize(), NULL, &g_pLightVertexShader ) );
    //DXUT_SetDebugName( g_pLightVertexShader, "VSMain" );
    //V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
    //   pPixelShaderBuffer->GetBufferSize(), NULL, &g_pLightPixelShader ) );
    //DXUT_SetDebugName( g_pLightPixelShader, "PSMain" );



    //SAFE_RELEASE( pVertexShaderBuffer );
    //SAFE_RELEASE( pPixelShaderBuffer );

    // Load the mesh
    V_RETURN( g_Mesh11.Create( pd3dDevice, L"tiny\\tiny.sdkmesh", true ) );


    // Create a sampler state
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamLinear ) );
    DXUT_SetDebugName( g_pSamLinear, "Primary" );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbVSPerObject ) );
    DXUT_SetDebugName( g_pcbVSPerObject, "CB_VS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPSPerObject ) );
    DXUT_SetDebugName( g_pcbPSPerObject, "CB_PS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_FRAME );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPSPerFrame ) );
    DXUT_SetDebugName( g_pcbPSPerFrame, "CB_PS_PER_FRAME" );

    D3D11_TEXTURE2D_DESC textureDesc;
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthTargetViewDesc;



    ZeroMemory(&textureDesc, sizeof(textureDesc));
    textureDesc.Width= g_backbufferWidth;
    textureDesc.Height= g_backbufferHeight;
    textureDesc.MipLevels= 1;
    textureDesc.ArraySize= 1;
    textureDesc.Format= DXGI_FORMAT_R32G32B32A32_FLOAT;
    //textureDesc.Format= DXGI_FORMAT_R24G8_TYPELESS;
    textureDesc.SampleDesc.Count= 1;
    textureDesc.SampleDesc.Quality= 0;
    textureDesc.Usage= D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    //textureDesc.BindFlags= D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags= 0;
    textureDesc.MiscFlags= 0;

    V_RETURN( pd3dDevice->CreateTexture2D(&textureDesc, NULL, &g_renderTargetTextureMap) );


   ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
    renderTargetViewDesc.Format= textureDesc.Format;
    renderTargetViewDesc.ViewDimension= D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice= 0;

    V_RETURN( pd3dDevice->CreateRenderTargetView(g_renderTargetTextureMap, &renderTargetViewDesc, &g_renderTargetViewMap) );

    /*ZeroMemory(&depthTargetViewDesc, sizeof(depthTargetViewDesc));
    depthTargetViewDesc.Format= DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthTargetViewDesc.ViewDimension= D3D11_DSV_DIMENSION_TEXTURE2D;
    depthTargetViewDesc.Texture2D.MipSlice= 0;

    V_RETURN( pd3dDevice->CreateDepthStencilView(g_renderTargetTextureMap, &depthTargetViewDesc, &g_depthStencilTargetViewMap) );*/

    ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
    shaderResourceViewDesc.Format= textureDesc.Format; //DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    shaderResourceViewDesc.ViewDimension= D3D_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip= 0;
    shaderResourceViewDesc.Texture2D.MipLevels= 1;

    V_RETURN( pd3dDevice->CreateShaderResourceView(g_renderTargetTextureMap, &shaderResourceViewDesc, &g_shaderResourceViewMap) );

    //D3D11_DEPTH_STENCIL_DESC depthDesc;
    //ZeroMemory(&depthDesc, sizeof(depthDesc));

    //V_RETURN( pd3dDevice->CreateDepthStencilState(&depthDesc, &g_pDepthStencilState) );

    D3D11_RASTERIZER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.FillMode= D3D11_FILL_SOLID;
    //desc.FillMode= D3D11_FILL_WIREFRAME;
    desc.CullMode= D3D11_CULL_NONE;
    desc.MultisampleEnable= FALSE;
    desc.AntialiasedLineEnable= TRUE;

    pd3dDevice->CreateRasterizerState(&desc, &g_pRasterizerState);
    pd3dImmediateContext->RSSetState(g_pRasterizerState);

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -300.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    //g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

    
    D3DXVECTOR3 vecEye2( g_vecEye2.x, g_vecEye2.y, g_vecEye2.z );
    D3DXVECTOR3 vecAt2 ( 0.0f, 0.0f, 0.0f );
    g_FlipCamera.SetViewParams( &vecEye2, &vecAt2 );
    g_FlipCamera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

    {
      D3DXMATRIX curveMatrix, surfMatrix, cubeMatrix, rotationMatrixX, rotationMatrixY, scalingMatrix, identityMatrix;
      D3DXMatrixTranslation( &cubeMatrix, 0,0,200 );
      D3DXMatrixTranslation( &surfMatrix, -250.0f, -250.0f, -250.0f );
      D3DXMatrixTranslation( &curveMatrix, 0, 0.0f, 0 );
      D3DXMatrixScaling(&scalingMatrix, 1.0f, 1.0f, 1.0f);
      D3DXMatrixRotationX(&rotationMatrixX, 3*D3DX_PI/7.0f);
      D3DXMatrixRotationZ(&rotationMatrixY, 3*D3DX_PI/15.0f);
      D3DXMatrixIdentity(&identityMatrix);
      //curveMatrix=rotationMatrixX*rotationMatrixY*curveMatrix;
      //curveMatrix=scalingMatrix*curveMatrix;
      //surfMatrix=scalingMatrix*surfMatrix;
      //curveMatrix=scalingMatrix*curveMatrix;
      cubeMatrix=rotationMatrixY*rotationMatrixX*scalingMatrix*cubeMatrix;

      g_quad[0].SetupQuad(pd3dDevice, curveMatrix, 1.0f);
      g_bookStandMesh.SetupBookShelf(pd3dDevice, curveMatrix, g_bookStand.GetPages(), 128.0f);
      //g_cube.SetupCube(pd3dDevice, cubeMatrix, 100.0f);
      //g_model.SetupFromAse("F:\\BrowserDownloads\\ku5rtyobmp-AK47\\ak47.ASE", pd3dDevice, cubeMatrix);
      //g_model.DeserializeTetrahedronsTetGen("D:\\projects\\tetgen1.5.0\\soccer.1.ele");
      //g_model.SetupFromAse("F:\\BrowserDownloads\\q78u2sv9fx1c-soccerball\\soccer.ASE", pd3dDevice, cubeMatrix);
      //g_model.SetupFromAse("F:\\BrowserDownloads\\ls56yrhqxxxc-SVD\\svd.ASE", pd3dDevice, cubeMatrix);
      //g_model.SetupAmbientOcclusionCPU();
      //g_model.SerializeToTetgen("D:\\projects\\tetgen1.5.0\\soccer.poly");

      //g_model.SetupFromTetGen(pd3dDevice, cubeMatrix, "D:\\projects\\tetgen1.5.0\\soccer.1");
      //g_model.SolveStaticSystem();
      //g_model.SerializeToTetgen("ak47.poly");

      {
         CObb obb;

         
         std::vector<idVec3> obb_vertices;

         const std::vector<CVertex> &vertices= g_model.GetVertices();

         for (int i=0; i<vertices.size(); ++i)
         {
            D3DXVECTOR4 v;
            v.x= vertices[i].v[0];
            v.y= vertices[i].v[1];
            v.z= vertices[i].v[2];
            v.w= 1.0f;
            
            D3DXVec4Transform(&v,&v,&cubeMatrix);

            obb_vertices.push_back(idVec3(v.x,v.y,v.z));
         }

         obb.Build(obb_vertices);

         g_obb.SetupFromObb(pd3dDevice, identityMatrix, obb);
      }

      //g_curve.SetupCurve(pd3dDevice, curveMatrix, _bspline_curve_type,1024);
      //g_surf.SetupSurface(pd3dDevice, surfMatrix, _bspline_surface_type, 32);

      if (CBSplineCurve* bspline= dynamic_cast<CBSplineCurve*>(g_curve.GetCurve()))
      {
         //bspline->CurveKnotInsert(0.55, 1);
      }
    }

    g_needPhysicsUpdate= true;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( g_field_of_view/2, g_fAspectRatio, 2.0f, g_zFar );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    //g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_FlipCamera.SetProjParams( D3DX_PI / 4, g_fAspectRatio, 2.0f, 16000.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    g_window_width= (float)pBackBufferSurfaceDesc->Width;
    g_window_height= (float)pBackBufferSurfaceDesc->Height;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    g_bookStand.Update();
  

    D3DXMATRIX figureMat;
    D3DXMatrixRotationZ(&figureMat, g_figureAngle);

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
    pd3dImmediateContext->ClearRenderTargetView( g_renderTargetViewMap, ClearColor );

    D3DXMATRIX mWorldViewProjection;
    D3DXVECTOR3 vLightDir;


    // Get the projection & view matrix from the camera class
    //mProj = *g_Camera.GetProjMatrix();
    //mView = *g_Camera.GetViewMatrix();

    // Get the light direction
    vLightDir = g_LightControl.GetLightDirection();


    D3D11_MAPPED_SUBRESOURCE MappedResource;

    /*
    // Per frame cb update
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pcbPSPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_PS_PER_FRAME* pPerFrame = ( CB_PS_PER_FRAME* )MappedResource.pData;
    float fAmbient = 0.1f;
    pPerFrame->m_vLightDirAmbient = D3DXVECTOR4( vLightDir.x, vLightDir.y, vLightDir.z, fAmbient );
    pd3dImmediateContext->Unmap( g_pcbPSPerFrame, 0 );
    pd3dImmediateContext->PSSetConstantBuffers( g_iCBPSPerFrameBind, 1, &g_pcbPSPerFrame );
    */

    //Get the mesh
    //IA setup

    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout11 );

    //UINT Strides[1];
    //UINT Offsets[1];
    //ID3D11Buffer* pVB[1];
    //pVB[0] = g_Mesh11.GetVB11( 0, 0 );
    //Strides[0] = ( UINT )g_Mesh11.GetVertexStride( 0, 0 );
    //Offsets[0] = 0;
    //pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    //pd3dImmediateContext->IASetIndexBuffer( g_Mesh11.GetIB11( 0 ), g_Mesh11.GetIBFormat11( 0 ), 0 );
    //


    D3DXMATRIX m;
    D3DXMatrixRotationX(&m, 3*D3DX_PI/2);
    
    // Set the per object constant data
    g_mWorld = *g_Camera.GetWorldMatrix();
    g_mProj = *g_Camera.GetProjMatrix();
    g_mView = *g_Camera.GetViewMatrix();

    g_mWorldViewProjection = g_mWorld * g_mView * g_mProj;

    //pd3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilState, 0);


    pd3dImmediateContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );
    pd3dImmediateContext->PSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

    pd3dImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );
    pd3dImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );

    /*pd3dImmediateContext->VSSetShader( g_pSSAODepthVertexShader, NULL, 0 );
    pd3dImmediateContext->PSSetShader( g_pSSAODepthPixelShader, NULL, 0 );*/

    //ID3D11RenderTargetView *renderTargets[1]= {g_renderTargetViewMap};
    //pd3dImmediateContext->OMSetRenderTargets(1, &g_renderTargetViewMap, pDSV);


    //g_cube.Display(pd3dDevice, pd3dImmediateContext);
    //g_model.Display(pd3dDevice, pd3dImmediateContext);

    //g_obb.Display(pd3dDevice, pd3dImmediateContext);

    g_bookStand.SubmitShaderResources();
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamLinear );

    g_bookStandMesh.Display(pd3dDevice, pd3dImmediateContext);
    
    //g_curve.Display(pd3dDevice, pd3dImmediateContext);
    //g_surf.Display(pd3dDevice, pd3dImmediateContext);

    //pd3dImmediateContext->VSSetShader( g_pSSAOPassVertexShader, NULL, 0 );
    //pd3dImmediateContext->PSSetShader( g_pSSAOPassPixelShader, NULL, 0 );

    //pd3dImmediateContext->OMSetRenderTargets(1,&pRTV,pDSV);

    //pd3dImmediateContext->PSSetShaderResources(0,1,&g_shaderResourceViewMap);


    //g_quad[0].Display(pd3dDevice, pd3dImmediateContext);
    //g_model.Display(pd3dDevice, pd3dImmediateContext);


    //DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    //g_HUD.OnRender( fElapsedTime );
    //g_SampleUI.OnRender( fElapsedTime );
    //RenderText();
    //DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh11.Destroy();
                
    SAFE_RELEASE( g_pVertexLayout11 );
    SAFE_RELEASE( g_pEdgeVertexLayout11 );
    SAFE_RELEASE( g_pVertexBuffer );
    SAFE_RELEASE( g_pIndexBuffer );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pSSAODepthVertexShader );
    SAFE_RELEASE( g_pSSAODepthPixelShader );
    SAFE_RELEASE( g_pSamLinear );
    SAFE_RELEASE( g_pEdgeVertexShader );
    SAFE_RELEASE( g_pEdgePixelShader );
    SAFE_RELEASE( g_pLightVertexShader );
    SAFE_RELEASE( g_pLightPixelShader );

    SAFE_RELEASE( g_pcbVSPerObject );
    SAFE_RELEASE( g_pcbPSPerObject );
    SAFE_RELEASE( g_pcbPSPerFrame );

    SAFE_RELEASE(g_renderTargetTextureMap);
    SAFE_RELEASE(g_renderTargetViewMap);
    SAFE_RELEASE(g_shaderResourceViewMap);

    SAFE_RELEASE(g_depthStencilTargetViewMap);

    SAFE_RELEASE(g_pDepthStencilState);

    SAFE_RELEASE(g_pRasterizerState);

    SAFE_RELEASE(g_pSSAOPassVertexShader);
    SAFE_RELEASE(g_pSSAOPassPixelShader);

    g_quad[0].Destroy();
    //g_quad[1].Destroy();
    //g_cube.Destroy();
    g_model.Destroy();
    g_curve.Destroy();
    g_surf.Destroy();
    g_obb.Destroy();
    g_bookStandMesh.Destroy();
}





