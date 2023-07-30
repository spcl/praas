#include <praas/process/runtime/internal/invoker.hpp>
#include <praas/process/runtime/internal/ipc/ipc.hpp>
#include "praas/process/runtime/internal/buffer.hpp"

#if defined(PRAAS_WITH_INVOKER_PYTHON)
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void define_pypraas_invoker(py::module& m)
{

  m.attr("__name__") = "_pypraas.invoker";

  py::enum_<praas::process::runtime::internal::ipc::IPCMode>(m, "IPCMode", py::arithmetic())
      .value("POSIX_MQ", praas::process::runtime::internal::ipc::IPCMode::POSIX_MQ);

  py::class_<praas::process::runtime::internal::Invoker>(m, "Invoker")
      .def(py::init<
           const std::string&, praas::process::runtime::internal::ipc::IPCMode, const std::string&>(
      ))
      .def("poll", &praas::process::runtime::internal::Invoker::poll)
      .def("create_context", &praas::process::runtime::internal::Invoker::create_context)
      .def(
          "finish", py::overload_cast<std::string_view, std::string_view>(
                        &praas::process::runtime::internal::Invoker::finish
                    )
      )
      .def(
          "finish",
          py::overload_cast<
              std::string_view, praas::process::runtime::internal::BufferAccessor<const char>, int>(
              &praas::process::runtime::internal::Invoker::finish
          )
      );
}

#endif
