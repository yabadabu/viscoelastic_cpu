#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

enum class BenchmarkScene : uint8_t {
  LargeCage = 0,
  Platforms,
  InsideBox,
  InsideSphere,
};

const char* benchmarkSceneName(BenchmarkScene scene);

struct BenchmarkScenario {
  int particle_count = 0;
  int worker_threads = 0;
  BenchmarkScene scene = BenchmarkScene::LargeCage;
};

struct BenchmarkResult {
  BenchmarkScenario scenario;
  double average_update_ms = 0.0;
};

std::vector<BenchmarkScenario> makeDefaultBenchmarkScenarios(
  BenchmarkScene scene);
std::vector<BenchmarkScenario> makeThreadScalingBenchmarkScenarios(
  int particle_count,
  int max_worker_threads,
  BenchmarkScene scene);
std::vector<BenchmarkScenario> makeThreadScalingBenchmarkScenarios(
  const std::vector<int>& particle_counts,
  int max_worker_threads,
  BenchmarkScene scene);

class BenchmarkRunner {
public:
  static constexpr int warmup_frames = 300;
  static constexpr int measured_frames = 300;

  BenchmarkRunner();

  bool start(
    const std::vector<BenchmarkScenario>& new_scenarios,
    const std::string& machine_label,
    const std::string& output_directory = "benchmarks/results");
  void cancel();

  bool isRunning() const { return running; }
  bool needsScenarioSetup() const { return running && scenario_needs_setup; }
  const BenchmarkScenario& currentScenario() const;
  void markScenarioReady();
  void recordUpdate(double update_ms);

  size_t currentScenarioIndex() const { return scenario_index; }
  size_t scenarioCount() const { return scenarios.size(); }
  int currentPhaseFrame() const;
  int currentPhaseFrameCount() const;
  const char* currentPhaseName() const;

  const std::string& detectedMachineName() const { return detected_machine_name; }
  const std::string& cpuName() const { return cpu_name; }
  const std::string& gitCommit() const { return git_commit; }
  const std::string& outputPath() const { return output_path; }
  const std::string& statusMessage() const { return status_message; }
  const std::vector<BenchmarkResult>& completedResults() const {
    return completed_results;
  }

private:
  void finishScenario();
  void closeOutput();

  std::vector<BenchmarkScenario> scenarios;
  std::vector<BenchmarkResult> completed_results;
  size_t scenario_index = 0;
  int warmup_frame = 0;
  int measured_frame = 0;
  double measured_time_sum_ms = 0.0;
  bool running = false;
  bool scenario_needs_setup = false;

  std::string detected_machine_name;
  std::string selected_machine_label;
  std::string cpu_name;
  std::string git_commit;
  std::string output_path;
  std::string status_message;
  std::ofstream output;
};
