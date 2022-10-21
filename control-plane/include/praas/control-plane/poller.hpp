
#ifndef PRAAS_CONTROLL_PLANE_POLLER_HPP
#define PRAAS_CONTROLL_PLANE_POLLER_HPP

#include <praas/control-plane/http.hpp>
#include <praas/control-plane/handle.hpp>

#include <memory>
#include <unordered_set>
#include <string>

namespace praas::control_plane {

  class Application;

} // namespace praas::control_plane

namespace praas::control_plane::poller {

  class Poller {
  public:

#if defined(WITH_TESTING)
    virtual void add_handle(const process::ProcessHandle*);
#else
    void add_handle(const process::ProcessHandle*);
#endif

    /**
     * @brief Removes handle from the epoll structure. No further messages will be processed.
     *
     * @param {name} [TODO:description]
     */
#if defined(WITH_TESTING)
    virtual void remove_handle(const process::ProcessHandle*);
#else
    void remove_handle(const process::ProcessHandle*);
#endif

  private:

    void handle_allocation();
    void handle_invocation_result();
    void handle_swap();
    void handle_data_metrics();

    // epoll data structures

    std::unordered_set<const process::ProcessHandle*> _handles;
  };

} // namespace praas::control_plane::poller

#endif
