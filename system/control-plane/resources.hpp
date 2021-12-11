
#ifndef __CONTROLL_PLANE_RESOURCES_HPP__
#define __CONTROLL_PLANE_RESOURCES_HPP__

#include <future>
#include <memory>
#include <random>
#include <deque>
#include <atomic>
#include <unordered_map>

#include <sockpp/tcp_socket.h>

namespace praas::control_plane {

  struct PendingAllocation {
    std::string payload;
    std::string function_name;
    std::string function_id;
  };

  struct Session {
    sockpp::tcp_socket connection;
    std::string session_id;
    int32_t max_functions;
    int32_t memory_size;
    std::deque<PendingAllocation> allocations;
    bool allocated;
    bool swap_in;

    Session(std::string);
  };

  struct Process {
    std::string global_process_id;
    std::string process_id;
    int16_t allocated_sessions;
    int16_t max_sessions;
    std::atomic<bool> busy;
    sockpp::tcp_socket connection;
    std::vector<Session*> sessions;

    Process(int16_t max_sessions = 0):
      allocated_sessions(0),
      max_sessions(max_sessions),
      busy(0)
    {}

    Process(Process && obj)
    {
      global_process_id = std::move(obj.global_process_id);
      process_id = std::move(obj.process_id);
      allocated_sessions = std::move(obj.allocated_sessions);
      max_sessions = std::move(obj.max_sessions);
      sessions = std::move(obj.sessions);
      connection = std::move(obj.connection);

      // atomics are not movable
      busy.store(obj.busy.load());
    }

    Process& operator=(Process && obj)
    {
      if(this != &obj) {
        global_process_id = std::move(obj.global_process_id);
        process_id = std::move(obj.process_id);
        allocated_sessions = std::move(obj.allocated_sessions);
        max_sessions = std::move(obj.max_sessions);
        sessions = std::move(obj.sessions);
        connection = std::move(obj.connection);

        // atomics are not movable
        busy.store(obj.busy.load());
      }

      return *this;
    }
  };

  struct Resources {

    // Since we store elements in a C++ hash map, we have a gurantee that pointers and references
    // to elements are valid even after a rehashing.
    // Thus, we can safely store pointers in epoll structures, and we know that after receiving
    // a message from process, the pointer to Process data structure will not change,
    // even if we significantly altered the size of this container.

    // session_id -> session
    std::unordered_map<std::string, Session> sessions;
    // process_id -> process resources
    std::unordered_map<std::string, Process> processes;
    // FIXME: processes per global process
    std::mutex mutex;

    ~Resources();

    Process& add_process(Process &&);
    Process* get_process(std::string process_id);
    Process* get_free_process(std::string process_id);
    void remove_process(std::string process_id);
    Session& add_session(Process &, std::string session_id);
    Session* get_session(std::string session_id);
    void remove_session(Process &, std::string session_id);
  };

}

#endif

