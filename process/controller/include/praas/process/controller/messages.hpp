#ifndef PRAAS_PROCESS_CONTROLLER_MESSAGES_HPP
#define PRAAS_PROCESS_CONTROLLER_MESSAGES_HPP

#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/ipc/messages.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

#include <boost/functional/hash.hpp>

namespace praas::process {
  struct FunctionWorker;
}

namespace praas::process::message {

  struct PendingMessage {

    enum class Type {
      GET,
      INVOCATION
    };

    Type type;

    const FunctionWorker* source;

  };

  /**
   * Store GET and INVOCATION requests from active local workers.
   *
   * Get request will be fulfilled once we process a corresponding PUT request.
   * We need to store the source of the message and find by the message key.
   *
   * Invocation request will be fulfilled once we receive invocation result, regardless
   * if it is local or remote.
   * We need to store the source of the message, and the invocation key.
   **/
  struct PendingMessages {

    bool insert_get(const std::string& key, const std::string& source, FunctionWorker& worker);

  private:

    using key_t = std::tuple<std::string, std::string>;

    struct KeyHash {
        std::size_t operator()(const key_t & key) const
        {
            return boost::hash_value(key);
        }
    };

    std::unordered_map<std::tuple<std::string, std::string>, PendingMessage, KeyHash> _msgs;
  };

  struct Message {

    std::string source;

    runtime::Buffer<char> data;

  };

  struct MessageStore {

    bool put(const std::string& key, const std::string& source, runtime::Buffer<char> & payload);

    std::optional<runtime::Buffer<char>> try_get(const std::string& key, std::string_view source);

  private:
    std::unordered_map<std::string, Message> _msgs;

    static constexpr std::string_view ANY_PROCESS = "ANY";
  };

} // namespace praas::process

#endif
