
#ifndef __CONTROLL_PLANE_RESOURCES_HPP__
#define __CONTROLL_PLANE_RESOURCES_HPP__

#include <future>
#include <memory>
#include <random>
#include <deque>
#include <unordered_map>

#include <sockpp/tcp_socket.h>

namespace praas::control_plane {

  struct PendingAllocation {
    std::unique_ptr<int8_t[]> payload;
    int32_t payload_size;
    std::string function_name;
  };

  struct Session {
    sockpp::tcp_socket connection;
    std::string session_id;
    std::deque<PendingAllocation> allocations;
    bool allocated;

    Session(std::string);
  };

  struct Process {
    std::string global_process_name;
    std::string process_id;
    int16_t allocated_sessions;
    sockpp::tcp_socket connection;
    std::vector<Session*> sessions;
  };

  struct Resources {

    // session_id -> session
    std::unordered_map<std::string, Session> sessions;
    // process_id -> process resources
    std::unordered_map<std::string, Process> processes;
    std::mutex mutex;

    ~Resources();

    Process& add_process(Process &&);
    Process* get_process(std::string process_id);
    Session& add_session(Process &, std::string session_id);
    Session* get_session(std::string session_id);
  };

}

#endif

