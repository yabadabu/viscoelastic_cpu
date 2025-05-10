#include "platform.h"
#include "json_file.h"

template <> ENGINE_API void load(json j, VEC2& v) {
  if (j.isArray()) {
    v.x = j[(size_t)0];
    v.y = j[(size_t)1];
  }
  else if (j.is_string()) {
    const char* k = j;
    int n = sscanf(k, "%f %f", &v.x, &v.y);
    assert(n == 2);
    (void)n;
  }
  else
    v = VEC2(0, 0);
}

template <> ENGINE_API void load(json j, VEC3& v) {
  if (j.isArray()) {
    v.x = j[(size_t)0];
    v.y = j[(size_t)1];
    v.z = j[(size_t)2];
  }
  else if (j.is_string()) {
    const char* k = j;
    int n = sscanf(k, "%f %f %f", &v.x, &v.y, &v.z);
    assert(n == 3);
    (void)n;
  }
  else
    v = VEC3(0, 0, 0);
}

template <> ENGINE_API void load(json j, VEC4& v) {
  if (j.isArray()) {
    v.x = j[(size_t)0];
    v.y = j[(size_t)1];
    v.z = j[(size_t)2];
    v.w = j[(size_t)3];
  }
  else if (j.is_string()) {
    const char* k = j;
    int n = sscanf(k, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w);
    assert(n == 4);
    (void)n;
  }
  else
    v = VEC4(0, 0, 0, 0);
}

template <> ENGINE_API void load(json j, QUAT& v) {
  if (!j.is_string()) {
    v = QUAT();
    return;
  }
  const char* k = j;
  assert(k);
  int n = sscanf(k, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w);
  if (n == 3) {
    // Interpret as 3 euler angles
    v = QUAT::createFromYawPitchRoll(deg2rad(v.x), -deg2rad(v.y), deg2rad(v.z));
    n = 4;
  }
  assert(n == 4);
}

void unscapeUnicodes(unsigned char* data, size_t data_size) {

  size_t spos = 0;
  unsigned char* dst = data;
  while (spos < data_size) {
    if (data[spos] == '\\' && spos + 1 < data_size && data[spos + 1] == 'u') {
      // Extract the Unicode code point (4 hexadecimal digits)
      unsigned unicode = 0;
      for (int i = 0; i < 4; ++i) {
        int char_value = 0;
        int hex_char = data[spos + 2 + i];
        if (hex_char >= '0' && hex_char <= '9')
          char_value = hex_char - '0';
        else if (hex_char >= 'A' && hex_char <= 'F')
          char_value = 10 + hex_char - 'A';
        else if (hex_char >= 'a' && hex_char <= 'f')
          char_value = 10 + hex_char - 'a';
        else {
          fatal("Invalid unicode found in json");
        }
        unicode += char_value * (1ULL << ((3 - i) * 4));
      }

      // Convert the code point to UTF-8
      if (unicode <= 0x7F) {
        *dst++ = static_cast<char>(unicode & 0xFF);
      }
      else if (unicode <= 0x7FF) {
        *dst++ = static_cast<char>(0xC0 | ((unicode >> 6) & 0xFF));
        *dst++ = static_cast<char>(0x80 | (unicode & 0x3F));
      }
      else if (unicode <= 0xFFFF) {
        *dst++ = static_cast<char>(0xE0 | ((unicode >> 12) & 0xFF));
        *dst++ = static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
        *dst++ = static_cast<char>(0x80 | (unicode & 0x3F));
      }

      // Move the position past the Unicode escape sequence
      spos += 6;
    }
    else {

      // Copy regular characters
      *dst++ = data[spos];
      ++spos;
    }
  }
}

JsonFile::~JsonFile() { freeJsonParser(parser); }

JsonFile::JsonFile(const char* filename) {

  parser = allocJsonParser();

  while (true) {
    if (!buf.load(filename)) {
      fatal("Failed to load json file %s\n", filename);
      continue;
    }
    unscapeUnicodes(buf.data(), buf.size());
    j = parseJson(parser, (const char*)buf.data(), buf.size());
    if (!j) {
      fatal("Invalid json read from file %s: %s\n", filename, getParseErrorText(parser));
      continue;
    }
    break;
  }
}

JsonFile::JsonFile(TBuffer& external_buf) {
  parser = allocJsonParser();
  unscapeUnicodes(external_buf.data(), external_buf.size());
  j = parseJson(parser, (const char*)external_buf.data(), external_buf.size());
  if (!j) 
    fatal("Invalid json read from buffer of %ld bytes: %s\n", external_buf.size(), getParseErrorText(parser));
}
