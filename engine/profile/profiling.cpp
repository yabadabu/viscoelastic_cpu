#include "platform.h"
#include <atomic>
#include <mutex>
#include "profiling.h"
#include "time/timer.h"
#include <unordered_map>

#if IN_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace Profiling {

#if ENABLE_PROFILING

	struct TEntry {
		uint64_t     time_stamp;
		const char* name;
		bool isBegin() const { return (time_stamp & 1) == 0; }
	};

	struct TContainer {
		TEntry* entries;
		uint32_t  used;
		uint32_t  max_entries;
		uint32_t  thread_id;
		TStr32    thread_name;
		uint32_t  index;
		std::unordered_map<uint32_t, uint32_t> label_ids;
		Vector<char*> labels;
		TContainer();
		~TContainer();
		uint32_t enter(const char* txt);
		void exit(uint32_t n);
		uint32_t ENGINE_API enterHash(const std::string& txt);
		void reset();
		void ENGINE_API allocEntries();
		void ENGINE_API freeEntries();
	};

  __declspec(thread) TContainer data_container;

  static Vector<TContainer*> data_containers;
  static std::atomic< uint32_t > num_data_containers = 0;
  static bool is_capturing = false;
  static uint32_t nframes_to_capture = 0;

  static double elapsed_time = 0.0f;
  static TTimer tm;
  static uint64_t ts_start_capture = 0;
  static uint64_t ts_end_capture = 0;

  std::mutex mutex_containers;

  TContainer::TContainer()
    : used(0)
    , max_entries(1 << 20)
  {
    entries = nullptr;
    std::unique_lock<std::mutex> lk(mutex_containers);
    index = num_data_containers++;
    data_containers.resize(index+1);
    data_containers[index] = this;

#if IN_PLATFORM_WINDOWS
    thread_id = GetCurrentThreadId();
#else
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    thread_id = tid;
#endif

    thread_name = "main";
  }

  TContainer::~TContainer() {
    freeEntries();
  }

  void TContainer::freeEntries() {
    if (entries)
      delete[] entries;
    entries = nullptr;
    if (index)
      data_containers[index] = nullptr;

    for (char* p : labels)
      free(p);
    labels.clear();
    label_ids.clear();
  }

  void TContainer::allocEntries() {
    entries = new TEntry[max_entries];
  }

  uint32_t TContainer::enter(const char* txt) {
	  if (!entries)
		  allocEntries();
	  TEntry* e = entries + used;
	  e->name = txt;
	  e->time_stamp = __rdtsc() & (~1ULL);
	  uint32_t n = used;
	  used = (used + 1) & (max_entries - 1);
	  return n;
  }

  void TContainer::exit(uint32_t n) {
	  TEntry* e = entries + used;
	  e->name = entries[n].name;
	  e->time_stamp = __rdtsc() | (1ULL);
	  used = (used + 1) & (max_entries - 1);
  }

  TContainer& getDataContainer() {
    return data_container;
  }

  uint32_t enterDataDataContainer(const char* txt) {
	  return getDataContainer().enter(txt);
  }
  
  uint32_t enterHashDataDataContainer(const std::string& name) {
      return getDataContainer().enterHash(name);
  }

  void     exitDataContainer(uint32_t id) {
	  getDataContainer().exit(id);
  }

  uint32_t TContainer::enterHash(const std::string& txt) {
    uint32_t hash_id = getID(txt.c_str(), txt.size());
    uint32_t id = 0;
    auto it = label_ids.find(hash_id);
    if (it == label_ids.end()) {
      id = (uint32_t)labels.size();
      label_ids.insert(it, std::pair<uint32_t, uint32_t>(hash_id, id));
      labels.resize(id + 1);
      labels[id] = strdup(txt.c_str());
    }
    else
      id = it->second;
    return enter(labels[id]);
  }

  void setCurrentThreadName(const char* new_name) {
    data_container.thread_name.from(new_name);
  }

  void TContainer::reset() {
    used = 0;
  }

  void saveAll(const char* ofilename) {
    FILE* f = fopen(ofilename, "wb");
    if (!f)
      return;
	std::unique_lock<std::mutex> lk(mutex_containers);

    // Query the freq to convert cycles to seconds
    uint64_t elapsed_ticks = ts_end_capture - ts_start_capture;

    fprintf(f, "{\"traceEvents\": [");

    uint32_t pid = 1234;
    int n = 0;
    for (uint32_t j = 0; j < num_data_containers; ++j) {
      auto dc = data_containers[j];
      if (!dc)
        continue;
      for (uint32_t i = 0; i < dc->used; ++i, ++n) {
        const TEntry* e = dc->entries + i;
        if (n)
          fprintf(f, ",");

        // Convert cycles to s
        assert(e->time_stamp >= ts_start_capture);
        uint64_t ts = e->time_stamp - ts_start_capture;
        double event_time = 1000000000.0 * ts / elapsed_ticks;
        event_time *= elapsed_time;
        uint64_t event_ticks = (uint64_t)event_time;

        if (e->isBegin()) {
          fprintf(f, "{\"name\":\"%s\", \"cat\":\"c++\"", e->name);
          fprintf(f, ",\"ph\":\"B\",\"ts\": %lld, \"pid\":%d, \"tid\" : %d }\n", event_ticks, pid, dc->thread_id);
        }
        else {
          fprintf(f, "{\"ph\":\"E\",\"ts\": %lld, \"pid\":%d, \"tid\" : %d }\n", event_ticks, pid, dc->thread_id);
        }
      }
      if (dc->used)
        fprintf(f, ",{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": %d, \"tid\": %d, \"args\": {\"name\":\"%s\" }}\n"
          , pid, dc->thread_id, dc->thread_name.c_str());
    }
    fprintf(f, "]}");


    fclose(f);
  }

  void start() {
	std::unique_lock<std::mutex> lk(mutex_containers);
    tm.reset();
    ts_start_capture = __rdtsc();
	for (uint32_t i = 0; i < num_data_containers; ++i) {
      auto dc = data_containers[i];
      if (dc)
        dc->reset();
    }
    is_capturing = true;
  }

  bool isCapturing() {
    return is_capturing;
  }

  void stop() {
    elapsed_time = tm.elapsed();
    ts_end_capture = __rdtsc();
    is_capturing = false;
    saveAll("capture.json");
  }

  void triggerCapture(uint32_t nframes) {
    nframes_to_capture = nframes;
  }


  void frameBegins() {
    if (!is_capturing) {
      if (nframes_to_capture > 0)
        start();
    }
    else {
      if (nframes_to_capture > 0) {
        --nframes_to_capture;
        if (nframes_to_capture == 0)
          stop();
      }
    }
  }

#endif

}


