#ifndef PRAAS_SERVING_DOCKER_CONTAINERS_HPP
#define PRAAS_SERVING_DOCKER_CONTAINERS_HPP

#include <praas/common/http.hpp>

#include <thread>

#include <tbb/concurrent_hash_map.h>

namespace praas::serving::docker {

  template <typename Value, typename Key = std::string>
  struct ConcurrentTable {

    // IntelTBB concurrent hash map, with a default string key
    using table_t = oneapi::tbb::concurrent_hash_map<Key, Value>;

    // Equivalent to receiving a read-write lock. Should be used only for
    // modifying contents.
    using rw_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::accessor;

    // Read lock. Guarantees that data is safe to access, as long as we keep the
    // accessor.
    using ro_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::const_accessor;
  };

  struct Process {

    std::string process_id{};

    std::string container_id{};
  };

  struct Processes {

    using ro_acc_t = typename ConcurrentTable<Process>::ro_acc_t;
    using rw_acc_t = typename ConcurrentTable<Process>::rw_acc_t;

    void add(const std::string& id, Process&& proc);

    void get(const std::string& id, ro_acc_t& acc) const;
    void get(const std::string& id, rw_acc_t& acc);

    bool erase(const std::string& id);

    void get_all(std::vector<Process>& processes);

  private:
    ConcurrentTable<Process>::table_t _processes;
  };

} // namespace praas::serving::docker

#endif
