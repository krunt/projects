#ifndef BOOK_STAND_DEF__
#define BOOK_STAND_DEF__

#include <d3d11.h>
#include "TexturePak.h"

class BookStand
{
public:
   BookStand(int numPages) : 
      m_textureArray(NULL),
      m_shaderResourceViewMap(NULL),
      m_pd3dImmediateContext(NULL),
      m_numPages(numPages),
      m_maxPages(numPages),
      m_offset(0.0f),
      m_maxOffset(0.0f),
      m_maxOffsetLoaded(-1),
      m_imageScratch(NULL)
   {}

   bool Init(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, const std::string &pakName);
   void MoveLeft(float offset);
   void MoveRight(float offset);
   void GetOffset(int pageIndex, float &splitRatio, int &texIndex1, int &texIndex2) const;
   int  GetPages() const { return m_numPages; }
   void Update(void);
   void SubmitShaderResources(void);
   void Shutdown(void);
   
   ~BookStand() { Shutdown(); }

private:
   ID3D11Texture2D * m_textureArray;
   ID3D11ShaderResourceView* m_shaderResourceViewMap;
   ID3D11DeviceContext* m_pd3dImmediateContext;
   int               m_numPages;
   int               m_maxPages;
   float             m_offset;
   float             m_maxOffset;
   int               m_maxOffsetLoaded;
   TexturePak        m_pak;
   unsigned char    *m_imageScratch;
};

#endif // BOOK_STAND_DEF__