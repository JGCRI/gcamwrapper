
#include "interp_interface.h"
#include <iostream>

#include "util/base/include/definitions.h"
#include "util/base/include/configuration.h"
#include "util/base/include/model_time.h"
#include "containers/include/scenario.h"
#include "containers/include/iscenario_runner.h"
#include "containers/include/scenario_runner_factory.h"
#include "util/logger/include/ilogger.h"
#include "util/logger/include/logger_factory.h"
#include "util/base/include/timer.h"
#include "util/base/include/version.h"
#include "util/base/include/xml_parse_helper.h"
#include "util/base/include/manage_state_variables.hpp"
#include "util/base/include/time_vector.h"
#include "reporting/include/xml_db_outputter.h"

#include <boost/filesystem/operations.hpp>

#include "set_data_helper.h"
#include "get_data_helper.h"
#include "solution_debugger.h"

using namespace std;


// Declared outside Main to make global.
Scenario* scenario;

class gcam {
    public:
        gcam(string aConfiguration, string aWorkDir):isInitialized(false), mCurrentPeriod(0) {
            try {
                boost::filesystem::current_path(boost::filesystem::path(aWorkDir));
            } catch(...) {
                Interp::stop("Could not set working directory to: "+aWorkDir);
            }
            initializeScenario(aConfiguration);
        }
        gcam(const gcam& aOther):isInitialized(aOther.isInitialized) {
            Interp::stop("TODO: not sure copying is safe");
        }
        ~gcam() {
            delete scenario->mManageStateVars;
            scenario->mManageStateVars = 0;
            runner.reset(0);
            scenario = 0;
        }

        void runToPeriod(const int aPeriod ) {
            if(!isInitialized) {
                Interp::stop("GCAM did not successfully initialize.");
            }
            Timer timer;

            bool success = runner->runScenarios(aPeriod, false, timer);
            mCurrentPeriod = aPeriod;
            if(!success) {
              Interp::warning("Failed to solve period "+util::toString(aPeriod));
            }
        }
      void setData(const Interp::DataFrame& aData, const std::string& aHeader) {
        if(!isInitialized) {
          Interp::stop("GCAM did not successfully initialize.");
        }
        SetDataHelper helper(aData, aHeader);
        helper.run(runner->getInternalScenario());
      }
      Interp::DataFrame getData(const std::string& aHeader) {
        if(!isInitialized) {
          Interp::stop("GCAM did not successfully initialize.");
        }
        GetDataHelper helper(aHeader);
        return helper.run(runner->getInternalScenario());
      }

      SolutionDebugger createSolutionDebugger(const int aPeriod) {
        delete scenario->mManageStateVars;
        scenario->mManageStateVars = new ManageStateVariables(aPeriod);
        return SolutionDebugger::createInstance(aPeriod);
      }

      int getCurrentPeriod() const {
          if(!isInitialized) {
              Interp::stop("GCAM did not successfully initialize.");
          }
          return mCurrentPeriod;
      }

      int convertPeriodToYear(const int aPeriod) const {
          if(!isInitialized) {
              Interp::stop("GCAM did not successfully initialize.");
          }
          return scenario->getModeltime()->getper_to_yr(aPeriod);
      }
      int convertYearToPeriod(const int aYear) const {
          if(!isInitialized) {
              Interp::stop("GCAM did not successfully initialize.");
          }
          return scenario->getModeltime()->getyr_to_per(aYear);
      }

      void printXMLDB() const {
          if(!isInitialized) {
              Interp::stop("GCAM did not successfully initialize.");
          }
          XMLDBOutputter xmldb;
          scenario->accept(&xmldb, -1);
          xmldb.finish();
          xmldb.finalizeAndClose();
      }

    private:
        bool isInitialized;
        int mCurrentPeriod;
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
            XMLParseHelper::initParser();
            bool success = XMLParseHelper::parseXML( loggerFileName, &loggerFactoryWrapper );

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
            success = XMLParseHelper::parseXML( configurationFileName, conf );
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
            XMLParseHelper::cleanupParser();

            timer.stop();

            isInitialized = true;
        }



};

#if defined(IS_INTERP_R)
RCPP_EXPOSED_CLASS_NODECL(gcam)
RCPP_EXPOSED_CLASS_NODECL(SolutionDebugger)
RCPP_MODULE(gcam_module) {
    Rcpp::class_<gcam>("gcam")

        .constructor<string, string>("constructor")

        .method("run_to_period",        &gcam::runToPeriod,         "run to model period")
        .method("set_data", &gcam::setData, "set data")
        .method("get_data", &gcam::getData, "get data")
        .method("create_solution_debugger", &gcam::createSolutionDebugger, "create solution debugger")
        .method("get_current_period", &gcam::getCurrentPeriod, "get the last run model period")
        .method("convert_period_to_year", &gcam::convertPeriodToYear, "convert a GCAM model period to year")
        .method("convert_year_to_period", &gcam::convertYearToPeriod, "convert a GCAM model year to model period")
        .method("print_xmldb", &gcam::printXMLDB, "write the full XML Database results")
        ;

  Rcpp::class_<SolutionDebugger>("SolutionDebugger")

  .method("get_prices",        &SolutionDebugger::getPrices,         "getPrices")
  .method("get_fx", &SolutionDebugger::getFX, "getFX")
  .method("get_supply", &SolutionDebugger::getSupply, "getSupply")
  .method("get_demand", &SolutionDebugger::getDemand, "getDemand")
  .method("get_price_scale_factor", &SolutionDebugger::getPriceScaleFactor, "getPriceScaleFactor")
  .method("get_quantity_scale_factor", &SolutionDebugger::getQuantityScaleFactor, "getQuantityScaleFactor")
  .method("set_prices", &SolutionDebugger::setPrices, "setPrices")
  .method("evaluate", &SolutionDebugger::evaluate, "evaluate")
  .method("evaluate_partial", &SolutionDebugger::evaluatePartial, "evaluatePartial")
  .method("calc_derivative", &SolutionDebugger::calcDerivative, "calcDerivative")
  .method("get_slope", &SolutionDebugger::getSlope, "getSlope")
  .method("set_slope", &SolutionDebugger::setSlope, "setSlope")
  ;
}
#elif defined(IS_INTERP_PYTHON)
using namespace boost::python;

BOOST_PYTHON_MODULE(gcam_module) {
    boost::python::numpy::initialize();
    class_<gcam>("gcam", init<string, string>())

        .def("run_to_period",        &gcam::runToPeriod,         "run to model period")
        .def("set_data", &gcam::setData, "set data")
        .def("get_data", &gcam::getData, "get data")
        .def("create_solution_debugger", &gcam::createSolutionDebugger, "create solution debugger")
        .def("get_current_period", &gcam::getCurrentPeriod, "get the last run model period")
        .def("convert_period_to_year", &gcam::convertPeriodToYear, "convert a GCAM model period to year")
        .def("convert_year_to_period", &gcam::convertYearToPeriod, "convert a GCAM model year to model period")
        .def("print_xmldb", &gcam::printXMLDB, "write the full XML Database results")
        ;
    to_python_converter<Interp::NumericVector, Interp::vec_to_python<Interp::NumericVector> >();
    to_python_converter<Interp::StringVector, Interp::vec_to_python<Interp::StringVector> >();
    to_python_converter<Interp::IntegerVector, Interp::vec_to_python<Interp::IntegerVector> >();
  class_<SolutionDebugger>("SolutionDebugger", no_init)

  .def("get_market_names",        &SolutionDebugger::getMarketNames,         "getMarketNames")
  .def("get_prices",        &SolutionDebugger::getPrices,         "getPrices")
  .def("get_fx", &SolutionDebugger::getFX, "getFX")
  .def("get_supply", &SolutionDebugger::getSupply, "getSupply")
  .def("get_demand", &SolutionDebugger::getDemand, "getDemand")
  .def("get_price_scale_factor", &SolutionDebugger::getPriceScaleFactor, "getPriceScaleFactor")
  .def("get_quantity_scale_factor", &SolutionDebugger::getQuantityScaleFactor, "getQuantityScaleFactor")
  .def("set_prices", &SolutionDebugger::setPrices_wrap, "setPrices")
  .def("evaluate", &SolutionDebugger::evaluate_wrap, "evaluate")
  .def("evaluate_partial", &SolutionDebugger::evaluatePartial, "evaluatePartial")
  .def("calc_derivative", &SolutionDebugger::calcDerivative, "calcDerivative")
  .def("get_slope", &SolutionDebugger::getSlope, "getSlope")
  .def("set_slope", &SolutionDebugger::setSlope_wrap, "setSlope")
  ;
}
#endif
