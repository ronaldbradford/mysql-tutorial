#ifndef UUIDV_UUID_GEN_H
#define UUIDV_UUID_GEN_H

/*
  Self-contained UUID generation supporting versions 1, 3, 4, 5, 6, 7.
  Versions 3 and 5 require a name/namespace and are not generatable from a
  bare version number, so uuidv(3) and uuidv(5) are rejected at init time.
  Output buffer must be at least 37 bytes (36 chars + NUL).
*/

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include <mutex>

namespace uuidv_gen {

inline std::mt19937_64 &rng() {
  static thread_local std::mt19937_64 engine{[] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(),
                      static_cast<unsigned>(time(nullptr))};
    return std::mt19937_64(seq);
  }()};
  return engine;
}

inline void random_bytes(unsigned char *out, size_t n) {
  auto &e = rng();
  size_t i = 0;
  while (i < n) {
    uint64_t v = e();
    size_t take = (n - i < 8) ? (n - i) : 8;
    memcpy(out + i, &v, take);
    i += take;
  }
}

inline void format(const unsigned char b[16], char *out) {
  snprintf(out, 37,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
           b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

/* 100-ns intervals since 1582-10-15 (Gregorian UUID epoch). */
inline uint64_t gregorian_ticks() {
  using namespace std::chrono;
  uint64_t unix_100ns =
      duration_cast<duration<uint64_t, std::ratio<1, 10000000>>>(
          system_clock::now().time_since_epoch())
          .count();
  return unix_100ns + 0x01B21DD213814000ULL;
}

/* Returns true on success. version must be one of 1,4,6,7. */
inline bool generate(int version, char *out) {
  unsigned char b[16];

  switch (version) {
    case 1: {
      uint64_t ts = gregorian_ticks();
      uint32_t time_low = static_cast<uint32_t>(ts & 0xFFFFFFFFULL);
      uint16_t time_mid = static_cast<uint16_t>((ts >> 32) & 0xFFFF);
      uint16_t time_hi = static_cast<uint16_t>((ts >> 48) & 0x0FFF);

      b[0] = static_cast<unsigned char>(time_low >> 24);
      b[1] = static_cast<unsigned char>(time_low >> 16);
      b[2] = static_cast<unsigned char>(time_low >> 8);
      b[3] = static_cast<unsigned char>(time_low);
      b[4] = static_cast<unsigned char>(time_mid >> 8);
      b[5] = static_cast<unsigned char>(time_mid);
      b[6] = static_cast<unsigned char>((time_hi >> 8) | 0x10);  /* version 1 */
      b[7] = static_cast<unsigned char>(time_hi);

      unsigned char rest[8];
      random_bytes(rest, 8);
      b[8] = static_cast<unsigned char>((rest[0] & 0x3F) | 0x80);  /* variant */
      b[9] = rest[1];
      /* random node with multicast bit set, per RFC for non-MAC nodes */
      b[10] = static_cast<unsigned char>(rest[2] | 0x01);
      memcpy(b + 11, rest + 3, 5);
      break;
    }
    case 4: {
      random_bytes(b, 16);
      b[6] = static_cast<unsigned char>((b[6] & 0x0F) | 0x40);  /* version 4 */
      b[8] = static_cast<unsigned char>((b[8] & 0x3F) | 0x80);  /* variant */
      break;
    }
    case 6: {
      uint64_t ts = gregorian_ticks() & 0x0FFFFFFFFFFFFFFFULL;  /* 60 bits */
      uint32_t time_high = static_cast<uint32_t>((ts >> 28) & 0xFFFFFFFFULL);
      uint16_t time_mid = static_cast<uint16_t>((ts >> 12) & 0xFFFF);
      uint16_t time_low = static_cast<uint16_t>(ts & 0x0FFF);

      b[0] = static_cast<unsigned char>(time_high >> 24);
      b[1] = static_cast<unsigned char>(time_high >> 16);
      b[2] = static_cast<unsigned char>(time_high >> 8);
      b[3] = static_cast<unsigned char>(time_high);
      b[4] = static_cast<unsigned char>(time_mid >> 8);
      b[5] = static_cast<unsigned char>(time_mid);
      b[6] = static_cast<unsigned char>(0x60 | ((time_low >> 8) & 0x0F));
      b[7] = static_cast<unsigned char>(time_low);

      unsigned char rest[8];
      random_bytes(rest, 8);
      b[8] = static_cast<unsigned char>((rest[0] & 0x3F) | 0x80);  /* variant */
      memcpy(b + 9, rest + 1, 7);
      break;
    }
    case 7: {
      using namespace std::chrono;
      uint64_t unix_ms =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count();
      b[0] = static_cast<unsigned char>((unix_ms >> 40) & 0xFF);
      b[1] = static_cast<unsigned char>((unix_ms >> 32) & 0xFF);
      b[2] = static_cast<unsigned char>((unix_ms >> 24) & 0xFF);
      b[3] = static_cast<unsigned char>((unix_ms >> 16) & 0xFF);
      b[4] = static_cast<unsigned char>((unix_ms >> 8) & 0xFF);
      b[5] = static_cast<unsigned char>(unix_ms & 0xFF);

      unsigned char rest[10];
      random_bytes(rest, 10);
      b[6] = static_cast<unsigned char>((rest[0] & 0x0F) | 0x70);  /* version 7 */
      b[7] = rest[1];
      b[8] = static_cast<unsigned char>((rest[2] & 0x3F) | 0x80);  /* variant */
      memcpy(b + 9, rest + 3, 7);
      break;
    }
    default:
      return false;
  }

  format(b, out);
  return true;
}

inline bool supported(int version) {
  return version == 1 || version == 4 || version == 6 || version == 7;
}

}  // namespace uuidv_gen

#endif  /* UUIDV_UUID_GEN_H */
