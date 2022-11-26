#ifndef PRAAS_FUNCTION_CONTEXT_HPP
#define PRAAS_FUNCTION_CONTEXT_HPP

#include <string>
#include <vector>

namespace praas::process {
  struct Invoker;
} // namespace praas::process

namespace praas::function {

  struct Context {

    // FIXME: byte arguments
    void put(std::string name);
    void get(std::string name);

  private:

    Context(process::Invoker& invoker)
        : _invoker(invoker)
    {
    }
    process::Invoker& _invoker;

    friend struct process::Invoker;
  };

} // namespace praas::function

#endif
