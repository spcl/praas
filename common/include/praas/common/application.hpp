#ifndef PRAAS_COMMON_APPLICATION_HPP
#define PRAAS_COMMON_APPLICATION_HPP

#include <string>
#include <vector>

namespace praas::common {

  struct Application {

    enum class Status {
      ACTIVE = 1,
      SWAPPED = 2
    };

    std::vector<std::string> active_processes;

    std::vector<std::string> swapped_processes;

    void update(Status status, std::string_view process_id)
    {
      if(status == common::Application::Status::ACTIVE) {
        active_processes.emplace_back(process_id);
      } else {

        auto it = std::find(
          active_processes.begin(),
          active_processes.end(),
          process_id
        );

        if(it != active_processes.end()) {
          swapped_processes.emplace_back(*it);
          active_processes.erase(it);
        }

      }
    }
  };

  struct ApplicationUpdate {
    Application::Status status;
    std::string process_id;
  };

}

#endif
