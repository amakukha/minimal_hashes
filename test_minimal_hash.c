#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#define NDEBUG
#include "farmhash.h"

#define SMALL   16
#define LARGE   (1 << 24)   // size of 16 MiB
#define SEED    0x12345678

uint8_t buffer[LARGE * SMALL];
uint32_t hashes[LARGE];

typedef uint32_t (*hash_function) (const uint8_t*, size_t);

//--- SuperFastHash -----------------------------------------------------------

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t SuperFastHash (const uint8_t * data, size_t len) {
    // By Paul Hsieh.
    // Source: http://www.azillionmonkeys.com/qed/hash.html
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

//--- Murmur 3 ----------------------------------------------------------------
// Platform-specific functions and macros

#ifdef __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

static FORCE_INLINE uint32_t rotl32 ( uint32_t x, int8_t r )
{
  return (x << r) | (x >> (32 - r));
}

static FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

#define ROTL32(x,y)     rotl32(x,y)
#define ROTL64(x,y)     rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

#define getblock(p, i) (p[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static FORCE_INLINE uint32_t fmix32 ( uint32_t h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//----------

static FORCE_INLINE uint64_t fmix64 ( uint64_t k )
{
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}

//-----------------------------------------------------------------------------

uint32_t MurmurHash3_x86_32 (const uint8_t* data, size_t len) {
  const int nblocks = len / 4;
  int i;

  uint32_t h1 = SEED;

  uint32_t c1 = 0xcc9e2d51;
  uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

  for(i = -nblocks; i; i++)
  {
    uint32_t k1 = getblock(blocks,i);

    k1 *= c1;
    k1 = ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

  uint32_t k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  h1 = fmix32(h1);

  return h1;
}

//-----------------------------------------------------------------------------

FORCE_INLINE uint64_t getblock64(const uint64_t * p, int i) {
        return p[i];
}

void MurmurHash3_x64_128(const uint8_t * data, const int len, void * out) {
        const int nblocks = len / 16;

        uint64_t h1 = SEED;
        uint64_t h2 = SEED;

        const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
        const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

        //----------
        // body

        const uint64_t * blocks = (const uint64_t *)(data);

        for (int i = 0; i < nblocks; i++)
        {
                uint64_t k1 = getblock64(blocks, i * 2 + 0);
                uint64_t k2 = getblock64(blocks, i * 2 + 1);

                k1 *= c1; k1 = ROTL64(k1, 31); k1 *= c2; h1 ^= k1;

                h1 = ROTL64(h1, 27); h1 += h2; h1 = h1 * 5 + 0x52dce729;

                k2 *= c2; k2 = ROTL64(k2, 33); k2 *= c1; h2 ^= k2;

                h2 = ROTL64(h2, 31); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
        }

        //----------
        // tail

        const uint8_t * tail = (const uint8_t*)(data + nblocks * 16);

        uint64_t k1 = 0;
        uint64_t k2 = 0;

        switch (len & 15)
        {
        case 15: k2 ^= ((uint64_t)tail[14]) << 48;
        case 14: k2 ^= ((uint64_t)tail[13]) << 40;
        case 13: k2 ^= ((uint64_t)tail[12]) << 32;
        case 12: k2 ^= ((uint64_t)tail[11]) << 24;
        case 11: k2 ^= ((uint64_t)tail[10]) << 16;
        case 10: k2 ^= ((uint64_t)tail[9]) << 8;
        case  9: k2 ^= ((uint64_t)tail[8]) << 0;
                k2 *= c2; k2 = ROTL64(k2, 33); k2 *= c1; h2 ^= k2;

        case  8: k1 ^= ((uint64_t)tail[7]) << 56;
        case  7: k1 ^= ((uint64_t)tail[6]) << 48;
        case  6: k1 ^= ((uint64_t)tail[5]) << 40;
        case  5: k1 ^= ((uint64_t)tail[4]) << 32;
        case  4: k1 ^= ((uint64_t)tail[3]) << 24;
        case  3: k1 ^= ((uint64_t)tail[2]) << 16;
        case  2: k1 ^= ((uint64_t)tail[1]) << 8;
        case  1: k1 ^= ((uint64_t)tail[0]) << 0;
                k1 *= c1; k1 = ROTL64(k1, 31); k1 *= c2; h1 ^= k1;
        };

        //----------
        // finalization

        h1 ^= len; h2 ^= len;

        h1 += h2;
        h2 += h1;

        h1 = fmix64(h1);
        h2 = fmix64(h2);

        h1 += h2;
        h2 += h1;

        ((uint64_t*)out)[0] = h1;
        ((uint64_t*)out)[1] = h2;
}

uint64_t MurmurHash3_64(const uint8_t* data, size_t len) {
    // Reference: https://github.com/neosmart/mmhash3/blob/master/MurmurHash3.h
    uint64_t temp[2];
    MurmurHash3_x64_128(data, len, &temp);
    return temp[0];
}

uint32_t MurmurHash3_x64_32(const uint8_t* data, size_t len) {
    return (uint32_t) MurmurHash3_64(data, len);
}

//-----------------------------------------------------------------------------
// --end of Murmur3


// Table-driven CRC32
// Source: python3 pycrc.py --model crc-32 --algorithm tbl --generate c -o file.c
// Reference: https://pycrc.org/tutorial.html
/**
 * Static table used for the table_driven implementation.
 */
static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32(const uint8_t *buf, size_t size) {
    uint32_t crc = ~0U;
    while (size--)
        crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    return crc ^ ~0U;
}

//-----------------------------------------------------------------------------
// --end of Table-driven CRC32


uint32_t DJB2_hash(const uint8_t* buf, size_t size) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + buf[i]; /* hash * 33 + byte */
    return hash;
}

uint32_t SDBM_hash(const uint8_t* buf, size_t size) {
    uint32_t hash = 0;
    for (size_t i = 0; i < size; i++)
        hash = (hash << 6) + (hash << 16) - hash + buf[i]; /* hash * 65599 + byte */
    return hash;
}

uint32_t dbm_hash(str, len)
register char *str;
register int len;
{
    // Optimized version of SDBM hash.
    // Uses "Duff's device": https://en.wikipedia.org/wiki/Duff%27s_device
    // Source: https://github.com/davidar/sdbm/blob/29d5ed2b5297e51125ee45f6efc5541851aab0fb/hash.c#L18-L47
    register unsigned long n = 0;

#define HASHC   n = *str++ + 65599 * n

    if (len > 0) {
        register int loop = (len + 8 - 1) >> 3;
        switch(len & (8 - 1)) {
            case 0: do {
                    HASHC;  case 7: HASHC;
            case 6: HASHC;  case 5: HASHC;
            case 4: HASHC;  case 3: HASHC;
            case 2: HASHC;  case 1: HASHC;
                    } while (--loop);
            }
    }
    return n;
}

uint32_t FNV1a(const uint8_t* data, size_t size) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }
    return h;
}

uint64_t FNV1a_64(const uint8_t* data, size_t size) {
    uint64_t h = 0xcbf29ce484222325UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 0x00000100000001B3UL;
    }
    return h;
}

uint32_t FNV1a_64_32(const uint8_t* data, size_t size) {
    // Imrovised conversion of 64-bit FNV1a hash into 32-bit hash.
    uint64_t h = FNV1a_64(data, size);
    return h ^ (h >> 32);
}

uint32_t MurmurOAAT_32(const uint8_t* data, size_t size) {
    // One-byte-at-a-time hash based on Murmur's mix
    uint32_t h = SEED;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

uint32_t KR_v2_hash(const uint8_t* data, size_t size) {
    // a.k.a. Java String hashCode(), a.k.a. "BKDR Hash Function"
    uint32_t hashval = 0;
    for (size_t i = 0; i < size; i++)
        hashval = 31*hashval + data[i];
    return hashval;
}

uint32_t Jenkins_one_at_a_time_hash(const uint8_t* data, size_t size) {
    // a.k.a. simply "One-at-a-Time Hash"
    uint32_t hash = 0;
    for (size_t i = 0; i < size; i++) {
        hash += data[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

#define POLY_CRC32C 0x82f63b78
#define POLY_CRC32  0xedb88320
uint32_t CRC32b(const uint8_t *data, size_t size) {
    unsigned int crc = 0xFFFFFFFF, mask;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 7; j >= 0; j--) {
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (POLY_CRC32 & mask);
        }
    }
    return ~crc;
}

uint32_t crc32c(const uint8_t *buf, size_t len) {
    // Source: https://stackoverflow.com/a/27950866/5407270
    // Note: CRC32b implementation is generally faster (it avoids branching)
    uint32_t crc = 0xFFFFFFFF;
    while (len--) {
        crc ^= *buf++;
        for (int k = 7; k >= 0; k--)
            crc = crc & 1 ? (crc >> 1) ^ POLY_CRC32C : crc >> 1;
    }
    return ~crc;
}

inline uint32_t _rotl32(uint32_t x, int32_t bits) {
    return x<<bits | x>>(32-bits);      // C idiom: will be optimized to a single CPU operation
}

uint32_t XOR_rotate(const uint8_t* data, size_t size) { 
    // Source: https://stackoverflow.com/a/7666668/5407270
    uint32_t result = 0x55555555;
    for (size_t i = 0; i < size; i++) {
        result ^= data[i];
        result = _rotl32(result, 5);
    }
    return result;
}

uint32_t x17_hash(const uint8_t* data, size_t size) {
    uint32_t h = SEED;
    for (size_t i = 0; i < size; i++)
        h = 17 * h + (data[i] - ' ');
    return h ^ (h >> 16);
}

uint32_t Adler32_slow(const uint8_t* data, size_t len) {
    // This is a slow implementation of Adler32, but it doesn't matter because
    // it's a VERY BAD hash function anyway. Don't use it!
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

uint32_t RSHash(const uint8_t* data, size_t size) {
    // Introduced by Robert Sedgewick in his "Algorithms in C" (1990) book.
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    // RSHash seems to outperforms JSHash, PJWHash (a.k.a. ELFHash) and APHash from the same source in all respects.
    uint32_t a = 63689, b = 378551;
    uint32_t hash = 0;
    for (uint32_t i = 0; i < size; i++) {
       hash = hash * a + data[i];
       a *= b;
    }
    return hash;
}

uint32_t JSHash(const uint8_t* data, size_t size) {
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    uint32_t hash = 1315423911;
    for (size_t i = 0; i < size; i++)
       hash ^= ((hash << 5) + data[i] + (hash >> 2));
    return hash;
}

uint32_t PJWHash(const uint8_t* str, size_t length) {
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    // ELFHash is the same, but performs slightly faster
    const unsigned int BitsInUnsignedInt = (unsigned int)(sizeof(unsigned int) * 8);
    const unsigned int ThreeQuarters     = (unsigned int)((BitsInUnsignedInt  * 3) / 4);
    const unsigned int OneEighth         = (unsigned int)(BitsInUnsignedInt / 8);
    const unsigned int HighBits          = (unsigned int)(0xFFFFFFFF) << (BitsInUnsignedInt - OneEighth);

    unsigned int hash = 0;
    unsigned int test = 0;
    for (size_t i = 0; i < length; i++) {
       hash = (hash << OneEighth) + str[i];
       if ((test = hash & HighBits) != 0)
          hash = (( hash ^ (test >> ThreeQuarters)) & (~HighBits));
    }

    return hash;
}

uint32_t ELFHash(const uint8_t* str, size_t length) {
    // Faster version of PJWHash.
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    unsigned int hash = 0;
    unsigned int x    = 0;
    for (size_t i = 0; i < length; i++) {
       hash = (hash << 4) + str[i];
       if ((x = hash & 0xF0000000L) != 0)
          hash ^= (x >> 24);
       hash &= ~x;
    }
    return hash;
}

uint32_t DEKHash(const uint8_t* str, size_t length) {
    // Introduced by Donald E. Knuth in "The Art Of Computer Programming", Volume 3.
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    unsigned int hash = length;
    for (size_t i = 0; i < length; i++)
       hash = ((hash << 5) ^ (hash >> 27)) ^ str[i];
    return hash;
}

uint32_t APHash(const uint8_t* str, size_t length) {
    // By Arash Partow.
    // Source: http://www.partow.net/programming/hashfunctions/index.html
    uint32_t hash = 0xAAAAAAAA;
    for (size_t i = 0; i < length; i++) {
       hash ^= ((i & 1) == 0) ? (  (hash <<  7) ^ str[i] * (hash >> 3)) :
                                (~((hash << 11) + (str[i] ^ (hash >> 5))));
    }
    return hash;
}

uint32_t Fletcher32(const uint8_t *data_, size_t len) {
    // Note: when `len` (size in bytes) is odd, this function will read past the last byte.
    // However, for C-strings, the next byte will be zero, which is sufficient for this test.
    // In other cases, you have to do zero-padding.
    const uint16_t* data = (const uint16_t*) data_;
    uint32_t c0, c1;
    len = (len + 1) & ~1;      /* Round up len to words */

    /* We similarly solve for n > 0 and n * (n+1) / 2 * (2^16-1) < (2^32-1) here. */
    /* On modern computers, using a 64-bit c0/c1 could allow a group size of 23726746. */
    for (c0 = c1 = 0; len > 0; ) {
        size_t blocklen = len;
        if (blocklen > 360*2) {
            blocklen = 360*2;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while ((blocklen -= 2));
        c0 = c0 % 65535;
        c1 = c1 % 65535;
    }
    return (c1 << 16 | c0);
}

uint32_t Fletcher32_wrap(const uint8_t* data, size_t size) {
    if (size & 1) {
        // Add zero-padding for odd sizes. (This is, actually, unnecessary for
        // C-strings, because they are terminated by a nul-byte by definition.)
        uint8_t* buf = malloc(size + 1);
        memcpy(buf, data, size);
        buf[size] = 0;
        uint32_t res = Fletcher32(buf, size + 1);
        free(buf);
        return res;
    }
    return Fletcher32(data, size);
}

uint32_t farmhash_fingerprint64_wrap(const uint8_t* data, size_t size) {
    // Calculates 64-bit version of FarmHash Fingerprint and takes the lower 32 bits
    return (uint32_t)farmhash_fingerprint64((const char *)data, size);
}

struct timeval stop, start;
void timing_start() {
    gettimeofday(&start, NULL);
}

void timing_end() {
    gettimeofday(&stop, NULL);
    printf("took %lu us, ", (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);
}

#define ALGO_COUNT 18
hash_function get_hash_function(int algo) {
    hash_function res;
    switch (algo) {
        case 1:  printf("FNV1a                 "); res = FNV1a; break;
        case 2:  printf("MurmurOAAT            "); res = MurmurOAAT_32; break;
        case 3:  printf("RSHash                "); res = RSHash; break;
        case 4:  printf("SDBM                  "); res = SDBM_hash; break;
        case 5:  printf("Jenkins OAAT          "); res = Jenkins_one_at_a_time_hash; break;
        case 6:  printf("K&R v2                "); res = KR_v2_hash; break;
        case 7:  printf("SuperFastHash         "); res = SuperFastHash; break;
        case 8:  printf("DJB2                  "); res = DJB2_hash; break;
        case 9:  printf("XOR_rotate            "); res = XOR_rotate; break;
        case 10: printf("DEKHash               "); res = DEKHash; break;
        case 11: printf("Fletcher32            "); res = Fletcher32_wrap; break;
        case 12: printf("Adler32               "); res = Adler32_slow; break;
        case 13: printf("MurmurHash3_x86_32    "); res = MurmurHash3_x86_32; break;
        case 14: printf("MurmurHash3_x64_32    "); res = MurmurHash3_x64_32; break;
        case 15: printf("farmhash_fingerprint32"); res = farmhash_fingerprint32; break;
        case 16: printf("farmhash_fingerprint64"); res = farmhash_fingerprint64_wrap; break;
        case 17: printf("CRC32 (bit-by-bit)    "); res = CRC32b; break;
        case 18: printf("CRC32 (table-driven)  "); res = crc32; break;

        // These are hidden, because they perform worse than the functions above.
        case 94: printf("FNV1a_64_32           "); res = FNV1a_64_32; break;
        case 95: printf("APHash                "); res = APHash; break;
        case 96: printf("JSHash                "); res = JSHash; break;
        case 97: printf("ELFHash               "); res = ELFHash; break;
        case 98: printf("PJWHash               "); res = PJWHash; break;

        case 100:printf("x17                   "); res = x17_hash; break;
        case 101:printf("CRC32c (bit-by-bit)   "); res = crc32c; break;

        default: printf("ERROR: unknown algo %d\n", algo); exit(-1);
    }
    printf("\t");
    return res;
}

void fill_random(uint32_t* buffer, size_t count) {
    for (size_t i = 0; i < count; i++)
        buffer[i] = random();
}

void show_sequence() {
    printf("Generated random sequence:");
    for (int i = 0; i < SMALL; i++) {
        printf(" %02x", buffer[i]);
    }
    printf("\n");
}

void count_collisions(const uint32_t* hashes, size_t count) {
    size_t slots = count * 32;
    uint32_t* cnt_table = calloc(slots, sizeof(uint32_t));
    size_t collisions = 0;
    for (size_t i = 0; i < count; i++) {
        size_t j = hashes[i] % slots;
        while (cnt_table[j] != 0 && cnt_table[j] != hashes[i]) {
            j = (j + 1) % slots;
        }
        if (cnt_table[j] == 0) {
            cnt_table[j] = hashes[i];
        } else {
            collisions++;
        }
    }
    free(cnt_table);
    double share = 100.0*(double)collisions/count;
    printf("%lu collisions out of %lu, %0.2f\n", collisions, count, share);
}

void evaluate_avalanching(const uint32_t* hashes, size_t count) {
    // Looks at hashes of consecutive sorted strings and assesses their
    // similarity score. The lower the total similarity, the worse avalanching.
    uint32_t score = 0;
    char last[9], current[9];
    for (size_t i = 0; i < count; i++) {
        sprintf(current, "%08X", hashes[i]);
        if (i) {
            for (int j = 0; j < 8; j++)
                if (last[j] == current[j]) score++;
        }
        strcpy(last, current);
    }
    float similarity = 100.*score/count/8;
    float avalanching = similarity <= 6.25 ? 100 : (100 - similarity) / (100 - 6.25) * 100;
    printf("score: %u, similarity: %.2f, avalanching: %.1f\n", score, similarity, avalanching);
}

void calc_buffer_hashes(char* type, const uint8_t* buffer, size_t length, size_t count) {
    printf("Calculating hashes of %s buffers...\n", type);
    for (int algo = 1; algo <= ALGO_COUNT; algo++) {
        hash_function func = get_hash_function(algo);
        timing_start();
        for (size_t i = 0; i < count; i++) {
            hashes[i] = func(buffer + length*i, length);
        }
        timing_end();
        count_collisions(hashes, count);
    }
}

void calc_word_hashes(const char* fname, char* buffer) {
#define MAX_WORD_LENGTH 50
    printf("Calculating hashes of English words... (please, provide name of the file with words as first argument)\n");

    char line[MAX_WORD_LENGTH + 1];
    size_t word_count = 0;

    // Open file
    FILE* f = fopen(fname, "r");

    // Read file line by line
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';   // strip newline
        strcpy(buffer + word_count*MAX_WORD_LENGTH, line);
        word_count++;
    }
    fclose(f);

    // Calculate hashes
    for (int algo = 1; algo <= ALGO_COUNT; algo++) {
        hash_function func = get_hash_function(algo);
        timing_start();
        for (size_t i = 0; i < word_count; i++) {
            char* word = buffer + MAX_WORD_LENGTH*i;
            size_t len = strlen(word);
            hashes[i] = func((uint8_t*)word, len);
        }
        timing_end();
        count_collisions(hashes, word_count);
        evaluate_avalanching(hashes, word_count);
    }
}

void show_known_hashes() {
    const char* phrase = "Hello, world!";
    for (int algo = 1; algo <= ALGO_COUNT; algo++) {
        hash_function func = get_hash_function(algo);
        uint32_t hash = func((const uint8_t*)phrase, strlen(phrase));
        printf("%08x\n", hash);
    }
}

int main(int argc, char* argv[]) {
    show_known_hashes(); // TODO: check
    srandom(SEED);
    fill_random((uint32_t*) buffer, LARGE * SMALL / sizeof(uint32_t));
    show_sequence();
    calc_buffer_hashes("small", buffer, SMALL, LARGE);
    calc_buffer_hashes("large", buffer, LARGE, SMALL);
    calc_word_hashes(argv[1], (char*)buffer);
    return 0;
}
