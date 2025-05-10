#ifndef INC_RENDER_MESH_LOADER_
#define INC_RENDER_MESH_LOADER_

class CDataSaver;

struct ENGINE_API TRawMesh {

  static const uint32_t magic_header = 0x44221100;
  static const uint32_t magic_vertexs = 0x44221101;
  static const uint32_t magic_indices = 0x44221102;
  static const uint32_t magic_groups = 0x44221103;
  static const uint32_t magic_aabb = 0x44221104;
  static const uint32_t magic_eof = 0x44221144;

  struct TSection {
    uint32_t magic;
    uint32_t num_bytes;
  };

  enum eTopology {
    TRIANGLE_LIST = 2000        // as defined in max
  };

  struct THeader {
    static const uint32_t current_version = 2;
    uint32_t version = current_version;
    uint32_t nvertexs = 0;
    uint32_t nindices = 0;
    uint32_t primitive_type = 0;

    uint32_t bytes_per_vertex = 0;
    uint32_t bytes_per_index = 0;
    uint32_t num_groups = 0;
    uint32_t dummy = 0;

    char     vertex_type_name[32] = { 0 };
    bool isValid() const;
  };

  struct TGroup {
    uint32_t first = 0;
    uint32_t count = 0;
  };
  typedef std::vector<TGroup> VGroups;

  THeader                header;
  float                  aabb[6] = { 0 };
  std::vector< uint8_t > vertexs;
  std::vector< uint8_t > indices;
  VGroups                groups;

  bool load(CMemoryDataProvider mdp);
  bool save(TBuffer& ds);

  // Access a vertex addr by it's index
  void* getRawVertexByIndex(uint32_t idx) {
    return vertexs.data() + idx * header.bytes_per_vertex;
  }
  const void* getRawVertexByIndex(uint32_t idx) const {
    return vertexs.data() + idx * header.bytes_per_vertex;
  }

  unsigned short* getIdxs16(int nface = 0) const {
    if (indices.empty())
      return nullptr;
    assert(header.bytes_per_index == 2);
    assert(header.primitive_type == TRIANGLE_LIST);
    return (unsigned short*)&indices[nface * 3 * header.bytes_per_index];
  }
};

#endif
