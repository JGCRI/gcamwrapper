#include <unistd.h>
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

#include "set_data_helper.h"
#include "get_data_helper.h"

// solver debugger
#include "containers/include/world.h"
#include "solution/util/include/solution_info_set.h"
#include "solution/util/include/solution_info.h"
#include "solution/util/include/solvable_solution_info_filter.h"
#include "solution/util/include/solution_info_param_parser.h"
#include "marketplace/include/marketplace.h"
#include "solution/util/include/fdjac.hpp"
#include "solution/util/include/edfun.hpp"

using namespace std;
using namespace Rcpp;

// Declared outside Main to make global.
Scenario* scenario;

using UBVECTOR = boost::numeric::ublas::vector<double>;

class SolutionDebugger {
public:
  static SolutionDebugger createInstance(const int aPeriod);

  SolutionDebugger(World *w, Marketplace *m, SolutionInfoSet &sisin, int per);

  NumericVector getPrices(const bool aScaled);

  NumericVector getFX();

  NumericVector getSupply(const bool aScaled);

  NumericVector getDemand(const bool aScaled);

  NumericVector getPriceScaleFactor();

  NumericVector getQuantityScaleFactor();

  void setPrices(const NumericVector& aPrices, const bool aScaled);

  NumericVector evaluate(const NumericVector& aPrices, const bool aScaled, const bool aResetAfterCalc);

  NumericVector evaluatePartial(const double aPrice, const int aIndex, const bool aScaled);

  //NumericVector evaluatePartial(const double aPrice, const String& aIndex, const bool aScaled);

  NumericMatrix calcDerivative();

private:
  World* world;
  Marketplace* marketplace;
  SolutionInfoSet solnInfoSet;
  int period;
  unsigned int nsolv;
  LogEDFun F;
  UBVECTOR x;
  UBVECTOR fx;
  StringVector marketNames;
  NumericVector priceScaleFactor;
  NumericVector quantityScaleFactor;
};

class gcam {
    public:
        gcam():isInitialized(false) {
            int ret = chdir("exe");
            initializeScenario();
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
        void initializeScenario() {
            string configurationArg = "configuration.xml";
            string loggerFactoryArg = "log_conf.xml";
            // Parse any command line arguments.  Can override defaults with command lone args
            //parseArgs( argc, argv, configurationArg, loggerFactoryArg );

            // Add OS dependent prefixes to the arguments.
            const string configurationFileName = configurationArg;
            const string loggerFileName = loggerFactoryArg;

            // Initialize the timer.  Create an object of the Timer class.
            Timer timer;
            timer.start();


            // Initialize the LoggerFactory
            //sLoggerFactoryWrapper loggerFactoryWrapper;
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

SolutionDebugger SolutionDebugger::createInstance(const int aPeriod) {
  SolutionInfoSet solnInfoSet( scenario->getMarketplace() );
  SolutionInfoParamParser solnParams;
  solnInfoSet.init( aPeriod, 0.001, 0.001, &solnParams );

  return SolutionDebugger(scenario->getWorld(), scenario->getMarketplace(), solnInfoSet, aPeriod);
}

RCPP_EXPOSED_CLASS_NODECL(gcam)
RCPP_EXPOSED_CLASS_NODECL(SolutionDebugger)
RCPP_MODULE(gcam_module) {
    Rcpp::class_<gcam>("gcam")

        .constructor("constructor")

        .method("runToPeriod",        &gcam::runToPeriod,         "run to model period")
        .method("setData", &gcam::setData, "set data")
        .method("getData", &gcam::getData, "get data")
        .method("createSolutionDebugger", &gcam::createSolutionDebugger, "create solution debugger")
        ;

  Rcpp::class_<SolutionDebugger>("SolutionDebugger")
    //.factory<int>(createInstance)

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
  ;
}

SolutionDebugger::SolutionDebugger(World *w, Marketplace *m, SolutionInfoSet &sisin, int per):
  world(w),
  marketplace(m),
  solnInfoSet(sisin),
  period(per),
  nsolv(sisin.getNumSolvable()),
  F(solnInfoSet,w,m,per,false),
  x(nsolv),
  fx(nsolv),
  marketNames(nsolv),
  priceScaleFactor(nsolv),
  quantityScaleFactor(nsolv)
{
    std::vector<SolutionInfo> smkts(solnInfoSet.getSolvableSet());
    for(unsigned int i = 0; i < smkts.size(); ++i) {
      x[i] = smkts[i].getPrice();
      marketNames[i] = smkts[i].getName();
      priceScaleFactor[i] = std::max(smkts[i].getForecastPrice(), 0.001);
      quantityScaleFactor[i] = std::abs(smkts[i].getForecastDemand());
    }
    //marketNames.names() = marketNames;
    priceScaleFactor.names() = marketNames;
    quantityScaleFactor.names() = marketNames;

    F.scaleInitInputs(x);
    F(x, fx);
}

NumericVector SolutionDebugger::getPrices(const bool aScaled) {
  NumericVector ret(nsolv);
  ret.assign(x.begin(), x.end());
  ret.names() = marketNames;
  if(!aScaled) {
    ret = ret * priceScaleFactor;
  }
  return ret;
}

NumericVector SolutionDebugger::getFX() {
  NumericVector ret(nsolv);
  ret.assign(fx.begin(), fx.end());
  ret.names() = marketNames;
  return ret;
}

NumericVector SolutionDebugger::getSupply(const bool aScaled) {
  NumericVector ret(nsolv);
  for(int i = 0; i < nsolv; ++i) {
    ret[i] = solnInfoSet.getSolvable(i).getSupply();
  }
  ret.names() = marketNames;
  if(aScaled) {
    ret = ret / quantityScaleFactor;
  }
  return ret;
}

NumericVector SolutionDebugger::getDemand(const bool aScaled) {
  NumericVector ret(nsolv);
  for(int i = 0; i < nsolv; ++i) {
    ret[i] = solnInfoSet.getSolvable(i).getDemand();
  }
  ret.names() = marketNames;
  if(aScaled) {
    ret = ret / quantityScaleFactor;
  }
  return ret;
}

NumericVector SolutionDebugger::getPriceScaleFactor() {
  return priceScaleFactor;
}

NumericVector SolutionDebugger::getQuantityScaleFactor() {
  return quantityScaleFactor;
}

void SolutionDebugger::setPrices(const NumericVector& aPrices, const bool aScaled) {
  for(int i = 0; i < nsolv; ++i) {
    x[i] = aPrices[i];
  }
  if(!aScaled) {
    F.scaleInitInputs(x);
  }
}

NumericVector SolutionDebugger::evaluate(const NumericVector& aPrices, const bool aScaled, const bool aResetAfterCalc) {
  setPrices(aPrices, aScaled);

  if(aResetAfterCalc) {
    scenario->getManageStateVariables()->setPartialDeriv(true);
    F.partial(1);
  }
  F(x,fx);
  if(aResetAfterCalc) {
    F.partial(-1);
  }

  return getFX();
}

NumericVector SolutionDebugger::evaluatePartial(const double aPrice, const int aIndex, const bool aScaled) {
  x[aIndex] = aScaled ? aPrice : aPrice / priceScaleFactor[aIndex];
  scenario->getManageStateVariables()->setPartialDeriv(true);
  F.partial(aIndex);
  F(x,fx,aIndex);
  F.partial(-1);

  return getFX();
}

/*NumericVector SolutionDebugger::evaluatePartial(const double aPrice, const String& aIndex, const bool aScaled) {
  return evaluatePartial(aPrice, marketNames.findName(aIndex), aScaled);
}*/

NumericMatrix SolutionDebugger::calcDerivative() {
  using UBMATRIX = boost::numeric::ublas::matrix<double>;
  UBMATRIX jac(nsolv,nsolv);
  fdjac(F, x, fx, jac, true);
  NumericMatrix jacRet(nsolv, nsolv);
  for(int row = 0; row < nsolv; ++row) {
    for(int col = 0; col < nsolv; ++col) {
      jacRet.at(row, col) = jac(row, col);
    }
  }
  rownames(jacRet) = marketNames;
  colnames(jacRet) = marketNames;

  return jacRet;
}

