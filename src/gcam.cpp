
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
#include "containers/include/imodel_feedback_calc.h"
#include "marketplace/include/marketplace.h"
#include "containers/include/world.h"

#include <boost/filesystem/operations.hpp>

#include "set_data_helper.h"
#include "get_data_helper.h"
#include "solution_debugger.h"

using namespace std;


// Declared outside Main to make global.
Scenario* scenario;

class gcam {
    public:
        gcam(string aConfiguration, string aWorkDir):isInitialized(false), mCurrentPeriod(0), mIsMidPeriod(false) {
            mCoutOrig = cout.rdbuf(Interp::getInterpCout().rdbuf());
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
            Configuration::reset();
            cout.rdbuf(mCoutOrig);
        }

        void runPeriod(const int aPeriod ) {
          if(!isInitialized) {
            Interp::stop("GCAM did not successfully initialize.");
          }
          Timer timer;

          bool success = runner->runScenarios(aPeriod, false, timer);
          if(!success) {
            Interp::warning("Failed to solve period "+util::toString(aPeriod));
          }
          mCurrentPeriod = aPeriod;
        }

        void runPeriodPre(const int aPeriod ) {
            if(!isInitialized) {
                Interp::stop("GCAM did not successfully initialize.");
            }
            Timer timer;

            if(aPeriod > 0 && (mCurrentPeriod+1) < aPeriod) {
              bool success = runner->runScenarios(aPeriod-1, false, timer);
              if(!success) {
                Interp::warning("Failed to solve period "+util::toString(aPeriod));
              }
            }
            mCurrentPeriod = aPeriod;

            scenario->logPeriodBeginning( aPeriod );

    // If this is period 0 initialize market price.
    if( aPeriod == 0 ){
        scenario->mMarketplace->initPrices(); // initialize prices
    }

    // Run the iteration of the model.
    scenario->mMarketplace->nullSuppliesAndDemands( aPeriod ); // initialize market demand to null
    scenario->mMarketplace->init_to_last( aPeriod ); // initialize to last period's info
    scenario->mWorld->initCalc( aPeriod ); // call to initialize anything that won't change during calc
    scenario->mMarketplace->assignMarketSerialNumbers( aPeriod ); // give the markets their serial numbers for this period.

    // Call any model feedback objects before we begin solving this period but after
    // we are initialized and ready to go.
    for( auto modelFeedback : scenario->mModelFeedbacks ) {
        modelFeedback->calcFeedbacksBeforePeriod( scenario, scenario->mWorld->getClimateModel(), aPeriod );
    }

    // Set up the state data for the current period.
    delete scenario->mManageStateVars;
    scenario->mManageStateVars = new ManageStateVariables( aPeriod );

    // Be sure to clear out any supplies and demands in the marketplace before making our
    // initial call to world.calc.  There may already be values in there if for instance
    // they got set from a restart file.
    scenario->mMarketplace->nullSuppliesAndDemands( aPeriod );

    scenario->mWorld->calc( aPeriod ); // call to calculate initial supply and demand
    mIsMidPeriod = true;
        }

      void runPeriodPost(const int aPeriod) {
          bool success = scenario->solve( aPeriod ); // solution uses Bisect and NR routine to clear markets

    scenario->mWorld->postCalc( aPeriod );

    // Mark that the period is now valid.
    scenario->mIsValidPeriod[ aPeriod ] = true;

    // Run the climate model for this period (only if the solver is successful)
    if( !success ) {
        ILogger& climatelog = ILogger::getLogger( "climate-log" );
        climatelog.setLevel( ILogger::WARNING );
        climatelog << "Solver unsuccessful for period " << aPeriod << "." << endl;
    }
    scenario->mWorld->runClimateModel( aPeriod );

    // Call any model feedbacks now that we are done solving the current period and
    // the climate model has been run.
    for( auto modelFeedback : scenario->mModelFeedbacks ) {
        modelFeedback->calcFeedbacksAfterPeriod( scenario, scenario->mWorld->getClimateModel(), aPeriod );
    }

    scenario->logPeriodEnding( aPeriod );

    // Write out the results for debugging.
    /*
    if( aPrintDebugging ){
        scenario->writeDebuggingFiles( aXMLDebugFile, aTabs, aPeriod );
    }
    */

    delete scenario->mManageStateVars;
    scenario->mManageStateVars = 0;
    mIsMidPeriod = false;
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

      SolutionDebugger createSolutionDebugger(const int aPeriod, const std::string& aMarketFilterStr) {
          int period = aPeriod;
        if(!mIsMidPeriod) {
            delete scenario->mManageStateVars;
            scenario->mManageStateVars = new ManageStateVariables(aPeriod);
        } else if(aPeriod != mCurrentPeriod) {
            Interp::warning("Solution debugger can only be created for current period "+util::toString(mCurrentPeriod)+" when running feedbacks.");
            period = mCurrentPeriod;
        }
        return SolutionDebugger::createInstance(period, aMarketFilterStr);
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

      std::string printXMLDB(const std::string& aXMLDBName) const {
          if(!isInitialized) {
              Interp::stop("GCAM did not successfully initialize.");
          }
          const std::string XMLDB_KEY = "xmldb-location";
          Configuration* conf = Configuration::getInstance();
          bool origShouldWrite = conf->shouldWriteFile(XMLDB_KEY);
          bool origAppendScnName = conf->shouldAppendScnToFile(XMLDB_KEY);
          std::string origXMLDBName = conf->getFile(XMLDB_KEY, XMLDB_KEY, false);
          // override the config to always write
          conf->mShouldWriteFileMap[XMLDB_KEY] = true;
          if(!aXMLDBName.empty()) {
              // override the config to not adjust the XMLDB name given
              conf->mShouldAppendScnFileMap[XMLDB_KEY] = false;
              conf->fileMap[XMLDB_KEY] = aXMLDBName;
          }
          XMLDBOutputter xmldb;
          scenario->accept(&xmldb, -1);
          xmldb.finish();
          xmldb.finalizeAndClose();
          if(!aXMLDBName.empty()) {
              // reset original settings
              conf->mShouldWriteFileMap[XMLDB_KEY] = origShouldWrite;
              conf->mShouldAppendScnFileMap[XMLDB_KEY] = origAppendScnName;
              conf->fileMap[XMLDB_KEY] = origXMLDBName;
          }
          return !aXMLDBName.empty() ? aXMLDBName :
              origAppendScnName ? origXMLDBName.append(scenario->getName()) :
              origXMLDBName;
      }

      std::string getScenarioName() const {
          return scenario->getName();
      }

      void setScenarioName(const std::string& aName) {
          scenario->setName(aName);
      }

    private:
        bool isInitialized;
        std::streambuf* mCoutOrig;
        int mCurrentPeriod;
        bool mIsMidPeriod;
        LoggerFactoryWrapper loggerFactoryWrapper;
        unique_ptr<IScenarioRunner> runner;
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

        .method("run_period",        &gcam::runPeriod,         "run to model period")
        .method("run_period_pre",        &gcam::runPeriodPre,         "run to model period pre solve")
        .method("run_period_post",        &gcam::runPeriodPost,         "run to model period solve and post")
        .method("set_data", &gcam::setData, "set data")
        .method("get_data", &gcam::getData, "get data")
        .method("create_solution_debugger", &gcam::createSolutionDebugger, "create solution debugger")
        .method("get_current_period", &gcam::getCurrentPeriod, "get the last run model period")
        .method("convert_period_to_year", &gcam::convertPeriodToYear, "convert a GCAM model period to year")
        .method("convert_year_to_period", &gcam::convertYearToPeriod, "convert a GCAM model year to model period")
        .method("print_xmldb", &gcam::printXMLDB, "write the full XML Database results")
        .method("set_scenario_name", &gcam::setScenarioName, "set scenario name")
        .method("get_scenario_name", &gcam::getScenarioName, "get scenario name")
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
  .method("reset_scales", &SolutionDebugger::resetScales, "resetScales")
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
