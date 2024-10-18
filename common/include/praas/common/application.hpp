#ifndef PRAAS_COMMON_APPLICATION_HPP
#define PRAAS_COMMON_APPLICATION_HPP

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace praas::common {

  struct Application {

    enum class Status : int8_t {
      NONE = 0,
      ACTIVE = 1,
      SWAPPED = 2,
      CLOSED = 3,
      DELETED = 4,
    };

    std::vector<std::string> active_processes;

    std::vector<std::string> swapped_processes;

    void update(Status status, std::string_view process_id)
    {
      /**
       * Transitions:
       * New process -> add active.
       * Process swaps off -> remove from active, add to swapped.
       * Process swaps in -> remove from swapped, add to active.
       * Process closed -> removed from active.
       * Process deleted -> remove from swapped.
       */
      if(status == common::Application::Status::ACTIVE) {

        active_processes.emplace_back(process_id);

        // Remove from swapped.
        auto it = std::find(
          swapped_processes.begin(),
          swapped_processes.end(),
          process_id
        );
        if(it != swapped_processes.end()) {
          swapped_processes.erase(it);
        }

      } else if(status == common::Application::Status::SWAPPED) {

        auto it = std::find(
          active_processes.begin(),
          active_processes.end(),
          process_id
        );

        if(it != active_processes.end()) {
          swapped_processes.emplace_back(*it);
          active_processes.erase(it);
        } else {
          swapped_processes.emplace_back(process_id);
        }

      } else if(status == common::Application::Status::CLOSED) {

        auto it = std::find(
          active_processes.begin(),
          active_processes.end(),
          process_id
        );

        if(it != active_processes.end()) {
          active_processes.erase(it);
        } else {
          spdlog::error("Ignoring non-existing active process {}", process_id);
        }

      } else if(status == common::Application::Status::DELETED) {

        auto it = std::find(
          swapped_processes.begin(),
          swapped_processes.end(),
          process_id
        );

        if(it != swapped_processes.end()) {
          swapped_processes.erase(it);
        } else {
          spdlog::error("Ignoring non-existing active process {}", process_id);
        }

      }
    }
  };

  struct ApplicationUpdate {
    Application::Status status;
    std::string process_id;
  };

} // namespace praas::common

#endif
