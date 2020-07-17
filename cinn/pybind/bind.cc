#include "cinn/pybind/bind.h"

namespace py = pybind11;

namespace cinn::pybind {

PYBIND11_MODULE(core_api, m) {
  m.doc() = "CINN core API";

  py::module runtime  = m.def_submodule("runtime", "bind cinn_runtime");
  py::module common   = m.def_submodule("common", "namespace cinn::common");
  py::module lang     = m.def_submodule("lang", "namespace cinn::lang");
  py::module ir       = m.def_submodule("ir", "namespace cinn::ir");
  py::module poly     = m.def_submodule("poly", "namespace cinn::poly, polyhedral");
  py::module backends = m.def_submodule("backends", "namespace cinn::backends, execution backends");

  BindRuntime(&runtime);
  BindCommon(&common);
  BindLang(&lang);
  BindIr(&ir);
  BindPoly(&poly);
  BindBackends(&backends);
}
}  // namespace cinn::pybind
