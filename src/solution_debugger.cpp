
#include "solution_debugger.h"

#include "containers/include/world.h"
#include "solution/util/include/solution_info.h"
#include "solution/util/include/solvable_solution_info_filter.h"
#include "solution/util/include/solution_info_param_parser.h"
#include "marketplace/include/marketplace.h"
#include "solution/util/include/fdjac.hpp"

using namespace Rcpp;

SolutionDebugger SolutionDebugger::createInstance(const int aPeriod) {
  SolutionInfoSet solnInfoSet( scenario->getMarketplace() );
  SolutionInfoParamParser solnParams;
  solnInfoSet.init( aPeriod, 0.001, 0.001, &solnParams );

  return SolutionDebugger(scenario->getWorld(), scenario->getMarketplace(), solnInfoSet, aPeriod);
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
