
#ifndef PRAAS_CONTROLL_PLANE_RESOURCES_HPP
#define PRAAS_CONTROLL_PLANE_RESOURCES_HPP

#include <praas/control-plane/application.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <tbb/concurrent_hash_map.h>

namespace praas::control_plane {

  template <typename Value, typename Key = std::string>
  struct ConcurrentTable {

    // IntelTBB concurrent hash map, with a default string key
    using table_t = oneapi::tbb::concurrent_hash_map<Key, Value>;

    // Equivalent to receiving a read-write lock. Should be used only for
    // modifying contents.
    using rw_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::accessor;

    // Read lock. Guarantees that data is safe to access, as long as we keep the
    // accessor
    using ro_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::const_accessor;
  };

  class Resources {
  public:
    class ROAccessor {
    public:
      const Application* get() const;

      bool empty() const;

    private:
      using ro_acc_t = ConcurrentTable<Application>::ro_acc_t;
      ro_acc_t _accessor;

      friend class Resources;
    };

    class RWAccessor {
    public:
      Application* get() const;

      bool empty() const;

    private:
      using rw_acc_t = ConcurrentTable<Application>::rw_acc_t;
      rw_acc_t _accessor;

      friend class Resources;
    };

    /**
     * @brief Acquires a write-lock on the hash map to insert a new application.
     *
     * @param {name} desired application object.
     */
    void add_application(Application&& application);

    void get_application(std::string application_name, ROAccessor& acc);
    void get_application(std::string application_name, RWAccessor& acc);

    /**
     * @brief Acquires a write-lock on the hash map to remove application, returning
     *
     * @param application_name
     */
    void delete_application(std::string application_name);

  private:
    ConcurrentTable<Application>::table_t _applications;
  };

  // struct PendingAllocation {
  //   std::string payload;
  //   std::string function_name;
  //   std::string function_id;
  // };

  // struct Resources {

  //  // Since we store elements in a C++ hash map, we have a gurantee that
  //  // pointers and references to elements are valid even after a rehashing.
  //  // Thus, we can safely store pointers in epoll structures, and we know that
  //  // after receiving a message from process, the pointer to Process data
  //  // structure will not change, even if we significantly altered the size of
  //  // this container.

  //  // session_id -> session
  //  std::unordered_map<std::string, Session> sessions;
  //  // process_id -> process resources
  //  std::unordered_map<std::string, Process> processes;
  //  // FIXME: processes per global process
  //  std::mutex _sessions_mutex;

  //} // namespace praas::control_plane
} // namespace praas::control_plane

#endif
