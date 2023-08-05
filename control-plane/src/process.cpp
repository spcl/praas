#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/process.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/uuid.hpp>

#include <drogon/HttpTypes.h>
#include <drogon/utils/FunctionTraits.h>
#include <spdlog/spdlog.h>
#include <trantor/utils/MsgBuffer.h>

namespace praas::control_plane::process {

  DataPlaneConnection::read_lock_t DataPlaneConnection::read_lock() const
  {
    return read_lock_t{_mutex};
  }

  DataPlaneConnection::write_lock_t DataPlaneConnection::write_lock() const
  {
    return write_lock_t{_mutex};
  }

  const std::string& Process::name() const
  {
    return _name;
  }

  const backend::ProcessInstance& Process::c_handle() const
  {
    return *_handle;
  }

  backend::ProcessInstance& Process::handle()
  {
    return *_handle;
  }

  void Process::set_handle(std::shared_ptr<backend::ProcessInstance>&& handle)
  {
    _handle = std::move(handle);
  }

  Application& Process::application() const
  {
    return *_application;
  }

  Status Process::status() const
  {
    return _status;
  }

  state::SessionState& Process::state()
  {
    return _state;
  }

  void Process::set_status(Status status)
  {
    _status = status;
  }

  Process::read_lock_t Process::read_lock() const
  {
    return read_lock_t{_mutex};
  }

  Process::write_lock_t Process::write_lock() const
  {
    return write_lock_t{_mutex};
  }

  void Process::connect(const trantor::TcpConnectionPtr& connectionPtr)
  {
    if (_status != Status::ALLOCATING) {
      throw praas::common::InvalidProcessState{"Can't register process"};
    }

    _connection = std::move(connectionPtr);
    _status = Status::ALLOCATED;
  }

  void Process::close_connection()
  {
    _status = Status::CLOSED;
    _connection.reset();
  }

  void Process::update_metrics(int32_t time, int32_t invocations, uint64_t timestamp)
  {
    std::unique_lock<std::mutex> lock{_metrics_mutex};

    auto cur_timestamp = std::chrono::system_clock::now();
    if (cur_timestamp >= _metrics.last_report) {
      _metrics.computation_time = time;
      _metrics.invocations = invocations;
      _metrics.last_invocation = timestamp;
      _metrics.last_report = cur_timestamp;
    }
  }

  DataPlaneMetrics Process::get_metrics() const
  {
    std::unique_lock<std::mutex> lock{_metrics_mutex};

    return _metrics;
  }

  void Process::swap()
  {
    if (!_connection) {
      return;
    }

    std::string_view swap_path = _state.swap->root_path();
    praas::common::message::SwapRequestData msg;
    msg.path(swap_path);

    _connection->send(msg.bytes(), decltype(msg)::BUF_SIZE);
  }

  void Process::add_invocation(
      HttpServer::request_t request, HttpServer::callback_t&& callback,
      const std::string& function_name
  )
  {
    // FIXME: send immediately if the process is already allocated
    _invocations.emplace_back(
        request, std::move(callback), function_name, _uuid_generator.generate()
    );
    // Count also pending invocations
    _active_invocations++;
    if (_status == Status::ALLOCATED) {
      _send_invocation(_invocations.back());
    }
  }

  void Process::send_invocations()
  {
    for (auto& invoc : _invocations) {
      _send_invocation(invoc);
    }
  }

  void Process::_send_invocation(Invocation& invoc)
  {

    std::string_view payload = invoc.request->getBody();

    praas::common::message::InvocationRequestData req;
    req.function_name(invoc.function_name);
    // FIXME: we have 36 chars but we only need to send 16 bytes
    req.invocation_id(common::UUID::str(invoc.invocation_id).substr(0, 16));
    req.payload_size(payload.length());
    req.total_length(payload.length());

    spdlog::info("Submitting invocation {} to {}", req.invocation_id(), name());

    _connection->send(req.bytes(), req.BUF_SIZE);
    _connection->send(payload.data(), payload.length());
  }

  int Process::active_invocations() const
  {
    return _active_invocations;
  }

  trantor::TcpConnectionPtr Process::dataplane_connection()
  {
    return this->_connection;
  }

  void Process::finish_invocation(
      std::string invocation_id, int return_code, const char* buf, size_t len
  )
  {
    // FIXME: we should be submitting the byte representation, not string - optimize comparison
    auto iter = std::find_if(_invocations.begin(), _invocations.end(), [&](auto& obj) -> bool {
      // FIXME: store and compare byte representation
      return common::UUID::str(obj.invocation_id).substr(0, 16) == invocation_id;
    });

    // FIXME: hide details in the HTTP server
    if (iter != _invocations.end()) {

      --_active_invocations;

      spdlog::error("Responding to client of invocation {}", invocation_id);
      Json::Value json;
      json["function"] = (*iter).function_name;
      json["invocation_id"] = invocation_id;
      json["return_code"] = return_code;
      json["result"] = std::string{buf, len};
      auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
      resp->setStatusCode(drogon::k200OK);

      (*iter).callback(resp);

      _invocations.erase(iter);

    } else {
      spdlog::error("Ignore non-existing invocation {}", invocation_id);
    }
  }

  void Process::set_creation_callback(
      std::function<void(ProcessPtr, std::optional<std::string>)>&& callback
  )
  {
    this->_creation_callback = std::move(callback);
  }

  void Process::created_callback(const std::optional<std::string>& error_msg)
  {
    if (_creation_callback) {
      if (!error_msg.has_value()) {
        _creation_callback(this->shared_from_this(), std::nullopt);
      } else {
        _creation_callback(nullptr, error_msg);
      }
      _creation_callback = nullptr;
    }
  }

} // namespace praas::control_plane::process
