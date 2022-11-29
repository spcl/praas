#include <praas/function/context.hpp>
#include <praas/function/invocation.hpp>
#include <praas/process/runtime/buffer.hpp>

#if defined(WITH_INVOKER_PYTHON)
  #include <pybind11/pybind11.h>
  #include <pybind11/stl.h>

  namespace py = pybind11;

  void define_pypraas_invoker(py::module & m);

  void define_pypraas_runtime(py::module & m) {

    py::class_<praas::process::runtime::BufferAccessor<char>>(m, "buffer_accessor")
        .def(py::init());

  }

  void define_pypraas_function(py::module & m) {

    m.doc() = "praas function module";

    // FIXME: function
    m.attr("__name__") = "pypraas.function";

    py::class_<praas::function::Invocation>(m, "invocation")
        .def(py::init())
        .def_readonly("key", &praas::function::Invocation::key)
        .def_readonly("function_name", &praas::function::Invocation::function_name)
        .def_readonly("args", &praas::function::Invocation::args);

    py::class_<praas::function::Context>(m, "context")
        .def_property_readonly("invocation_id", &praas::function::Context::invocation_id)
        .def_property_readonly("process_id", &praas::function::Context::process_id)
        .def("start_invocation", &praas::function::Context::start_invocation)
        .def("end_invocation", &praas::function::Context::end_invocation)
        .def("as_buffer", &praas::function::Context::as_buffer);

    py::class_<praas::function::Buffer>(m, "buffer")
        .def(py::init());
        //.def_property_readonly("pointer", &praas::function::Buffer::ptr)
        //.def_property_readonly("length", &praas::function::Buffer::len)
        //.def_property_readonly("size", &praas::function::Buffer::size);
  }

  PYBIND11_MODULE(pypraas, m) {

    m.def_submodule("function", "test");

    define_pypraas_runtime(m);
    define_pypraas_function(m);
    define_pypraas_invoker(m);
  }

#endif
