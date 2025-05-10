//
//  memory_data_provider.h
//  Tube01
//
//  Created by Juan Abadia on 8/1/17.
//  Copyright © 2017 Juan Abadia. All rights reserved.
//

#ifndef INC_MEMORY_DATA_PROVIDER_H_
#define INC_MEMORY_DATA_PROVIDER_H_

// ---------------------------------------------------------------------
class CMemoryDataProvider {
  const uint8_t* data = nullptr;     // Memory is not owned
  uint32_t       data_size = 0;
  uint32_t       offset = 0;

public:

  CMemoryDataProvider() = default;

  CMemoryDataProvider(const void* new_data, uint32_t new_data_size)
    : data(static_cast<const uint8_t*>(new_data))
    , data_size(new_data_size)
    , offset(0)
  {}

  const void* consumeBytes(uint32_t nbytes) {
    assert(data);
    assert(offset + nbytes <= data_size);
    auto p = top();
    offset += nbytes;
    return p;
  }

  const void* top() {
    return data + offset;
  }

  void rewind() {
    offset = 0;
  }

  bool isValid() const {
    return data != nullptr;
  }

  uint32_t remainingBytes() const {
    assert(data_size >= offset);
    return data_size - offset;
  }

  template< typename TPOD >
  const TPOD* assign() {
    assert(data);
    const TPOD* pod = static_cast<const TPOD*>(top());
    consumeBytes(sizeof(TPOD));
    return pod;
  }

  template< typename TPOD >
  void read(TPOD& pod) {
    pod = *assign<TPOD>();
  }

  void readBytes(void* out_addr, uint32_t num_bytes) {
    memcpy(out_addr, consumeBytes(num_bytes), num_bytes);
  }

  enum class SeekMode { FROM_START, FROM_CURRENT, FROM_END_OF_FILE };
  int seek(uint32_t new_offset, SeekMode whence) {
    if (whence == SeekMode::FROM_START) {
      offset = new_offset;
    }
    else	if (whence == SeekMode::FROM_CURRENT) {
      offset += new_offset;
    }
    else	if (whence == SeekMode::FROM_END_OF_FILE) {
      offset = data_size - new_offset;
    }
    return 0;
  }

  uint32_t tell() const { return offset; }

};

#endif
