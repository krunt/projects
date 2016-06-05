
#include "BookStand.h"

#include "TexturePak.h"


bool BookStand::Init(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, const std::string &pakName)
{
   D3D11_TEXTURE2D_DESC textureDesc;
   D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

   if (!m_pak.Init(pakName)) {
      return false;
   }

   m_maxPages = m_pak.GetCount();

   ZeroMemory(&textureDesc, sizeof(textureDesc));
   textureDesc.Width= TEXTURE_BOOK_WIDTH;
   textureDesc.Height= TEXTURE_BOOK_HEIGHT;
   textureDesc.MipLevels= 1;
   textureDesc.ArraySize= m_maxPages + 1;
   //textureDesc.Format= DXGI_FORMAT_R8_UINT; //DXGI_FORMAT_R8_UNORM
   textureDesc.Format= DXGI_FORMAT_R8_UNORM;
   //textureDesc.Format= DXGI_FORMAT_R24G8_TYPELESS;
   textureDesc.SampleDesc.Count= 1;
   textureDesc.SampleDesc.Quality= 0;
   textureDesc.Usage= D3D11_USAGE_DEFAULT;
   textureDesc.BindFlags= D3D11_BIND_SHADER_RESOURCE;
   //textureDesc.BindFlags= D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
   textureDesc.CPUAccessFlags= 0;
   textureDesc.MiscFlags= 0;

   if ( FAILED(pd3dDevice->CreateTexture2D(&textureDesc, NULL, &m_textureArray)) ) {
      return false;
   }

   ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
   shaderResourceViewDesc.Format= textureDesc.Format;
   shaderResourceViewDesc.ViewDimension= D3D_SRV_DIMENSION_TEXTURE2DARRAY;
   shaderResourceViewDesc.Texture2DArray.ArraySize = m_maxPages + 1;
   shaderResourceViewDesc.Texture2DArray.MipLevels = 1;
   shaderResourceViewDesc.Texture2DArray.MostDetailedMip = 0;
   shaderResourceViewDesc.Texture2DArray.FirstArraySlice = 0;

   if ( FAILED(pd3dDevice->CreateShaderResourceView(m_textureArray, 
         &shaderResourceViewDesc, &m_shaderResourceViewMap)) ) {
      return false;
   }

   m_pd3dImmediateContext = pd3dImmediateContext;

   m_imageScratch = (unsigned char *)malloc(TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT);

   m_pak.ReadScratchImage(m_imageScratch);

   m_pd3dImmediateContext->UpdateSubresource(m_textureArray, 0, NULL,
      (const void *)m_imageScratch, TEXTURE_BOOK_WIDTH, TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT);

   return true;
}

void BookStand::MoveLeft(float offset)
{
   m_offset = min(m_offset + offset, (float)m_maxPages-0.01f);
   m_maxOffset = max(m_offset, m_maxOffset);
}

void BookStand::MoveRight(float offset)
{
   m_offset = max(m_offset - offset, 0.0f);
}

void BookStand::Update(void) 
{
   if (m_maxOffsetLoaded < (int)m_maxOffset) {
      for (; m_maxOffsetLoaded < (int)m_maxOffset; ++m_maxOffsetLoaded) {
         m_pak.ReadImage(m_imageScratch, m_maxOffsetLoaded + 1);

         m_pd3dImmediateContext->UpdateSubresource(m_textureArray, m_maxOffsetLoaded + 1 + 1, NULL,
            (const void *)m_imageScratch, TEXTURE_BOOK_WIDTH, TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT);
      }
   }
}

void BookStand::GetOffset(int pageIndex, float &splitRatio, int &texIndex1, int &texIndex2) const {
   int page = (int)m_offset;

   splitRatio = m_offset - float(page);

   if (page < m_numPages && pageIndex >= page) {
      if (pageIndex > page) {
         texIndex1 = texIndex2 = -1;
      } else /*if (pageIndex == page)*/ {
         texIndex1 = 0;
         texIndex2 = -1;
      }
      return;
   }

   if (splitRatio > 0.99f) {
      texIndex1 = page - pageIndex;
      texIndex2 = page - pageIndex;
   } else if (splitRatio < 0.01f) {
      texIndex1 = page - 1 - pageIndex;
      texIndex2 = page - 1 - pageIndex;
   } else {
      texIndex1 = page - pageIndex;
      texIndex2 = page - 1 - pageIndex;
   }
}

void BookStand::SubmitShaderResources(void)
{
   m_pd3dImmediateContext->PSSetShaderResources(0, 1, &m_shaderResourceViewMap);
}

void BookStand::Shutdown(void)
{}