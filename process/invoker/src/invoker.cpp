#include <praas/process/invoker.hpp>

namespace praas::process {

  Invoker::Invoker(ipc::IPCMode ipc_mode, std::string ipc_name)
  {
    if (ipc_mode == ipc::IPCMode::POSIX_MQ) {
      _ipc_channel = std::make_unique<ipc::POSIXMQChannel>(ipc_name, false);
    }
  }

  praas::function::Invocation Invoker::poll()
  {
    praas::function::Invocation invoc;

    _ipc_channel->receive();

    return invoc;
  }

  void Invoker::finish(praas::function::Invocation&)
  {

  }

  void Invoker::shutdown()
  {

  }
}
