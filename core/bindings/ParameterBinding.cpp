#include <pybind11/pybind11.h>

#include "Includes.h"
#include "Parameter.h"

namespace py = pybind11;

PYBIND11_MODULE(hydrobricks, m) {
    m.doc() = "hydrobricks Python interface";

    py::class_<Parameter>(m, "Parameter")
        .def(py::init<const std::string &>())
        .def("set_name", &Parameter::SetName)
        .def("get_name", &Parameter::GetName);

}
