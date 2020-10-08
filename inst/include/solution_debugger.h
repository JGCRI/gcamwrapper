#ifndef __SOLUTION_DEBUGGER_H__
#define __SOLUTION_DEBUGGER_H__

#define STRICT_R_HEADERS
#include "Rcpp.h"

#include "solution/util/include/solution_info_set.h"
#include "solution/util/include/edfun.hpp"
#include <Eigen/Core>

class World;
class Marketplace;

class SolutionDebugger {
public:
  static SolutionDebugger createInstance(const int aPeriod);

  SolutionDebugger(World *w, Marketplace *m, SolutionInfoSet &sisin, int per);

  Rcpp::NumericVector getPrices(const bool aScaled);

  Rcpp::NumericVector getFX();

  Rcpp::NumericVector getSupply(const bool aScaled);

  Rcpp::NumericVector getDemand(const bool aScaled);

  Rcpp::NumericVector getPriceScaleFactor();

  Rcpp::NumericVector getQuantityScaleFactor();

  void setPrices(const Rcpp::NumericVector& aPrices, const bool aScaled);

  Rcpp::NumericVector evaluate(const Rcpp::NumericVector& aPrices, const bool aScaled, const bool aResetAfterCalc);

  Rcpp::NumericVector evaluatePartial(const double aPrice, const int aIndex, const bool aScaled);

  Rcpp::NumericMatrix calcDerivative();

  Rcpp::NumericVector getSlope();

  void setSlope(const Rcpp::NumericVector& aDX);

private:
  //using UBVECTOR = Eigen::VectorXd;
  using UBVECTOR = boost::numeric::ublas::vector<double>;

  World* world;
  Marketplace* marketplace;
  SolutionInfoSet solnInfoSet;
  int period;
  unsigned int nsolv;
  LogEDFun F;
  UBVECTOR x;
  UBVECTOR fx;
  Rcpp::StringVector marketNames;
  Rcpp::NumericVector priceScaleFactor;
  Rcpp::NumericVector quantityScaleFactor;
};

#endif // __SOLUTION_DEBUGGER_H__
