#include "pybind11/pybind11.h"
#include "eudaq/DataCollector.hh"

namespace py = pybind11;

class PyDataCollector;

namespace{
  auto dummy = eudaq::Factory<eudaq::DataCollector>::
    Register<PyDataCollector, const std::string&, const std::string&>
    (eudaq::cstr2hash("PyDataCollector"));
}

class PyDataCollector : public eudaq::DataCollector {
public:
  using eudaq::DataCollector::DataCollector;
  
  int32_t var1 = 66;
  const int& GetVar1() { return var1; }
  void SetVar1(int32_t v) { var1 = v; }

  void DoInitialise() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoInitialise
		      );
  }
  void DoConfigure() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoConfigure
		      );
  }
  void DoStartRun() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoStartRun
		      );
  }
  void DoStopRun() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoStopRun
		      );
  }
  void DoReset() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoReset
		      );
  }
  void DoTerminate() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoTerminate
		      );
  }
  void DoStatus() override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoStatus
		      );
  }
  void DoConnect(eudaq::ConnectionSPC id) override {
    PYBIND11_OVERLOAD(void, /* Return type */
		      eudaq::DataCollector,
		      DoConnect,
		      id
		      );
  }
  void DoDisconnect(eudaq::ConnectionSPC id) override {
    PYBIND11_OVERLOAD(void, /* Return type */
  		      eudaq::DataCollector,
  		      DoDisconnect,
  		      id
  		      );
  }
  void DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev) override {
    PYBIND11_OVERLOAD(void, /* Return type */
  		      eudaq::DataCollector,
  		      DoReceive,
  		      id,
  		      ev
  		      );
  }
   
};

void init_pybind_datacollector(py::module &m){
  py::class_<eudaq::DataCollector, PyDataCollector, std::shared_ptr<eudaq::DataCollector>> datacollector_(m, "DataCollector");
  datacollector_.def(py::init([](const std::string &name,const std::string &runctrl){return PyDataCollector::Make("PyDataCollector", name, runctrl);}));
//  datacollector_.def("DoInitialise", &eudaq::DataCollector::DoInitialise);
//  datacollector_.def("DoConfigure", &eudaq::DataCollector::DoConfigure);
//  datacollector_.def("DoStartRun", &eudaq::DataCollector::DoStartRun);
//  datacollector_.def("DoStopRun", &eudaq::DataCollector::DoStopRun);
//  datacollector_.def("DoReset", &eudaq::DataCollector::DoReset);
//  datacollector_.def("DoTerminate", &eudaq::DataCollector::DoTerminate);
//  datacollector_.def("DoConnect", &eudaq::DataCollector::DoConnect,
//		     "Called when a producer is connecting", py::arg("id"));
//  datacollector_.def("DoDisconnect", &eudaq::DataCollector::DoDisconnect,
//		    "Called when a producer is disconnecting", py::arg("id"));
  datacollector_.def("SetStatusTag", &eudaq::DataCollector::SetStatusTag);
  datacollector_.def("SetStatusMsg", &eudaq::DataCollector::SetStatusMsg);
  datacollector_.def("DoReceive", &eudaq::DataCollector::DoReceive,
		     "Called when an event is recievied", py::arg("id"), py::arg("ev"));
  datacollector_.def("WriteEvent", &eudaq::DataCollector::WriteEvent,
		     "Write event to disk", py::arg("ev"));
  datacollector_.def("SetServerAddress", &eudaq::DataCollector::SetServerAddress,
		     "Set port of the data listening", py::arg("addr"));
  datacollector_.def("Connect", &eudaq::DataCollector::Connect);
  datacollector_.def("IsConnected", &eudaq::DataCollector::IsConnected);
  datacollector_.def("GetConfiguration", &eudaq::DataCollector::GetConfiguration);
  datacollector_.def("GetInitConfiguration", &eudaq::DataCollector::GetInitConfiguration);
  // I want to access the RunNumber
  datacollector_.def("GetRunNumber", &eudaq::DataCollector::GetRunNumber);
  datacollector_.def("GetVar1", [](PyDataCollector &self){ return self.GetVar1();});
  datacollector_.def("SetVar1", [](PyDataCollector &self, int32_t v){ self.SetVar1(v);});
  // datacollector_.def_property_readonly_static("var1", [](PyDataCollector  &self) { return self.var1; });
  // datacollector_.def_readwrite("var3", &eudaq::DataCollector::testVar);
}
