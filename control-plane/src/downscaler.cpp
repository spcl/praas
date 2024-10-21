
#include <praas/control-plane/downscaler.hpp>

#include <praas/control-plane/application.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/worker.hpp>

namespace praas::control_plane::downscaler {

  void Downscaler::run()
  {
    if(!_enabled) {
      return;
    }

    if(_worker.joinable()) {
      spdlog::error("Downscaler thread is already running!");
      return;
    }
    _worker = std::thread(&Downscaler::_poll, this);
  }

  void Downscaler::shutdown()
  {
    _ending = true;
  }

  void Downscaler::wait()
  {
    if(!_enabled) {
      return;
    }

    if (_worker.joinable()) {
      _worker.join();
    }
  }

  void Downscaler::register_process(process::ProcessPtr process)
  {
    if(!_enabled) {
      return;
    }

    std::unique_lock<std::mutex> lock{_add_mutex};
    _updates_list.emplace_back(true, std::move(process));
  }

  void Downscaler::remove_process(process::ProcessPtr process)
  {
    if(!_enabled) {
      return;
    }

    std::unique_lock<std::mutex> lock{_add_mutex};
    _updates_list.emplace_back(false, std::move(process));
  }

  void Downscaler::_poll()
  {

    while (!_ending) {

      auto now = std::chrono::system_clock::now();
      auto now_timestamp = std::chrono::system_clock::now().time_since_epoch().count();

      spdlog::debug("[Downscaler] Wake up!");

      {
        std::unique_lock<std::mutex> lock{_add_mutex};

        spdlog::debug("[Downscaler] Updating list of processes, # before {}!", _process_list.size());
        for(auto & [status, ptr] : _updates_list) {

          if(status) {

            auto lock = ptr->read_lock();
            auto name = ptr->name();
            _process_list[name] = ProcessStats{std::move(ptr), static_cast<uint64_t>(now_timestamp)};

          } else {
            auto lock = ptr->read_lock();
            _process_list.erase(ptr->name());
          }

        }

        _updates_list.clear();

        spdlog::debug("[Downscaler] Updating list of processes, # after {}!", _process_list.size());
      }

      for (auto& [name, stats] : _process_list) {

        // When was the last check?
        auto dur = now_timestamp - stats.last_event;
        if(dur < _swapping_threshold) {
          continue;
        }

        auto lock = stats.proc->read_lock();
        // Is it already closing/swapped?
        if(stats.proc->status() != process::Status::ALLOCATED) {
          continue;
        }

        uint64_t how_long;
        if(stats.proc->get_metrics().invocations > 0) {
          how_long = now_timestamp - stats.proc->get_metrics().last_invocation;
        } else {
          how_long = now_timestamp - stats.last_event;
        }

        if(how_long > _swapping_threshold) {

          // FIXME: add a method passing proc ptr directly? save search
          auto proc_name = stats.proc->name();

          using ptr_t = void (Application::*)(
              std::string process_name,
              deployment::Deployment* deployment,
              std::function<void(size_t, double, const std::optional<std::string>&)>&& callback
          );
          spdlog::info("[Downscaler] Schedule swap of process {}", proc_name);
          _workers.add_other_task(
            (ptr_t)&Application::swap_process,
            &stats.proc->application(),
            proc_name,
            _deployment,
            [proc_name](size_t, double, const std::optional<std::string>&) {
              spdlog::debug("[Downscaler] Finished swap of process {}", proc_name);
            }
          );
        }

        if(stats.proc->get_metrics().invocations > 0) {
          stats.last_event = stats.proc->get_metrics().last_invocation;
        }
      }

      std::this_thread::sleep_for(std::chrono::seconds(_polling_interval));
    }
  }

} // namespace praas::control_plane::downscaler
