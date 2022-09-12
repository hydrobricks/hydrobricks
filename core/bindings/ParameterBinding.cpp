#include <pybind11/pybind11.h>

namespace py = pybind11;

int xadd(int i, int j) {
    return i + j;
}

PYBIND11_MODULE(hydrobricks, m) {
m.doc() = "pybind11 example plugin"; // optional module docstring

m.def("add", &xadd, "A function that adds two numbers");
}
