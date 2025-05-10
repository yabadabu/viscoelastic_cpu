#include "platform.h"
#include "cache_manager.h"
#include <direct.h>

#ifdef IN_PLATFORM_APPLE
#include <sys/stat.h>
int mkdir( const char* path ) {
  return mkdir( path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
}
#endif

TTempStr CCacheManager::storeFilename(const char* name) const {
  uint32_t uid = getID(name);
  char name_no_paths[256] = { 0 };
  int i = 0;
  while (name[i]) {
    name_no_paths[i] = (name[i] == '/') ? '_' : name[i];
    ++i;
  }
  return TTempStr("%s%08x_%s.cache", root_folder.c_str(), uid, name_no_paths);
}

// orig_name  = data/shaders/basic.fx
// cache_name = data/shaders/basic_SM5_ps_DEBUG.fx
void CCacheManager::cache(const std::string& orig_name, const std::string& cache_name, const TBuffer& buffer) {
  TTempStr oname = storeFilename(cache_name.c_str());

  // Recover the current time of the real file
  struct stat result;
  if (stat(orig_name.c_str(), &result) != 0) {
    nerrors++;
    return;
  }

  TBuffer buf_and_meta;
  buf_and_meta.resize(buffer.size() + sizeof(TMetaData));
  memcpy(buf_and_meta.data(), buffer.data(), buffer.size());
  TMetaData* meta = (TMetaData*)(buf_and_meta.data() + buffer.size());
  *meta = TMetaData();
  meta->data_size = buffer.size();
  meta->orig_filename_id = getID(orig_name.c_str());
  meta->time_stamp = result.st_mtime;
  buf_and_meta.save(oname);
  ++nsaved;
}

CCacheManager::CCacheManager(const char* new_title, const char* cache_root_path) {
  title.from(new_title);
  root_folder.from(cache_root_path);
  assert(root_folder.endsWith("/"));
  mkdir(cache_root_path);
}

bool CCacheManager::isCached(const std::string& orig_name, const std::string& cache_name, TBuffer& obuffer) {
  TTempStr oname = storeFilename(cache_name.c_str());
  nmisses++;

  // File must exists
  if (!obuffer.load(oname))
    return false;

  // Must have space for the metadata
  if (obuffer.size() < sizeof(TMetaData))
    return false;
  size_t expected_data_size = obuffer.size() - sizeof(TMetaData);
  const TMetaData* m = (TMetaData*)(obuffer.data() + expected_data_size);

  // Meta data magic must match
  if (!m->isValid())
    return false;

  if (m->orig_filename_id != getID(orig_name.c_str()))
    return false;

  // The saved expected size must match
  if (m->data_size != expected_data_size)
    return false;

  // Recover the current time of the real file
  struct stat result;
  if (stat(orig_name.c_str(), &result) != 0)
    return false;

  // If the file has changed, fail
  if (m->time_stamp != result.st_mtime)
    return false;

  // Return the buffer without the metadata
  obuffer.resize(expected_data_size);

  nmisses--;
  nhits++;

  return true;
}

void CCacheManager::debugInMenu() {
  //if (ImGui::TreeNode(title.c_str())) {
  //  ImGui::Text("Root: %s", root_folder.c_str());
  //  ImGui::Text("Num Misses:%d", nmisses);
  //  ImGui::Text("Num Hits:%d", nhits);
  //  ImGui::Text("Num Saved:%d", nsaved);
  //  ImGui::Text("Num Errors:%d", nerrors);
  //  ImGui::TreePop();
  //}
}
