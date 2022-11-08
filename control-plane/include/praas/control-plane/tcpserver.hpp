
#ifndef PRAAS_CONTROLL_PLANE_POLLER_HPP
#define PRAAS_CONTROLL_PLANE_POLLER_HPP

#include <praas/common/messages.hpp>
#include <praas/control-plane/process.hpp>

#include <memory>
#include <string>
#include <unordered_set>

#include <spdlog/spdlog.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpServer.h>

namespace praas::control_plane {

  class Application;

} // namespace praas::control_plane

namespace praas::control_plane::config {

  class TCPServer;

}

namespace praas::control_plane::worker {

  class Workers;

}

namespace praas::control_plane::tcpserver {

  class TCPServer {
  public:
    TCPServer(const config::TCPServer&, worker::Workers& workers);

    ~TCPServer();

#if defined(WITH_TESTING)
    virtual void add_process(const process::ProcessPtr& ptr);
#else
    void add_process(const process::ProcessPtr& ptr);
#endif

    /**
     * @brief Removes handle from the epoll structure. No further messages will be processed.
     *
     * @param {name} [TODO:description]
     */
#if defined(WITH_TESTING)
    virtual void remove_process(const process::Process&);
#else
    void remove_process(const process::Process&);
#endif

    void shutdown();

    int port() const;

    int num_connected_processes() const;

    int num_registered_processes() const;

  protected:
    void handle_disconnection(const trantor::TcpConnectionPtr& connPtr);

    void handle_message(const trantor::TcpConnectionPtr &connectionPtr, trantor::MsgBuffer *buffer);

    // Looks up the associated invocation in a process and calls the callback.
    // Requires a read/write access to the list of invocations.
    void
    handle_invocation_result(const process::ProcessPtr& ptr, const praas::common::message::InvocationResultParsed&);

    // Calls to process to finish and swap.
    // Needs to call the application to handle the change of process state.
    void handle_swap(const process::ProcessPtr& ptr);

    // Update data plane metrics of a process
    // Requires write access to this process component.
    void
    handle_data_metrics(const process::ProcessPtr& ptr, const praas::common::message::DataPlaneMetricsParsed&);

    // Close down a process.
    // Requires write access to the application.
    void handle_closure(const trantor::TcpConnectionPtr&);

    void handle_connection(const trantor::TcpConnectionPtr&, const common::message::ProcessConnectionParsed & msg);

    //
    //    void handle_message(process::ProcessObserver* process, praas::common::message::Message&&
    //    msg, sockpp::tcp_socket &&);
    //
    //    void handle_error(const process::ProcessObserver* process, sockpp::tcp_socket &&);
    //
    //    std::optional<sockpp::tcp_socket> accept_connection();
    //
    //    static constexpr int MAX_EPOLL_EVENTS = 32;
    //    static constexpr int EPOLL_TIMEOUT = 1000;
    //
    //    // Accept incoming connections
    //    sockpp::tcp_acceptor _listen;
    //

    std::atomic<int> _num_registered_processes;

    std::atomic<int> _num_connected_processes;

    // Main event loop thread
    trantor::EventLoopThread _loop_thread;

    // TCP server instance
    trantor::TcpServer _server;

    // Thread pool
    worker::Workers& _workers;

    bool _is_running;

    // There are following requirements on the data structures
    // (1) We need to support adding handles before connections are made to link new connection
    //     with a socket. Thus, we store pointers to processes.
    // (2) When a new connection is established, we need to store it.
    // (3) When the process announces its name, we need to add context to the connection
    //      to assign the process instance from waiting queue.
    // (4) The handle must be available upon receiving data to perform actions.
    // (5) The connection must be available to a process to send results.
    //
    // A single hash table would be sufficient if it wasn't for the fact that we need to
    // store connections *before* process is recognized.
    std::unordered_map<std::string, process::ProcessPtr> _pending_processes;

    //
    //    // We use epoll to wait for either new connections from
    //    // subprocesses/sessions, or to read new messages from subprocesses.
    //    int _epoll_fd;
    //
    //    std::atomic<bool> _ending;
    //
    //    // There are following requirements on the data structures
    //    // (1) We need to support adding handles before connections are made to link new
    //    connection
    //    // with a socket.
    //    // (2) When a new connection is established, we need to store it.
    //    // (3) When the process announces its name, we need to link to an existing handle via a
    //    name.
    //    // (4) We need to store all of the connections to prevent destruction.
    //    // (5) We need to provide a single pointer to epoll data that contains the connection.
    //    // (6) Both socket and handle must be available upon receiving data.
    //    //
    //    // A single hash table would be sufficient if it wasn't for the fact that we need to
    //    // store connections *before* process is recognized.
    //
    //  ConcurrentTable<process::ProcessObserver>::table_t _handles;
  };

} // namespace praas::control_plane::tcpserver

#endif
