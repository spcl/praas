
#ifndef PRAAS_CONTROL_PLANE_DOWNSCALER_HTTP
#define PRAAS_CONTROL_PLANE_DOWNSCALER_HTTP

#include <praas/control-plane/concurrent_table.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>

#include <chrono>
#include <cstdint>

namespace praas::control_plane::downscaler {

  class Downscaler {
  public:
    Downscaler(worker::Workers& workers, deployment::Deployment* deployment, const config::DownScaler& cfg):
      _polling_interval(cfg.polling_interval), _swapping_threshold(cfg.swapping_threshold),
      _enabled(cfg.enabled), _workers(workers)
    {
      praas::control_plane::downscaler::Downscaler::_deployment = deployment;
    }

    ~Downscaler()
    {
      shutdown();
      wait();
    }

    Downscaler(const Downscaler&) = delete;
    Downscaler(Downscaler&&) = delete;
    Downscaler& operator=(const Downscaler&) = delete;
    Downscaler& operator=(Downscaler&&) = delete;

    void run();

    void shutdown();

    void wait();

    void register_process(process::ProcessPtr process);

    void remove_process(process::ProcessPtr process);

  private:

    struct ProcessStats
    {
      process::ProcessPtr proc;
      uint64_t last_event;
    };

    std::unordered_map<std::string, ProcessStats> _process_list;

    std::mutex _add_mutex;
    std::vector<std::tuple<bool, process::ProcessPtr>> _updates_list;

    int _polling_interval;

    int _swapping_threshold;

    bool _enabled;

    std::atomic<bool> _ending{false};

    void _poll();

    worker::Workers& _workers;

    deployment::Deployment* _deployment;
    std::thread _worker;
  };

} // namespace praas::control_plane::downscaler

#endif
