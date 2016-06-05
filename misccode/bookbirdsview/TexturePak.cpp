#include "TexturePak.h"

bool TexturePak::Init(const std::string &path)
{
   m_fileHandle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

   if (m_fileHandle == INVALID_HANDLE_VALUE) {
      return false;
   }

   DWORD lowOrderPart, highOrderPart;
   lowOrderPart = GetFileSize(m_fileHandle, &highOrderPart);

   if (highOrderPart != 0) {
      return false;
   }

   if (lowOrderPart == 0 || lowOrderPart % (TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT) != 0) {
      return false;
   }

   m_count = lowOrderPart / (TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT) - 1;

   m_fileMapping = CreateFileMappingA(m_fileHandle, NULL, PAGE_READONLY, 0, 
      0, NULL);

   if (!m_fileMapping) {
      return false;
   }

   m_ptr = (unsigned char*)MapViewOfFile(m_fileMapping, FILE_MAP_READ, 0, 0, lowOrderPart);

   if (!m_ptr) {
      return false;
   }

   return true;
}

void TexturePak::ReadImage(unsigned char *imagePtr, int imageIndex)
{
   int imageSize = TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT;
   memcpy(imagePtr, m_ptr + (imageIndex + 1) * imageSize, imageSize);
}

void TexturePak::ReadScratchImage(unsigned char *imagePtr)
{
   int imageSize = TEXTURE_BOOK_WIDTH * TEXTURE_BOOK_HEIGHT;
   memcpy(imagePtr, m_ptr, imageSize);
}

void TexturePak::Shutdown()
{
   if (!m_ptr)
      return;

   UnmapViewOfFile(m_ptr);
   CloseHandle(m_fileMapping);
   CloseHandle(m_fileHandle);

   return;
}