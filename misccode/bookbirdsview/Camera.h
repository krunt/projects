#ifndef CAMERA_DEF_H_
#define CAMERA_DEF_H_

#include <d3d9types.h>

enum ViewMode
{
   _ViewModeNone,
   _ViewModeOrbit,
   _ViewModeHand,
   _ViewModeAround,
};

class CCamera
{
public:
   CCamera()
   {
      m_view_mode= _ViewModeNone;

      m_near= 2.0f;
      m_far= 4000.0f;

      m_aspect_ratio= 1.0f;

      m_field_of_view= -D3DX_PI / 2;
      //m_field_of_view= D3DX_PI / 4;

      ZeroMemory(&m_view_matrix, sizeof(m_view_matrix));
      ZeroMemory(&m_projection_matrix, sizeof(m_projection_matrix));

      D3DXMatrixPerspectiveRH(&m_projection_matrix, m_field_of_view, m_aspect_ratio, m_near, m_far);

      D3DXMatrixIdentity(&m_world_matrix);
   }

   /*
   void MyD3DXMatrixPerspectiveRH(D3DXMATRIX *projection_matrix, float field_of_view, float aspect_ratio, float znear, float zfar)
   {
      float tan_fov_x= tan(field_of_view);
      float tan_fov_y= tan_fov_x/aspect_ratio;

      projection_matrix->m[0][0]= tan_fov_x;
      projection_matrix->m[1][0]= 0.0f;
      projection_matrix->m[2][0]= 0.0f;
      projection_matrix->m[3][0]= 0.0f;

      projection_matrix->m[0][1]= 0.0f;
      projection_matrix->m[1][1]= tan_fov_y;
      projection_matrix->m[2][1]= 0.0f;
      projection_matrix->m[3][1]= 0.0f;

      projection_matrix->m[0][2]= 0.0f;
      projection_matrix->m[1][2]= 0.0f;
      projection_matrix->m[2][2]= -1.0f;
      projection_matrix->m[3][2]= -znear; //-2.0f*znear;

      projection_matrix->m[0][3]= 0.0f;
      projection_matrix->m[1][3]= 0.0f;
      projection_matrix->m[2][3]= -1.0f;
      projection_matrix->m[3][3]= 0.0f;
   }
   */

   void SetProjParams(float field_of_view, float aspect_ratio, float znear, float zfar)
   {
      //m_field_of_view= -field_of_view; 
      m_aspect_ratio= aspect_ratio;
      m_near= znear;
      m_far= zfar;

      D3DXMatrixPerspectiveRH(&m_projection_matrix, m_field_of_view, m_aspect_ratio, m_near, m_far);
   }

   void SetWindow(long width, long height)
   {
      m_aspect_ratio= (float)width / height;
   }

   void HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      if(uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONUP || uMsg == WM_MBUTTONDOWN)
      {
         m_view_mode= _ViewModeNone;
      }

      if(uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN)
      {
         m_view_mode= _ViewModeNone;
         m_drag_start_x= ( short )LOWORD( lParam );
         m_drag_start_y= ( short )HIWORD( lParam );
         m_drag_start_transform= m_world_matrix;

         switch (uMsg)
         {
         case   WM_LBUTTONDOWN:
            m_view_mode= _ViewModeOrbit;
            break;
         //case  WM_RBUTTONDOWN:
         //   m_view_mode= _ViewModeHand;
         //   break;
         case WM_RBUTTONDOWN:
            m_view_mode= _ViewModeHand; //_ViewModeAround;
            break;
         };
      }

      if( uMsg == WM_MOUSEMOVE )
      {
         int new_x= ( short )LOWORD( lParam );
         int new_y= ( short )HIWORD( lParam );

         
         switch (m_view_mode)
         {

            case _ViewModeOrbit:
            {
               D3DXMATRIX rotationMatrixX, rotationMatrixY, rotationMatrixXY;
               D3DXMatrixRotationY(&rotationMatrixY, (new_x-m_drag_start_x)*.005f);
               D3DXMatrixRotationX(&rotationMatrixX, (new_y-m_drag_start_y)*.005f);

               D3DXMatrixMultiply(&rotationMatrixXY, &rotationMatrixY, &rotationMatrixX);
               D3DXMatrixMultiply(&m_world_matrix, &rotationMatrixXY, &m_drag_start_transform);
            }
            break;

            case _ViewModeHand:
            {
               D3DXMATRIX translationMatrix;

               float incrementX= -(float)(new_x-m_drag_start_x)*0.5f;
               float incrementY= (float)(new_y-m_drag_start_y)*0.5f;

               D3DXMatrixTranslation(&translationMatrix, incrementX, incrementY, 0.0f);
               D3DXMatrixMultiply(&m_world_matrix, &translationMatrix, &m_drag_start_transform);
            }
            break;

            case _ViewModeAround:
            {
               D3DXMATRIX rotationMatrixX, rotationMatrixY, rotationMatrixXY;
               D3DXMatrixRotationY(&rotationMatrixY, (new_x-m_drag_start_x)*.005f);
               D3DXMatrixRotationX(&rotationMatrixX, (new_y-m_drag_start_y)*.005f);

               D3DXMatrixMultiply(&rotationMatrixXY, &rotationMatrixY, &rotationMatrixX);
               D3DXMatrixMultiply(&m_world_matrix, &m_drag_start_transform, &rotationMatrixXY);

               //D3DXQUATERNION quatX, quatY;
               //D3DXQuaternionMultiply

               //D3DXQUATERNION quat;
               //D3DXQuaternionRotationYawPitchRoll(&quat, (new_x-m_drag_start_x)*.005f, (new_y-m_drag_start_y)*.005f, 0.0f);

               //D3DXMATRIX quatMatrix;
               //D3DXMatrixRotationQuaternion(&quatMatrix, &quat);

               //D3DXMatrixMultiply(&m_world_matrix, &m_drag_start_transform, &quatMatrix);
            }
            break;

         };
      }

      if (uMsg == WM_MOUSEWHEEL)
      {
         D3DXMATRIX translationMatrix;

         float increment= (float)((short)HIWORD(wParam))*0.8f;

         D3DXMatrixTranslation(&translationMatrix, 0.0f, 0.0f, -increment);
         D3DXMatrixMultiply(&m_world_matrix, &m_world_matrix, &translationMatrix);
      }
   }

   void SetViewParams(const D3DXVECTOR3 *eye, const D3DXVECTOR3 *point_at)
   {
      D3DXVECTOR3 up_vector= D3DXVECTOR3(0.0f, 1.0f, 0.0f);
      D3DXMatrixLookAtRH(&m_view_matrix, eye, point_at, &up_vector);
   }

   const D3DXMATRIX *GetWorldMatrix() const
   {
      return &m_world_matrix;
   }

   const D3DXMATRIX *GetViewMatrix() const
   {
      return &m_view_matrix;
   }

   const D3DXMATRIX *GetProjMatrix() const
   {
      return &m_projection_matrix;
   }

private:
   ViewMode m_view_mode;
   long m_drag_start_x, m_drag_start_y;
   D3DXMATRIX m_drag_start_transform;

   float m_near, m_far;
   float m_aspect_ratio;
   float m_field_of_view;

   D3DXMATRIX m_world_matrix;
   D3DXMATRIX m_view_matrix;
   D3DXMATRIX m_projection_matrix;
};

#endif