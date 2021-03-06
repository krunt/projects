#ifndef TYPES_DEF_
#define TYPES_DEF_

#include <boost/cstdint.hpp>

namespace btorrent {
    typedef boost::int64_t size_type;
    typedef boost::uint64_t unsigned_size_type;

    typedef boost::int8_t i8;
    typedef boost::uint8_t u8;
    typedef boost::int16_t i16;
    typedef boost::uint16_t u16;
    typedef boost::int32_t i32;
    typedef boost::uint32_t u32;
    typedef boost::int64_t i64;
    typedef boost::uint64_t u64;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define int2get(T,A)       do { T = (u16)((u8*)A)[0] << 8  \
                            | (u16)((u8*)A)[1]; } while (0)
#define int4get(T,A)       do { T = (u32)((u8*)A)[0] << 24 | (u32)((u8*)A)[1] << 16 \
                                | (u32)((u8*)A)[2] << 8 | (u32)((u8*)A)[3]; } while (0)
#define int8get(T,A)       do { T = (u64)((u8*)A)[0] << 56 | (u64)((u8*)A)[1] << 48 \
                                | (u64)((u8*)A)[2] << 40 | (u64)((u8*)A)[3] << 32 \
                                | (u64)((u8*)A)[4] << 24 | (u64)((u8*)A)[5] << 16 \
                                | (u64)((u8*)A)[6] << 8 \
                                | (u64)((u8*)A)[7] << 0; } while (0)

#define int2store(T,A)       do { *((u8 *)(T))=(u8) ((A>>8));\
                                  *(((u8 *)(T))+1)=(u8) (((A))); } while(0)
#define int4store(T,A)       do { *((u8 *)(T))=(u8) ((A>>24));\
                                  *(((u8 *)(T))+1)=(u8) (((A) >> 16));\
                                  *(((u8 *)(T))+2)=(u8) (((A) >> 8));\
                                  *(((u8 *)(T))+3)=(u8) (((A))); } while(0)
#define int8store(T,A)       do { *((u8 *)(T))=(u8) ((A>>56));\
                                  *(((u8 *)(T))+1)=(u8) (((A) >> 48));\
                                  *(((u8 *)(T))+2)=(u8) (((A) >> 40));\
                                  *(((u8 *)(T))+3)=(u8) (((A) >> 32));\
                                  *(((u8 *)(T))+4)=(u8) (((A) >> 24));\
                                  *(((u8 *)(T))+5)=(u8) (((A) >> 16));\
                                  *(((u8 *)(T))+6)=(u8) (((A) >> 8));\
                                  *(((u8 *)(T))+7)=(u8) (((A))); } while(0)

#else

#define int2get(T,A)       (T)=*((u16*)(A))
#define int4get(T,A)       (T)=*((u32*)(A))
#define int8get(T,A)       (T)=*((u64*)(A))

#define int2store(T,A) *((u16*)T)=(A)
#define int4store(T,A) *((u32*)T)=(A)
#define int8store(T,A) *((u64*)T)=(A)

#endif

#ifndef be16toh

#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define htobe16(x) __bswap_16 (x)
#    define be16toh(x) __bswap_16 (x)
#  else
#    define be16toh(x) (x)
#    define htobe16(x) (x)
#  endif

#endif

#ifndef be32toh

#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define htobe32(x) __bswap_32 (x)
#    define be32toh(x) __bswap_32 (x)
#  else
#    define be32toh(x) (x)
#    define htobe32(x) (x)
#  endif

#endif

#ifndef be64toh

#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define htobe64(x) __bswap_64 (x)
#    define be64toh(x) __bswap_64 (x)
#  else
#    define be64toh(x) (x)
#    define htobe64(x) (x)
#  endif

#endif

#endif //TYPES_DEF_
