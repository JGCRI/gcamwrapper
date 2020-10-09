#include <unistd.h>

#include "interp_interface.h"
#include <iostream>

#include "util/base/include/definitions.h"
#include "util/base/include/configuration.h"
#include "containers/include/scenario.h"
#include "containers/include/iscenario_runner.h"
#include "containers/include/scenario_runner_factory.h"
#include "util/logger/include/ilogger.h"
#include "util/logger/include/logger_factory.h"
#include "util/base/include/timer.h"
#include "util/base/include/version.h"
#include "util/base/include/xml_helper.h"
#include "util/base/include/manage_state_variables.hpp"

//#include "set_data_helper.h"
//#include "get_data_helper.h"
//#include "solution_debugger.h"

using namespace std;
//using namespace Rcpp;


// Declared outside Main to make global.
Scenario* scenario;

class gcam {
    public:
        gcam(string aConfiguration, string aWorkDir):isInitialized(false) {
            int success = chdir(aWorkDir.c_str());
            if(!success) {
                Interp::stop("Could not set working directory to: "+aWorkDir);
            }
            initializeScenario(aConfiguration);
        }
        gcam(const gcam& aOther):isInitialized(aOther.isInitialized) {
            cout << "it's copying" << endl;
        }

        void runToPeriod(const int aPeriod ) {
            if(!isInitialized) {
                Interp::stop("GCAM did not successfully initialize.");
            }
            Timer timer;

            bool success = runner->runScenarios(aPeriod, false, timer);
            if(!success) {
              Interp::warning("Failed to solve period "+util::toString(aPeriod));
            }
        }
      void setData(const Interp::DataFrame& aData, const std::string& aHeader) {
          std::cout << aHeader << std::endl;
          namespace bp = boost::python;
          namespace bnp = boost::python::numpy;
          Interp::NumericVector vec = Interp::getDataFrameCol(aData, 1);//bp::extract<Interp::NumericVector>(aData.values()[1]);
          //Interp::NumericVector vec = bnp::ndarray::from_object(aData.values()[1], bnp::dtype::get_builtin<double>());
          std::cout << (vec.get_dtype().get_itemsize()) << std::endl;
          double* d = reinterpret_cast<double*>(vec.get_data());
          cout << d[0]<< " " << d[1] << endl;
          Interp::StringVector strVec = Interp::getDataFrameCol(aData, 0);//bp::extract<Interp::NumericVector>(aData.values()[0]);
          std::cout << (strVec.get_dtype().get_itemsize()) << std::endl;
          cout << bp::extract<char const *>(bp::str(strVec.get_dtype())) << endl;
          bp::str* s = reinterpret_cast<bp::str*>(strVec.get_data());
          cout << bp::extract<char const*>(s[0]) << " " << bp::extract<char const *>(s[1]) << endl;
          if(s[0] == "coal") { 
              cout << "Does match!" << endl;
          }
          /*
        if(!isInitialized) {
          Interp::stop("GCAM did not successfully initialize.");
        }
        */
        //RSetDataHelper helper(aData, aHeader);
        //helper.run(runner->getInternalScenario());
      }
      Interp::DataFrame getData(const std::string& aHeader) {
          /*
        if(!isInitialized) {
          Interp::stop("GCAM did not successfully initialize.");
        }
        RGetDataHelper helper(aHeader);
        return helper.run(runner->getInternalScenario());
        */
          std::vector<std::string> names = { "sector", "value" };
          Interp::StringVector sectorVec = Interp::createVector<std::string, Interp::StringVector>(2);
          //Interp::bp::str* s = reinterpret_cast<Interp::bp::str*>(sectorVec.get_data());
          sectorVec[0] = std::string("coal");
          sectorVec[1] = std::string("gas");
          Interp::NumericVector valueVec = Interp::createVector<double, Interp::NumericVector>(2);
          valueVec[0] = 1.0;
          valueVec[1] = 2.0;
          Interp::DataFrame ret = Interp::createDataFrame(names);
          ret["sector"] = sectorVec;
          ret["value"] = valueVec;
          return ret;
      }

      /*
      SolutionDebugger createSolutionDebugger(const int aPeriod) {
        delete scenario->mManageStateVars;
        scenario->mManageStateVars = new ManageStateVariables(aPeriod);
        return SolutionDebugger::createInstance(aPeriod);
      }
      */

    private:
        bool isInitialized;
        LoggerFactoryWrapper loggerFactoryWrapper;
        auto_ptr<IScenarioRunner> runner;
        void initializeScenario(string configurationArg) {
            string loggerFactoryArg = "log_conf.xml";

            // Add OS dependent prefixes to the arguments.
            const string configurationFileName = configurationArg;
            const string loggerFileName = loggerFactoryArg;

            // Initialize the timer.  Create an object of the Timer class.
            Timer timer;
            timer.start();


            // Initialize the LoggerFactory
            bool success = XMLHelper<void>::parseXML( loggerFileName, &loggerFactoryWrapper );

            // Check if parsing succeeded. Non-zero return codes from main indicate
            // failure.
            if( !success ){
                Interp::stop("Could not parse logger config: "+loggerFileName);
            }


            // Get the main log file.
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );

            mainLog << "Running GCAM model code base version " << __ObjECTS_VER__ << " revision "
                << __REVISION_NUMBER__ << endl << endl;

            // Parse configuration file.
            mainLog.setLevel( ILogger::NOTICE );
            mainLog << "Configuration file:  " << configurationFileName << endl;
            mainLog << "Parsing input files..." << endl;
            Configuration* conf = Configuration::getInstance();
            success = XMLHelper<void>::parseXML( configurationFileName, conf );
            // Check if parsing succeeded. Non-zero return codes from main indicate
            // failure.
            if( !success ){
                Interp::stop("Could not parse configuration: "+configurationFileName);
            }

            // Create an empty exclusion list so that any type of IScenarioRunner can be
            // created.
            list<string> exclusionList;

            // Create an auto_ptr to the scenario runner. This will automatically
            // deallocate memory.
            runner = ScenarioRunnerFactory::createDefault( exclusionList );

            // Setup the scenario.
            success = runner->setupScenarios( timer );
            // Check if setting up the scenario, which often includes parsing,
            // succeeded.
            if( !success ){
                Interp::stop("Failed to setup scenario.");
            }

            // Cleanup Xerces. This should be encapsulated with an initializer object to ensure against leakage.
            XMLHelper<void>::cleanupParser();

            timer.stop();

            isInitialized = true;
        }



};

#if defined(USING_R)
RCPP_EXPOSED_CLASS_NODECL(gcam)
//RCPP_EXPOSED_CLASS_NODECL(SolutionDebugger)
RCPP_MODULE(gcam_module) {
    Rcpp::class_<gcam>("gcam")

        .constructor<string, string>("constructor")

        .method("runToPeriod",        &gcam::runToPeriod,         "run to model period")
        .method("setData", &gcam::setData, "set data")
        .method("getData", &gcam::getData, "get data")
        .method("createSolutionDebugger", &gcam::createSolutionDebugger, "create solution debugger")
        ;

  /*Rcpp::class_<SolutionDebugger>("SolutionDebugger")

  .method("getPrices",        &SolutionDebugger::getPrices,         "getPrices")
  .method("getFX", &SolutionDebugger::getFX, "getFX")
  .method("getSupply", &SolutionDebugger::getSupply, "getSupply")
  .method("getDemand", &SolutionDebugger::getDemand, "getDemand")
  .method("getPriceScaleFactor", &SolutionDebugger::getPriceScaleFactor, "getPriceScaleFactor")
  .method("getQuantityScaleFactor", &SolutionDebugger::getQuantityScaleFactor, "getQuantityScaleFactor")
  .method("setPrices", &SolutionDebugger::setPrices, "setPrices")
  .method("evaluate", &SolutionDebugger::evaluate, "evaluate")
  .method("evaluatePartial", &SolutionDebugger::evaluatePartial, "evaluatePartial")
  .method("calcDerivative", &SolutionDebugger::calcDerivative, "calcDerivative")
  .method("getSlope", &SolutionDebugger::getSlope, "getSlope")
  .method("setSlope", &SolutionDebugger::setSlope, "setSlope")
  ;*/
}
#elif defined(PY_VERSION_HEX)
using namespace boost::python;
BOOST_PYTHON_MODULE(gcam_module) {
    boost::python::numpy::initialize();
    class_<gcam>("gcam", init<string, string>())

        .def("runToPeriod",        &gcam::runToPeriod,         "run to model period")
        .def("setData", &gcam::setData, "set data")
        .def("getData", &gcam::getData, "get data")
        //.def("createSolutionDebugger", &gcam::createSolutionDebugger, "create solution debugger")
        ;

    /*
  class_<SolutionDebugger>("SolutionDebugger", no_init)

  .def("getPrices",        &SolutionDebugger::getPrices,         "getPrices")
  .def("getFX", &SolutionDebugger::getFX, "getFX")
  .def("getSupply", &SolutionDebugger::getSupply, "getSupply")
  .def("getDemand", &SolutionDebugger::getDemand, "getDemand")
  .def("getPriceScaleFactor", &SolutionDebugger::getPriceScaleFactor, "getPriceScaleFactor")
  .def("getQuantityScaleFactor", &SolutionDebugger::getQuantityScaleFactor, "getQuantityScaleFactor")
  .def("setPrices", &SolutionDebugger::setPrices, "setPrices")
  .def("evaluate", &SolutionDebugger::evaluate, "evaluate")
  .def("evaluatePartial", &SolutionDebugger::evaluatePartial, "evaluatePartial")
  .def("calcDerivative", &SolutionDebugger::calcDerivative, "calcDerivative")
  .def("getSlope", &SolutionDebugger::getSlope, "getSlope")
  .def("setSlope", &SolutionDebugger::setSlope, "setSlope")
  ;
  */
}
#endif
