#include <praas/control-plane/process.hpp>

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/application.hpp>
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/uuid.hpp>

#include <chrono>
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

  std::shared_ptr<backend::ProcessInstance>& Process::handle_ptr()
  {
    return _handle;
  }

  void Process::reset_handle()
  {
    _handle.reset();
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
    if (_status != Status::ALLOCATING && _status != Status::SWAPPING_IN) {
      throw praas::common::InvalidProcessState{fmt::format("Can't register process. Wrong status: {}", static_cast<int>(_status))};
    }

    _connection = std::move(connectionPtr);
    _status = Status::ALLOCATED;

    // We notify everyone else of our presence AND send initial world description
    application().connected_process(*this);

    // Now send pending invocations. Lock prevents adding more invocations,
    // and by the time we are finishing, the direct connection will be already up
    // and invocation can be submitted directly.
    // Thus, no invocation should be lost.
    send_pending_messages();

    this->created_callback(std::nullopt);
  }

  void Process::close_connection()
  {
    if(_connection) {
      _connection->shutdown();
      // Shutting down connection will not prevent the disconnection
      // trigger from running in trantor.
      // We need to let it know the connection is gone and trigger is expected.
      _connection->setContext(nullptr);
    }
  }

  void Process::closed_connection()
  {
    _status = Status::CLOSED;
    _connection->setContext(nullptr);
    _connection.reset();
  }

  void Process::update_metrics(uint64_t time, uint32_t invocations, uint64_t timestamp)
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

  void Process::swap(std::function<void(size_t, double, const std::optional<std::string>&)>&& callback)
  {
    if (!_connection) {
      if(callback) {
        callback(0, 0, "Process is not connected");
      }
      return;
    }

    std::string swap_path = _state.swap->path(_name);
    praas::common::message::SwapRequestData msg;
    msg.path(swap_path);

    _swapping_callback = std::move(callback);
    _connection->send(msg.bytes(), decltype(msg)::BUF_SIZE);
  }

  void Process::add_invocation(
      HttpServer::request_t request, HttpServer::callback_t&& callback,
      const std::string& function_name, std::chrono::high_resolution_clock::time_point start
  )
  {
    _invocations.emplace_back(
        request, std::move(callback), function_name, _uuid_generator.generate(), start
    );
    // Count also pending invocations
    _active_invocations++;
    if (_status == Status::ALLOCATED) {
      _send_invocation(_invocations.back());
    }
  }

  void Process::send_pending_messages()
  {
    for (auto& invoc : _invocations) {
      if (!invoc.submitted) {
        _send_invocation(invoc);
      }
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

    invoc.submitted = true;
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

      auto now = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::microseconds>(now - (*iter).start).count();
      spdlog::info(fmt::format("Invocation finished, took {} us", duration));

      spdlog::error("Responding to client of invocation {}", invocation_id);
      Json::Value json;
      json["function"] = (*iter).function_name;
      json["process_name"] = _name;
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
      std::function<void(ProcessPtr, std::optional<std::string>)>&& callback,
      bool wait_for_allocation
  )
  {
    this->_creation_callback = std::move(callback);
    this->_wait_for_allocation = wait_for_allocation;
  }

  void Process::created_callback(const std::optional<std::string>& error_msg)
  {
    if (_creation_callback) {
      if (!error_msg.has_value()) {

        // Success is reported conditionally - process is connected and backend
        // returned process information.
        if (!this->_handle) {
          return;
        }
        if (_wait_for_allocation && this->_status != Status::ALLOCATED) {
          return;
        }

        // Created callback can be called by process connection, or by
        // the backend returning information.
        // For example, Fargate needs to query the IP address.
        // Only once we have, we can call back HTTP query.
        _handle->connect([this](const std::optional<std::string>& error_msg) {
          if (error_msg.has_value()) {
            _creation_callback(nullptr, error_msg);
          } else {
            _creation_callback(this->shared_from_this(), std::nullopt);
          }
          _creation_callback = nullptr;
        });
      } else {

        // Failure is always reported.
        _creation_callback(nullptr, error_msg);
        _creation_callback = nullptr;
      }
    }
  }

  void Process::swapped_callback(size_t size, double time, const std::optional<std::string>& error_msg)
  {
    if (_swapping_callback) {
      _swapping_callback(size, time, error_msg);
      _swapping_callback = nullptr;
    }
  }

} // namespace praas::control_plane::process
