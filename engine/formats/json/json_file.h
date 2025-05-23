#pragma once

#include "json.h"
#include "memory/buffer.h"

struct JsonFile {

  TBuffer     buf;
  JsonParser* parser = nullptr;
  json        j;

  JsonFile(const char* filename);
  JsonFile(TBuffer& external_buf);
  ~JsonFile();

  operator json() const { return j; }
};

