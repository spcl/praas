#ifndef PRAAS_PROCESS_INVOKER_HPP
#define PRAAS_PROCESS_INVOKER_HPP

#include <praas/process/ipc/ipc.hpp>
#include <praas/function/invocation.hpp>

#include <string>
#include <vector>
#include <memory>

namespace praas::process {

  struct Invoker {

    Invoker(ipc::IPCMode ipc_mode, std::string ipc_name);

    praas::function::Invocation poll();

    // FIXME: move data
    void finish(praas::function::Invocation&);

    void shutdown();
  private:

    std::unique_ptr<ipc::IPCChannel> _ipc_channel_read;
    std::unique_ptr<ipc::IPCChannel> _ipc_channel_write;
  };

} // namespace praas::function

#endif
