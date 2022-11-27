#ifndef PRAAS_PROCESS_INVOKER_HPP
#define PRAAS_PROCESS_INVOKER_HPP

#include <praas/process/runtime/ipc/ipc.hpp>
#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <string>
#include <vector>
#include <memory>

namespace praas::process {

  struct Invoker {

    Invoker(runtime::ipc::IPCMode ipc_mode, std::string ipc_name);

    std::optional<praas::function::Invocation> poll();

    void finish(praas::function::Context&, int return_code);

    void shutdown();

    function::Context create_context();

  private:

    std::atomic<bool> _ending{};

    std::unique_ptr<runtime::ipc::IPCChannel> _ipc_channel_read;
    std::unique_ptr<runtime::ipc::IPCChannel> _ipc_channel_write;
  };

} // namespace praas::process

#endif
