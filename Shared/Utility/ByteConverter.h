
#ifndef ByteConverter_h__
#define ByteConverter_h__

#ifdef __BIG_ENDIAN__
#define EndianConvert(val) val
#else
#include <byteswap.h>
#include <stdint.h>

// _Generic solo acepta nombres de funciones, no macros, asi que tengo que agregar wrappers. Auch

static inline int16_t bswap16(int16_t x)
{
    return bswap_16(x);
}

static inline uint16_t bswap16u(uint16_t x)
{
    return bswap_16(x);
}

static inline int32_t bswap32(int32_t x)
{
    return bswap_32(x);
}

static inline uint32_t bswap32u(uint32_t x)
{
    return bswap_32(x);
}

static inline float bswap32f(float x)
{
    union
    {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    u.i = bswap32u(u.i);
    return u.f;
}

static inline int64_t bswap64(int64_t x)
{
    return bswap_64(x);
}

static inline uint64_t bswap64u(uint64_t x)
{
    return bswap_64(x);
}

static inline double bswap64f(double x)
{
    union
    {
        double d;
        uint64_t i;
    } u;
    u.d = x;
    u.i = bswap64u(u.i);
    return u.d;
}

static inline char charid(char x)
{
    return x;
}

static inline uint8_t ucharid(uint8_t x)
{
    return x;
}

#define EndianConvert(val) _Generic((val), \
    int16_t: bswap16,                      \
   uint16_t: bswap16u,                     \
    int32_t: bswap32,                      \
   uint32_t: bswap32u,                     \
    int64_t: bswap64,                      \
   uint64_t: bswap64u,                     \
      float: bswap32f,                     \
     double: bswap64f,                     \
       char: charid,                       \
     int8_t: charid,                       \
    uint8_t: ucharid                       \
) (val)
#endif //__BIG_ENDIAN__

#endif //ByteConverter_h__
