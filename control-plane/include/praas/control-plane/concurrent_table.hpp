
#ifndef PRAAS_CONTROL_PLANE_CONCURRENT_TABLE_HPP
#define PRAAS_CONTROL_PLANE_CONCURRENT_TABLE_HPP

#include <string>

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
    // accessor.
    using ro_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::const_accessor;
  };

} // namespace praas::control_plane

#endif
