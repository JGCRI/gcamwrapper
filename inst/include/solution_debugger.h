#ifndef __SOLUTION_DEBUGGER_H__
#define __SOLUTION_DEBUGGER_H__

#include "interp_interface.h"

#include "solution/util/include/solution_info_set.h"
#include "solution/util/include/edfun.hpp"
#include "solution/util/include/ublas-helpers.hpp"

class World;
class Marketplace;

class SolutionDebugger {
public:
  static SolutionDebugger createInstance(const int aPeriod, const std::string& aMarketFilterStr);

  SolutionDebugger(World *w, Marketplace *m, SolutionInfoSet &sisin, int per);

  Interp::StringVector getMarketNames();

  Interp::NumericVector getPrices(const bool aScaled);

  Interp::NumericVector getFX();

  Interp::NumericVector getSupply(const bool aScaled);

  Interp::NumericVector getDemand(const bool aScaled);

  Interp::NumericVector getPriceScaleFactor();

  Interp::NumericVector getQuantityScaleFactor();

  void setPrices(const Interp::NumericVector& aPrices, const bool aScaled);
#ifdef IS_INTERP_PYTHON
  inline void setPrices_wrap(const boost::python::numpy::ndarray& aPrices, const bool aScaled) {
      setPrices(aPrices, aScaled);
  }
#endif

  Interp::NumericVector evaluate(const Interp::NumericVector& aPrices, const bool aScaled, const bool aResetAfterCalc);
#ifdef IS_INTERP_PYTHON
  inline Interp::NumericVector evaluate_wrap(const boost::python::numpy::ndarray& aPrices, const bool aScaled, const bool aResetAfterCalc) {
      return evaluate(aPrices, aScaled, aResetAfterCalc);
  }
#endif

  Interp::NumericVector evaluatePartial(const double aPrice, const int aIndex, const bool aScaled);

  Interp::NumericMatrix calcDerivative();

  Interp::NumericVector getSlope();

  void setSlope(const Interp::NumericVector& aDX);
#ifdef IS_INTERP_PYTHON
  inline void setSlope_wrap(const boost::python::numpy::ndarray& aDX) {
      setSlope(aDX);
  }
#endif

  void resetScales(const Interp::NumericVector& aPriceScale, const Interp::NumericVector& aQuantityScale);

private:

  World* world;
  Marketplace* marketplace;
  SolutionInfoSet solnInfoSet;
  int period;
  unsigned int nsolv;
  LogEDFun F;
  UBVECTOR x;
  UBVECTOR fx;
  Interp::StringVector marketNames;
};

#endif // __SOLUTION_DEBUGGER_H__
