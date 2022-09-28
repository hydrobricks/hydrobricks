#include <pybind11/pybind11.h>

#include "Includes.h"
#include "Parameter.h"
#include "ParameterVariable.h"

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(hydrobricks, m) {
    m.doc() = "hydrobricks Python interface";

    py::class_<Parameter>(m, "Parameter")
        .def(py::init<const std::string &, float>())
        .def_property("name", &Parameter::GetName, &Parameter::SetName)
        .def("__repr__",
             [](const Parameter &a) {
                 return "<hydrobricks.Parameter named '" + a.GetName() + "'>";
             }
        );

    py::class_<ParameterVariableYearly, Parameter>(m, "ParameterVariableYearly")
        .def(py::init<const std::string &, float>())
        .def("set_values", &ParameterVariableYearly::SetValues, "Set the parameter values",
            "year_start"_a, "year_end"_a, "values"_a);
}