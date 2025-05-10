#include "platform.h"
#include "raw_mesh.h"

bool TRawMesh::THeader::isValid() const {
  return version == current_version;
}

bool TRawMesh::load(CMemoryDataProvider mdp) {
  
  if (!mdp.isValid())
    return false;

  bool finished = false;
  while (!finished) {

    TSection s;
    mdp.read(s);
    switch (s.magic) {

    case magic_header:
      mdp.read(header);
      if (!header.isValid())
        return false;
      break;

    case magic_vertexs:
      vertexs.resize(s.num_bytes);
      mdp.readBytes(vertexs.data(), s.num_bytes);
      break;

    case magic_indices:
      indices.resize(s.num_bytes);
      mdp.readBytes(indices.data(), s.num_bytes);
      break;

    case magic_groups: {
      uint32_t num_groups = s.num_bytes / sizeof(TGroup);
      if (num_groups != header.num_groups)
        return false;
      groups.resize(num_groups);
      mdp.readBytes(groups.data(), s.num_bytes);
      break;
    }

    case magic_aabb: 
      assert(s.num_bytes == sizeof(aabb));
      mdp.readBytes(&aabb, s.num_bytes);
      break;

    case magic_eof:
      finished = true;
      break;
    }

  }
  return true;
}

static void saveChunk(TBuffer& ds, uint32_t magic, uint32_t num_bytes, const void* data) {
  TRawMesh::TSection s;
  s.magic = magic;
  s.num_bytes = num_bytes;
  ds.write(s);
  if (data)
    ds.writeBytes(data, num_bytes);
}

bool TRawMesh::save(TBuffer& ds) {
  saveChunk(ds, magic_header, sizeof(THeader), &header);
  saveChunk(ds, magic_vertexs, (uint32_t)vertexs.size(), vertexs.data());
  saveChunk(ds, magic_indices, (uint32_t)indices.size(), indices.data());
  saveChunk(ds, magic_groups, (uint32_t)(groups.size() * sizeof(TGroup)), groups.data());
  saveChunk(ds, magic_aabb, (uint32_t)(sizeof(float) * 6), aabb);
  saveChunk(ds, magic_eof, 0, nullptr);
  return true;
}