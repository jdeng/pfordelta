#pragma once

class Simple16
{
  static int encode(const uint32_t *in, uint32_t *out, int size);
  static int decode(const uint32_t *in, uint32_t *out, int size);
};

class PForDelta
{
public:
  static constexpr uint32_t kBlockSize = 128;
  static constexpr uint32_t kThreshold = uint32_t(0.1 * kBlockSize);
  static constexpr uint32_t kBits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 20, 32};

  //size of out must be not less than kBlockSize
  static int encode(const uint32_t *in, uint32_t *out);
  static int decode(const uint32_t *in, uint32_t *out);
};

