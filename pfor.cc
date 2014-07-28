// PforDelta / S16 in C++ 11
// Copyright (C) 2014 Jack Deng <jackdeng@gmail.com>
// MIT LICENSE
//
// partially based on https://code.google.com/p/poly-ir-toolkit/source/browse/#svn%2Fbranches%2Fwei%2Fsrc%2Fcompression_toolkit
// rewritten with templates

#include <functional>
#include <algorithm>

#include "pfor.h"

namespace {

//pack with the same size (32 items)
template <uint32_t Bits, uint32_t Iter = 32> struct pack 
{
  static const uint32_t Mask = (1 << Bits) - 1;
  static const uint32_t Pos = 32 - Iter;
  static const uint32_t Start = (Pos * Bits) / 32, End = ((Pos + 1) * Bits - 1) / 32;
  static const uint32_t Offset = 32 - ((Pos + 1) * Bits - End * 32);

  inline static void f(const uint32_t *p, uint32_t *w) {
    if (End == Start) {
      w[Start] |= (p[Pos] << Offset);
    }
    else {
      w[Start] |= (p[Pos] >> (32 - Offset));
      w[End] = (p[Pos] << Offset);
    }
    pack<Bits, Iter-1>::f(p, w);    
  }

  inline static void g(const uint32_t *p, uint32_t *w) {
    if (End == Start) {
      w[Pos] = (p[Start] >> Offset) & Mask;
    }
    else {
      w[Pos] = (p[Start] << (32 - Offset)) & Mask;
      w[Pos] |= (p[End] >> Offset) & Mask;
    }
    pack<Bits, Iter-1>::g(p, w); 
  }
};

template <uint32_t Bits> struct pack<Bits, 0> { 
  static void f(const uint32_t *, uint32_t *) {} 
  static void g(const uint32_t *, uint32_t *) {} 
};
template <uint32_t Iter> struct pack<0, Iter> { 
  static void f(const uint32_t *, uint32_t *) {} 
  static void g(const uint32_t *, uint32_t *) {} 
};
template <uint32_t Iter> struct pack<32, Iter> { 
  static void f(const uint32_t *p, uint32_t *w) { std::copy(p, p+32, w); } 
  static void g(const uint32_t *p, uint32_t *w) { std::copy(p, p+32, w); } 
};

// S16 template
/*
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,0,0,0,0,0,0,0},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {4,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {3,4,4,4,4,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {5,5,5,5,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {4,4,5,5,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {6,6,6,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {5,5,6,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {7,7,7,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {10,9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {14,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} 
*/

template <uint32_t TotalBits, uint32_t Bits, uint32_t... Rest>
struct s16pack_ {
  static const uint32_t UsedBits = 28 - TotalBits;
  static const uint32_t RemainBits = TotalBits - Bits;
  static int f(const uint32_t *p, uint32_t& w, int left, int count) {
    if (left == 0) 
      return count;
    if (*p >= (1 << Bits)) 
      return -1;
//    w += (*p << RemainBits);
    w += (*p << UsedBits);
    return s16pack_<RemainBits, Rest...>::f(p + 1, w, left - 1, count + 1);
  }

  static int g(uint32_t p, uint32_t *w, int count) {
    *w = (p >> UsedBits) & ((1 << Bits) - 1);
    return s16pack_<RemainBits, Rest...>::g(p, w + 1, count + 1);
  }
};

template <uint32_t Bits>
struct s16pack_<Bits, Bits> {
  static const uint32_t UsedBits = 28 - Bits;
  static int f(const uint32_t *p, uint32_t& w, int left, int count) {
    if (left == 0)
      return count;
    if (*p >= (1 << Bits))
      return -1;
    w += (*p << UsedBits);
    return count + 1;
  }

  static int g(uint32_t p, uint32_t *w, int count) { 
  *w = (p >> UsedBits) & ((1 <<Bits) - 1);
  return count + 1; 
  }
};

template <uint32_t... Is> struct s16pack: public s16pack_<28, Is...>{};
}

int Simple16::encode(const uint32_t *in, uint32_t *out, int size)
{
  static const std::function<int(const uint32_t *, uint32_t&, int, int)> kPackers[] = {
    s16pack<1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1>::f,
    s16pack<2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1>::f,
    s16pack<1,1,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,1,1>::f,
    s16pack<1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2>::f,
    s16pack<2,2,2,2,2,2,2,2,2,2,2,2,2,2>::f,
    s16pack<4,3,3,3,3,3,3,3,3>::f,
    s16pack<3,4,4,4,4,3,3,3>::f,
    s16pack<4,4,4,4,4,4,4>::f,
    s16pack<5,5,5,5,4,4>::f,
    s16pack<4,4,5,5,5,5>::f,
    s16pack<6,6,6,5,5>::f,
    s16pack<5,5,6,6,6>::f,
    s16pack<7,7,7,7>::f,
    s16pack<10,9,9>::f,
    s16pack<14,14>::f,
    s16pack<28>::f 
  };

  int left = size;
  auto p = in;
  auto w = out;
  while (left > 0) {
    int m = -1;
    for (size_t i=0; i<16; ++i) {
      auto& packer = kPackers[i];
      *w = i << 28;
      int ret = packer(p, *w, left, 0);
      if (ret > 0) {
        m = ret;
        break;
      }
    }
    if (m < 0) return -1;

    left -= m;
    p += m;
    w ++;
  }

  return w - out;
}

int Simple16::decode(const uint32_t *in, uint32_t *out, int size)
{
  static const std::function<int(uint32_t, uint32_t *, int)> kUnpackers[] = {
    s16pack<1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1>::g,
    s16pack<2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1>::g,
    s16pack<1,1,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,1,1>::g,
    s16pack<1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2>::g,
    s16pack<2,2,2,2,2,2,2,2,2,2,2,2,2,2>::g,
    s16pack<4,3,3,3,3,3,3,3,3>::g,
    s16pack<3,4,4,4,4,3,3,3>::g,
    s16pack<4,4,4,4,4,4,4>::g,
    s16pack<5,5,5,5,4,4>::g,
    s16pack<4,4,5,5,5,5>::g,
    s16pack<6,6,6,5,5>::g,
    s16pack<5,5,6,6,6>::g,
    s16pack<7,7,7,7>::g,
    s16pack<10,9,9>::g,
    s16pack<14,14>::g,
    s16pack<28>::g 
  };

  int left = size;
  auto p = in;
  auto w = out;
  while (left > 0) {
    int k = (*p) >> 28;
    auto& unpacker = kUnpackers[k];
    int m = unpacker(*p, w, 0);
    left -= m;
    w += m;
    p ++;
  }

  return p - in;
}

constexpr uint32_t PForDelta::kBits[];

int PForDelta::encode(const uint32_t *in, uint32_t *out)
{
    static const std::function<void(const uint32_t *, uint32_t *)> kPackers[] = {
      pack<0>::f, pack<1>::f, pack<2>::f, pack<3>::f,
      pack<4>::f, pack<5>::f, pack<6>::f, pack<7>::f,
      pack<8>::f, pack<9>::f, pack<10>::f, pack<11>::f,
      pack<12>::f, pack<13>::f, pack<16>::f, pack<20>::f,
      pack<32>::f
    };

    auto packn = [] (const uint32_t *p, uint32_t *w, int bits, int n) {
      if (bits == 32) {
        std::copy(p, p+n, w);
        return;
      }
      for (int i=0; i<n; ++i) {
        const uint32_t start = i * bits / 32, end = ((i+1) * bits - 1) / 32;
        const uint32_t offset = 32 - ((i+1) * bits - end * 32);
        if (end == start) {
          w[start] |= (p[i] << offset);
       }
        else {
          w[start] |= (p[i] >> (32 - offset));
          w[end] = (p[i] << offset);
        }
      }
    };

    auto m = *std::max_element(in, in + kBlockSize);
    int t = m < 256? 0: (m < 65536? 1: 2);
    int ebits = m < 256? 8: (m < 65536? 16: 32);
    
    for (uint32_t bits_idx = 0; bits_idx < sizeof(kBits)/sizeof(kBits[0]) - 2; ++bits_idx) {
      uint32_t bits = kBits[bits_idx+2];
      int start = 0, last = -1, n = 0; 

      if (bits == 32) {
        std::copy(in, in+kBlockSize, out + 1);
        *out = (bits_idx << 12) + (2 << 10) + kBlockSize;
        return kBlockSize + 1;
      }

      uint32_t tmp[kBlockSize], ex[kBlockSize];  
      for (int i=0; i<kBlockSize; ++i) {
        if (in[i] >= (1 << bits) || ((last >= 0) && (i - last == (1 << bits)))) {
          if (last < 0) 
            start = i;
          else 
            tmp[last] = i - 1 - last;
          last = i;
          ex[n++] = in[i];
        }
        else {
          tmp[i] = in[i];
        }
      }

      if (last >= 0)
        tmp[last] = (1 << bits) - 1;
      else
        start = kBlockSize;

      if (n <= kThreshold) {
        int size = (bits * kBlockSize) / 32;

        auto w = out + 1;
        std::fill(w, w+size, 0);
        auto& packer = kPackers[bits_idx + 2];
        for (auto p = tmp; p < tmp + kBlockSize; p += 32, w += bits) 
          packer(p, w);

        size = (ebits * n + 31) / 32;
        std::fill(w, w+size, 0);
        packn(ex, w, ebits, n);
        w += size;

        *out = (bits_idx << 12) + (t << 10) + start;
        return w - out;
      }
    }

    return 0;
}

int PForDelta::decode(const uint32_t *in, uint32_t *out) 
{
    static const std::function<void(const uint32_t *, uint32_t *)> kUnpackers[] = {
      pack<0>::g, pack<1>::g, pack<2>::g, pack<3>::g,
      pack<4>::g, pack<5>::g, pack<6>::g, pack<7>::g,
      pack<8>::g, pack<9>::g, pack<10>::g, pack<11>::g,
      pack<12>::g, pack<13>::g, pack<16>::g, pack<20>::g,
      pack<32>::g
    };

    uint32_t x = *in;
    int bits_idx = ((x >> 12) & 15) + 2;
    int bits = kBits[bits_idx]; 
    int t = (x >> 12) & 3;
    int start = x & ((1 << 10) - 1);

    auto p = in + 1;
    auto w = out;
    auto& unpacker = kUnpackers[bits_idx];
    int size = (bits * kBlockSize) / 32;
    for (; p < in + 1 + size; p += bits, w += 32) 
      unpacker(p, w);

    int i = 0;
    switch(t) {
    case 0:
      for (; start < kBlockSize; ++i) {
        int x = out[start] + 1;
        out[start] = (p[i >> 2] >> (24 - ((i & 3) << 3))) & 255;
        start += x;
      } 
      p += (i+3)/4;
      break;
    case 1:
      for (; start < kBlockSize; ++i) {
        int x = out[start] + 1;
        out[start] = (p[i >> 1] >> (16 - ((i & 1) << 4))) & 65535;
        start += x;
      } 
      p += (i+1)/2;
      break;
 
    case 2:
       for (; start < kBlockSize; ++i) {
        int x = out[start] + 1;
        out[start] = p[i];
        start += x;
      }
      p += i;
      break;
    }

    return p - in;
}

#if defined(PFOR_TEST)

#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// g++  -Ofast -std=c++11 -DPFOR_TEST -o pfor pfor.cc
int main(int, const char *[])
{
  std::vector<uint32_t> x(128), y(128 + 128), z(129);
  for (int i=0; i<x.size(); ++i) x[i] = rand() & 0xfff;
  x[10] = x[50] = x[100] = 1000000;

  const size_t N = 1000 * 1000;
  int n = 0;
  time_t t0 = time(NULL);
  for (size_t i = 0; i<N; ++i) 
    n = Simple16::encode(x.data(), y.data(), x.size());
  time_t t1 = time(NULL);
  printf("s16 encode time: %ld, ints used %d\n", t1 - t0, n);


  for (size_t i = 0; i<N; ++i) 
    n = Simple16::decode(y.data(), z.data(), x.size());
  time_t t2 = time(NULL);
  printf("s16 decode time: %ld, ints used %d\n", t2 - t1, n);

  for (int i=0; i<x.size(); ++i) {
    if (x[i] != z[i]) 
    printf("s16 test failed: i=%d x=%u z=%u\n", i, x[i], z[i]);
  }

  for (size_t i = 0; i<N; ++i) 
    n = PForDelta::encode(x.data(), y.data());
  time_t t3 = time(NULL);
  printf("pfor encode time: %ld, ints used %d\n", t3 - t2, n);

  for (size_t i = 0; i<N; ++i) 
    n = PForDelta::decode(y.data(), z.data());
  time_t t4 = time(NULL);
  printf("pfor decode time: %ld, ints used %d\n", t4 - t3, n);

  for (int i=0; i<x.size(); ++i) {
    if (x[i] != z[i]) 
    printf("pfor test failed: i=%d x=%u z=%u\n", i, x[i], z[i]);
  }

  return 0;
}

#endif

