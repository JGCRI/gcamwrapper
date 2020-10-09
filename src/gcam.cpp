#include <unistd.h>
#define STRICT_R_HEADERS
#include "Rcpp.h"
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

#include "set_data_helper.h"
#include "get_data_helper.h"
#include "solution_debugger.h"

using namespace std;
using namespace Rcpp;

// Declared outside Main to make global.
Scenario* scenario;

class gcam {
    public:
        gcam(string aConfiguration, string aWorkDir):isInitialized(false) {
            int success = chdir(aWorkDir);
            if(!success) {
                Rcpp::stop("Could not set working directory to: "+aWorkDir);
            }
            initializeScenario(aConfiguration);
        }
        void runToPeriod(const int aPeriod ) {
            if(!isInitialized) {
                Rcpp::stop("GCAM did not successfully initialize.");
            }
            Timer timer;

            bool success = runner->runScenarios(aPeriod, false, timer);
            if(!success) {
              Rcpp::warning("Failed to solve period "+util::toString(aPeriod));
            }
        }
      void setData(const DataFrame& aData, const String& aHeader) {
        if(!isInitialized) {
          Rcpp::stop("GCAM did not successfully initialize.");
        }
        RSetDataHelper helper(aData, aHeader);
        helper.run(runner->getInternalScenario());
      }
      List getData(const String& aHeader) {
        if(!isInitialized) {
          Rcpp::stop("GCAM did not successfully initialize.");
        }
        RGetDataHelper helper(aHeader);
        return helper.run(runner->getInternalScenario());
      }

      SolutionDebugger createSolutionDebugger(const int aPeriod) {
        delete scenario->mManageStateVars;
        scenario->mManageStateVars = new ManageStateVariables(aPeriod);
        return SolutionDebugger::createInstance(aPeriod);
      }

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
                Rcpp::stop("Could not parse logger config: "+loggerFileName);
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
                Rcpp::stop("Could not parse configuration: "+configurationFileName);
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
                Rcpp::stop("Failed to setup scenario.");
            }

            // Cleanup Xerces. This should be encapsulated with an initializer object to ensure against leakage.
            XMLHelper<void>::cleanupParser();

            timer.stop();

            isInitialized = true;
        }



};

RCPP_EXPOSED_CLASS_NODECL(gcam)
RCPP_EXPOSED_CLASS_NODECL(SolutionDebugger)
RCPP_MODULE(gcam_module) {
    Rcpp::class_<gcam>("gcam")

        .constructor<string, string>("constructor")

        .method("runToPeriod",        &gcam::runToPeriod,         "run to model period")
        .method("setData", &gcam::setData, "set data")
        .method("getData", &gcam::getData, "get data")
        .method("createSolutionDebugger", &gcam::createSolutionDebugger, "create solution debugger")
        ;

  Rcpp::class_<SolutionDebugger>("SolutionDebugger")

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
  ;
}
