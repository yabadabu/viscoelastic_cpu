//
//  buffer.h
//  Tube01
//
//  Created by Juan Abadia on 4/1/17.
//  Copyright © 2017 Juan Abadia. All rights reserved.
//

#ifndef INC_BUFFER_H_
#define INC_BUFFER_H_

#include <vector>
#include <cstdint>
#include "memory_data_provider.h"

struct TBuffer : public std::vector< uint8_t > {

  TBuffer() = default;
  TBuffer(size_t nbytes) {
    resize(nbytes);
  }
  TBuffer(const char* filename) {
    load(filename);
  }
  TBuffer(const void* addr, size_t nbytes) {
    resize(nbytes);
    memcpy(data(), addr, nbytes);
  }

  explicit TBuffer(const std::string& str) {
    size_t nbytes = str.size();
    resize(nbytes);
    memcpy(data(), str.c_str(), nbytes);
  }

  bool load(const char* doc_filename) {
    return loadRaw(doc_filename);
  }
  bool save(const char* doc_filename) const {
    return saveRaw(doc_filename);
  }

  bool loadRaw(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f)
      return false;
    fseek(f, 0, SEEK_END);
    auto sz = ftell(f);
    this->resize(sz);
    fseek(f, 0, SEEK_SET);
    auto bytes_read = fread(data(), 1, sz, f);
    assert(bytes_read == sz);
    fclose(f);
    return true;
  }
  bool saveRaw(const char* filename) const {
    FILE* f = fopen(filename, "wb");
    if (!f)
      return false;
    auto bytes_written = fwrite(data(), 1, size(), f);
    assert(bytes_written == size());
    fclose(f);
    return true;
  }

  CMemoryDataProvider getNewMemoryDataProvider() const {
    return CMemoryDataProvider(data(), (uint32_t)size());
  }

  // ------------------------------------------------------
  template< typename POD >
  void write(const POD& pod) {
    writeBytes(&pod, sizeof(POD));
  }

  void writeBytes(const void* data_to_save, uint32_t num_bytes) {
    auto prev_size = size();
    resize(prev_size + num_bytes);
    memcpy(data() + prev_size, data_to_save, num_bytes);
  }

  uint32_t writeString(const char* str) {
    uint32_t top = (uint32_t) size();
    writeBytes(str, (uint32_t)strlen(str) + 1);
    return top;
  }

  void write(const TBuffer& other) {
    if (!other.empty())
      writeBytes(other.data(), (uint32_t)other.size());
  }

};

#endif /* buffer_h */
