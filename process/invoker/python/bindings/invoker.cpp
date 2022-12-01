#include <praas/process/invoker.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>

#if defined(WITH_INVOKER_PYTHON)
  #include <pybind11/pybind11.h>
  #include <pybind11/stl.h>

  namespace py = pybind11;

  void define_pypraas_invoker(py::module & m) {

    m.attr("__name__") = "_pypraas.invoker";

    py::enum_<praas::process::runtime::ipc::IPCMode>(m, "IPCMode", py::arithmetic())
        .value("POSIX_MQ", praas::process::runtime::ipc::IPCMode::POSIX_MQ);

    py::class_<praas::process::Invoker>(m, "Invoker")
        .def(py::init<const std::string&, praas::process::runtime::ipc::IPCMode, const std::string&>())
        .def("poll", &praas::process::Invoker::poll)
        .def("create_context", &praas::process::Invoker::create_context)
        .def("finish", &praas::process::Invoker::finish);
  }

#endif
