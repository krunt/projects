#ifndef TEXTURE_PAK_DEF__
#define TEXTURE_PAK_DEF__

#include <windows.h>
#include <string>

const int TEXTURE_BOOK_WIDTH = 595;
const int TEXTURE_BOOK_HEIGHT = 842;

class TexturePak
{
public:
   TexturePak(void) : m_ptr(NULL), m_count(0) {}

   bool Init(const std::string &path);
   void ReadImage(unsigned char *imagePtr, int imageIndex);
   void ReadScratchImage(unsigned char *imagePtr);
   int  GetCount() const { return m_count; }

   void Shutdown();

   ~TexturePak() { Shutdown(); }

private:
   HANDLE m_fileHandle;
   HANDLE m_fileMapping;
   unsigned char * m_ptr;
   int    m_count;
};

#endif // TEXTURE_PAK_DEF__