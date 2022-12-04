#include <praas/function/buffer.hpp>
#include <praas/function/context.hpp>
#include <praas/function/invocation.hpp>
#include <praas/process/runtime/buffer.hpp>

#if defined(PRAAS_WITH_INVOKER_PYTHON)

  #include <pybind11/pybind11.h>
  #include <pybind11/stl.h>

  namespace py = pybind11;

  void define_pypraas_invoker(py::module &m);

  void define_pypraas_runtime(py::module &m) {

    py::class_<praas::process::runtime::BufferAccessor<char>>(m, "BufferAccessor")
        .def(py::init());
  }

  void define_pypraas_function(py::module &m) {

    m.doc() = "praas function module";

    // FIXME: function
    m.attr("__name__") = "_pypraas.function";

    py::class_<praas::function::Invocation>(m, "Invocation")
        .def(py::init())
        .def_readonly("key", &praas::function::Invocation::key)
        .def_readonly("function_name",
                      &praas::function::Invocation::function_name)
        .def_readonly("args", &praas::function::Invocation::args);

    py::class_<praas::function::InvocationResult>(m, "InvocationResult")
        .def(py::init())
        .def_readonly("key", &praas::function::InvocationResult::key)
        .def_readonly("function_name",
                      &praas::function::InvocationResult::function_name)
        .def_readonly("return_code",
                      &praas::function::InvocationResult::return_code)
        .def_readonly("payload", &praas::function::InvocationResult::payload);

    py::class_<praas::function::Context>(m, "Context")
        .def_property_readonly("invocation_id",
                               &praas::function::Context::invocation_id)
        .def_property_readonly("process_id",
                               &praas::function::Context::process_id)
        .def_readonly_static("SELF", &praas::function::Context::SELF)
        .def_readonly_static("ANY", &praas::function::Context::ANY)
        .def("start_invocation", &praas::function::Context::start_invocation)
        .def("end_invocation", &praas::function::Context::end_invocation)
        .def("get_output_buffer", &praas::function::Context::get_output_buffer,
             py::arg("size") = 0)
        .def("get_buffer", &praas::function::Context::get_buffer)
        .def("set_output_buffer", &praas::function::Context::set_output_buffer)
        .def("as_buffer", &praas::function::Context::as_buffer)
        .def("put", py::overload_cast<std::string_view, std::string_view,
                                      praas::function::Buffer>(
                        &praas::function::Context::put))
        .def("get", &praas::function::Context::get)
        .def("invoke", &praas::function::Context::invoke);

    py::class_<praas::function::Buffer>(m, "Buffer")
        .def(py::init())
        .def_readonly("pointer", &praas::function::Buffer::ptr)
        .def_readonly("size", &praas::function::Buffer::size)
        .def_readwrite("length", &praas::function::Buffer::len)
        .def("str", &praas::function::Buffer::str)
        .def("view_readable",
             [](praas::function::Buffer &self) {
               return py::memoryview::from_memory(self.ptr,
                                                  sizeof(std::byte) * self.len);
             })
        .def("view_writable", [](praas::function::Buffer &self) {
          return py::memoryview::from_memory(self.ptr,
                                             sizeof(std::byte) * self.size);
        });
  }

  PYBIND11_MODULE(_pypraas, m) {

    auto function_module = m.def_submodule("function");
    auto invoker_module = m.def_submodule("invoker");

    define_pypraas_runtime(m);
    define_pypraas_function(function_module);
    define_pypraas_invoker(invoker_module);
  }

#endif
