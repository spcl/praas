#ifndef PRAAS_PROCESS_INVOKER_HPP
#define PRAAS_PROCESS_INVOKER_HPP

#include <praas/function/context.hpp>
#include <praas/function/invocation.hpp>
#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>

#include <memory>
#include <string>
#include <vector>

namespace praas::process {

struct Invoker {

  Invoker(runtime::ipc::IPCMode ipc_mode, std::string ipc_name);

  std::optional<praas::function::Invocation> poll();

  void finish(praas::function::Context &, int return_code);

  void shutdown();

  function::Context create_context();

private:

  // Standard input size = 5 MB
  static constexpr int BUFFER_SIZE = 1024 * 1024 * 5;

  process::runtime::Buffer<std::byte> _input;

  std::atomic<bool> _ending{};

  std::unique_ptr<runtime::ipc::IPCChannel> _ipc_channel_read;
  std::unique_ptr<runtime::ipc::IPCChannel> _ipc_channel_write;
};

} // namespace praas::process

#endif
