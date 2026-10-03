#include "platform.h"
#include "benchmark_runner.h"

#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

#if defined(IN_PLATFORM_WINDOWS)
#include <intrin.h>
#else
#include <unistd.h>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#if defined(IN_PLATFORM_APPLE)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#endif

namespace {

std::string trim(std::string value) {
  while (!value.empty() && std::isspace((unsigned char)value.back()))
    value.pop_back();
  size_t first = 0;
  while (first < value.size() && std::isspace((unsigned char)value[first]))
    ++first;
  return value.substr(first);
}

std::string readFirstLine(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::string line;
  if (input)
    std::getline(input, line);
  return trim(line);
}

std::filesystem::path findGitDirectory() {
  std::error_code error;
  std::filesystem::path directory = std::filesystem::current_path(error);
  if (error)
    return {};

  while (!directory.empty()) {
    const std::filesystem::path marker = directory / ".git";
    if (std::filesystem::is_directory(marker, error))
      return marker;
    if (std::filesystem::is_regular_file(marker, error)) {
      const std::string gitdir = readFirstLine(marker);
      const std::string prefix = "gitdir:";
      if (gitdir.compare(0, prefix.size(), prefix) == 0) {
        std::filesystem::path path = trim(gitdir.substr(prefix.size()));
        if (path.is_relative())
          path = directory / path;
        return path.lexically_normal();
      }
    }

    const std::filesystem::path parent = directory.parent_path();
    if (parent == directory)
      break;
    directory = parent;
  }
  return {};
}

std::string detectGitCommit() {
  const std::filesystem::path git_directory = findGitDirectory();
  if (git_directory.empty())
    return "unknown";

  const std::string head = readFirstLine(git_directory / "HEAD");
  if (head.empty())
    return "unknown";

  std::string commit = head;
  const std::string ref_prefix = "ref:";
  if (head.compare(0, ref_prefix.size(), ref_prefix) == 0) {
    const std::string reference = trim(head.substr(ref_prefix.size()));
    commit = readFirstLine(git_directory / reference);
    if (commit.empty()) {
      std::ifstream packed_refs(git_directory / "packed-refs");
      std::string line;
      while (std::getline(packed_refs, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '^')
          continue;
        const size_t separator = line.find(' ');
        if (separator != std::string::npos &&
            line.substr(separator + 1) == reference) {
          commit = line.substr(0, separator);
          break;
        }
      }
    }
  }

  commit = trim(commit);
  if (commit.empty())
    return "unknown";
  if (commit.size() > 12)
    commit.resize(12);
  return commit;
}

std::string detectMachineName() {
  std::array<char, 256> name = {};
#if defined(IN_PLATFORM_WINDOWS)
  DWORD size = (DWORD)name.size();
  if (GetComputerNameA(name.data(), &size))
    return std::string(name.data(), size);
#else
  if (gethostname(name.data(), name.size() - 1) == 0)
    return name.data();
#endif
  return "unknown-machine";
}

std::string detectCpuName() {
#if defined(IN_PLATFORM_WINDOWS) && (defined(_M_X64) || defined(_M_IX86))
  int registers[4] = {};
  __cpuid(registers, 0x80000000);
  const unsigned int max_leaf = (unsigned int)registers[0];
  if (max_leaf >= 0x80000004) {
    std::array<char, 49> brand = {};
    for (unsigned int leaf = 0; leaf < 3; ++leaf) {
      __cpuid(registers, (int)(0x80000002 + leaf));
      memcpy(brand.data() + leaf * 16, registers, 16);
    }
    return trim(brand.data());
  }
#elif (defined(__x86_64__) || defined(__i386__))
  unsigned int eax = 0;
  unsigned int ebx = 0;
  unsigned int ecx = 0;
  unsigned int edx = 0;
  if (__get_cpuid_max(0x80000000, nullptr) >= 0x80000004) {
    std::array<char, 49> brand = {};
    for (unsigned int leaf = 0; leaf < 3; ++leaf) {
      __get_cpuid(0x80000002 + leaf, &eax, &ebx, &ecx, &edx);
      memcpy(brand.data() + leaf * 16 + 0, &eax, 4);
      memcpy(brand.data() + leaf * 16 + 4, &ebx, 4);
      memcpy(brand.data() + leaf * 16 + 8, &ecx, 4);
      memcpy(brand.data() + leaf * 16 + 12, &edx, 4);
    }
    return trim(brand.data());
  }
#elif defined(IN_PLATFORM_APPLE)
  std::array<char, 256> brand = {};
  size_t size = brand.size();
  if (sysctlbyname("machdep.cpu.brand_string", brand.data(), &size,
      nullptr, 0) == 0)
    return trim(brand.data());
  size = brand.size();
  if (sysctlbyname("hw.model", brand.data(), &size, nullptr, 0) == 0)
    return trim(brand.data());
#endif
  return "unknown-cpu";
}

std::string utcTimestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc = {};
#if defined(IN_PLATFORM_WINDOWS)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &utc);
  return buffer;
}

std::string filenameSafe(std::string value) {
  for (char& c : value) {
    const unsigned char uc = (unsigned char)c;
    if (!std::isalnum(uc) && c != '-' && c != '_')
      c = '-';
  }
  while (!value.empty() && value.back() == '-')
    value.pop_back();
  return value.empty() ? "machine" : value;
}

std::string csvField(const std::string& value) {
  if (value.find_first_of(",\"\r\n") == std::string::npos)
    return value;
  std::string escaped = "\"";
  for (char c : value) {
    if (c == '"')
      escaped += '"';
    escaped += c;
  }
  escaped += '"';
  return escaped;
}

} // namespace

const char* benchmarkSceneName(BenchmarkScene scene) {
  switch (scene) {
  case BenchmarkScene::LargeCage: return "large_cage";
  case BenchmarkScene::Platforms: return "platforms";
  case BenchmarkScene::InsideBox: return "inside_box";
  case BenchmarkScene::InsideSphere: return "inside_sphere";
  }
  return "unknown";
}

std::vector<BenchmarkScenario> makeDefaultBenchmarkScenarios(
    BenchmarkScene scene) {
  std::vector<BenchmarkScenario> scenarios;
  for (int particles_k = 8; particles_k <= 128; particles_k += 8) {
    scenarios.push_back({ particles_k * 1024, 12, scene });
    scenarios.push_back({ particles_k * 1024, 24, scene });
  }
  return scenarios;
}

std::vector<BenchmarkScenario> makeThreadScalingBenchmarkScenarios(
    int particle_count,
    int max_worker_threads,
    BenchmarkScene scene) {
  return makeThreadScalingBenchmarkScenarios(
    std::vector<int>{ particle_count }, max_worker_threads, scene);
}

std::vector<BenchmarkScenario> makeThreadScalingBenchmarkScenarios(
    const std::vector<int>& particle_counts,
    int max_worker_threads,
    BenchmarkScene scene) {
  std::vector<BenchmarkScenario> scenarios;
  for (int particle_count : particle_counts) {
    for (int worker_threads = 1;
         worker_threads <= max_worker_threads;
         ++worker_threads) {
      scenarios.push_back({ particle_count, worker_threads, scene });
    }
  }
  return scenarios;
}

BenchmarkRunner::BenchmarkRunner()
  : detected_machine_name(detectMachineName())
  , cpu_name(detectCpuName())
  , git_commit(detectGitCommit()) {
}

bool BenchmarkRunner::start(
    const std::vector<BenchmarkScenario>& new_scenarios,
    const std::string& machine_label,
    const std::string& output_directory) {
  if (running || new_scenarios.empty())
    return false;

  std::error_code error;
  std::filesystem::create_directories(output_directory, error);
  if (error) {
    status_message = "Could not create benchmark output directory: " +
      error.message();
    return false;
  }

  selected_machine_label = machine_label.empty()
    ? detected_machine_name
    : machine_label;
  const std::string filename = utcTimestamp() + "_" +
    filenameSafe(selected_machine_label) + "_" +
    filenameSafe(git_commit) + ".csv";
  std::filesystem::path candidate =
    std::filesystem::path(output_directory) / filename;
  for (int suffix = 2; std::filesystem::exists(candidate, error); ++suffix) {
    candidate = std::filesystem::path(output_directory) /
      (std::filesystem::path(filename).stem().string() + "_" +
       std::to_string(suffix) + ".csv");
  }
  output_path = candidate.string();
  output.open(output_path, std::ios::out | std::ios::trunc);
  if (!output) {
    status_message = "Could not open benchmark output: " + output_path;
    output_path.clear();
    return false;
  }

  output << "machine,cpu,git_commit,scene,particles,threads,average_update_ms\n";
  output.flush();

  scenarios = new_scenarios;
  completed_results.clear();
  scenario_index = 0;
  warmup_frame = 0;
  measured_frame = 0;
  measured_time_sum_ms = 0.0;
  scenario_needs_setup = true;
  running = true;
  status_message = "Benchmark running";
  return true;
}

void BenchmarkRunner::cancel() {
  if (!running)
    return;
  running = false;
  scenario_needs_setup = false;
  status_message = "Benchmark cancelled; completed rows were preserved";
  closeOutput();
}

const BenchmarkScenario& BenchmarkRunner::currentScenario() const {
  assert(running && scenario_index < scenarios.size());
  return scenarios[scenario_index];
}

void BenchmarkRunner::markScenarioReady() {
  assert(needsScenarioSetup());
  scenario_needs_setup = false;
  warmup_frame = 0;
  measured_frame = 0;
  measured_time_sum_ms = 0.0;
}

void BenchmarkRunner::recordUpdate(double update_ms) {
  if (!running || scenario_needs_setup)
    return;

  if (warmup_frame < warmup_frames) {
    ++warmup_frame;
    return;
  }

  measured_time_sum_ms += update_ms;
  ++measured_frame;
  if (measured_frame >= measured_frames)
    finishScenario();
}

int BenchmarkRunner::currentPhaseFrame() const {
  return warmup_frame < warmup_frames ? warmup_frame : measured_frame;
}

int BenchmarkRunner::currentPhaseFrameCount() const {
  return warmup_frame < warmup_frames ? warmup_frames : measured_frames;
}

const char* BenchmarkRunner::currentPhaseName() const {
  if (scenario_needs_setup)
    return "Setting up";
  return warmup_frame < warmup_frames ? "Warm-up" : "Measuring";
}

void BenchmarkRunner::finishScenario() {
  const BenchmarkScenario scenario = currentScenario();
  const double average_ms = measured_time_sum_ms / measured_frames;
  completed_results.push_back({ scenario, average_ms });

  output << csvField(selected_machine_label) << ','
         << csvField(cpu_name) << ','
         << csvField(git_commit) << ','
         << benchmarkSceneName(scenario.scene) << ','
         << scenario.particle_count << ','
         << scenario.worker_threads << ','
         << std::fixed << std::setprecision(6) << average_ms << '\n';
  output.flush();

  ++scenario_index;
  if (scenario_index == scenarios.size()) {
    running = false;
    scenario_needs_setup = false;
    status_message = "Benchmark completed";
    closeOutput();
    return;
  }

  scenario_needs_setup = true;
  warmup_frame = 0;
  measured_frame = 0;
  measured_time_sum_ms = 0.0;
}

void BenchmarkRunner::closeOutput() {
  if (output.is_open())
    output.close();
}
