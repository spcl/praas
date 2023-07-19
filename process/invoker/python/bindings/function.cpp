#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/context.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/invocation.hpp>

#if defined(PRAAS_WITH_INVOKER_PYTHON)

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void define_pypraas_invoker(py::module& m);

void define_pypraas_runtime(py::module& m)
{

  py::class_<praas::process::runtime::internal::BufferAccessor<char>>(m, "BufferAccessor")
      .def(py::init());
}

void define_pypraas_function(py::module& m)
{

  m.doc() = "praas function module";

  // FIXME: function
  m.attr("__name__") = "_pypraas.function";

  py::class_<praas::process::runtime::Invocation>(m, "Invocation")
      .def(py::init())
      .def_readonly("key", &praas::process::runtime::Invocation::key)
      .def_readonly("function_name", &praas::process::runtime::Invocation::function_name)
      .def_readonly("args", &praas::process::runtime::Invocation::args);

  py::class_<praas::process::runtime::InvocationResult>(m, "InvocationResult")
      .def(py::init())
      .def_readonly("key", &praas::process::runtime::InvocationResult::key)
      .def_readonly("function_name", &praas::process::runtime::InvocationResult::function_name)
      .def_readonly("return_code", &praas::process::runtime::InvocationResult::return_code)
      .def_readonly("payload", &praas::process::runtime::InvocationResult::payload);

  py::class_<praas::process::runtime::Context>(m, "Context")
      .def_property_readonly("invocation_id", &praas::process::runtime::Context::invocation_id)
      .def_property_readonly("process_id", &praas::process::runtime::Context::process_id)
      .def_property_readonly(
          "active_processes", &praas::process::runtime::Context::active_processes
      )
      .def_readonly_static("SELF", &praas::process::runtime::Context::SELF)
      .def_readonly_static("ANY", &praas::process::runtime::Context::ANY)
      .def("start_invocation", &praas::process::runtime::Context::start_invocation)
      .def("end_invocation", &praas::process::runtime::Context::end_invocation)
      .def(
          "get_output_buffer", &praas::process::runtime::Context::get_output_buffer,
          py::arg("size") = 0
      )
      .def("get_buffer", &praas::process::runtime::Context::get_buffer)
      .def("set_output_buffer", &praas::process::runtime::Context::set_output_buffer)
      .def("as_buffer", &praas::process::runtime::Context::as_buffer)
      .def(
          "put",
          py::overload_cast<std::string_view, std::string_view, praas::process::runtime::Buffer>(
              &praas::process::runtime::Context::put
          )
      )
      .def("get", &praas::process::runtime::Context::get)
      .def("invoke", &praas::process::runtime::Context::invoke);

  py::class_<praas::process::runtime::Buffer>(m, "Buffer")
      .def(py::init())
      .def_readonly("pointer", &praas::process::runtime::Buffer::ptr)
      .def_readonly("size", &praas::process::runtime::Buffer::size)
      .def_readwrite("length", &praas::process::runtime::Buffer::len)
      .def("str", &praas::process::runtime::Buffer::str)
      .def(
          "view_readable",
          [](praas::process::runtime::Buffer& self) {
            return py::memoryview::from_memory(self.ptr, sizeof(std::byte) * self.len);
          }
      )
      .def("view_writable", [](praas::process::runtime::Buffer& self) {
        return py::memoryview::from_memory(self.ptr, sizeof(std::byte) * self.size);
      });
}

PYBIND11_MODULE(_pypraas, m)
{

  auto function_module = m.def_submodule("function");
  auto invoker_module = m.def_submodule("invoker");

  define_pypraas_runtime(m);
  define_pypraas_function(function_module);
  define_pypraas_invoker(invoker_module);
}

#endif
