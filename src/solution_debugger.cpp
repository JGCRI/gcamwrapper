
#include "solution_debugger.h"

#include "containers/include/world.h"
#include "solution/util/include/solution_info.h"
#include "solution/util/include/solvable_solution_info_filter.h"
#include "solution/util/include/solution_info_param_parser.h"
#include "marketplace/include/marketplace.h"
#include "solution/util/include/fdjac.hpp"
#include "util/base/include/manage_state_variables.hpp"

using namespace Interp;

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
  marketNames(createVector<std::string, StringVector>(nsolv)),
  priceScaleFactor(createVector<double, NumericVector>(nsolv)),
  quantityScaleFactor(createVector<double, NumericVector>(nsolv))
{
  std::vector<SolutionInfo> smkts(solnInfoSet.getSolvableSet());
  for(unsigned int i = 0; i < smkts.size(); ++i) {
    x[i] = smkts[i].getPrice();
    marketNames[i] = smkts[i].getName().c_str();
    priceScaleFactor[i] = std::max(smkts[i].getForecastPrice(), 0.001);
    quantityScaleFactor[i] = std::abs(smkts[i].getForecastDemand());
  }
  setVectorNames(priceScaleFactor, marketNames);
  setVectorNames(quantityScaleFactor, marketNames);

  F.scaleInitInputs(x);
  F(x, fx);
}

StringVector SolutionDebugger::getMarketNames() {
    return marketNames;
}

NumericVector SolutionDebugger::getPrices(const bool aScaled) {
  NumericVector ret(createVector<double, NumericVector>(nsolv));
  for(int i = 0; i < nsolv; ++i) {
      double val = x[i];
      if(!aScaled) {
          val *= priceScaleFactor[i];
      }
      ret[i] = val;
  }
  setVectorNames(ret, marketNames);
  return ret;
}

NumericVector SolutionDebugger::getFX() {
  NumericVector ret(createVector<double, NumericVector>(nsolv));
  std::copy(fx.begin(), fx.end(), &ret[0]);
  setVectorNames(ret, marketNames);
  return ret;
}

NumericVector SolutionDebugger::getSupply(const bool aScaled) {
  NumericVector ret(createVector<double, NumericVector>(nsolv));
  for(int i = 0; i < nsolv; ++i) {
      double val = solnInfoSet.getSolvable(i).getSupply();
      if(aScaled) {
          val /= quantityScaleFactor[i];
      }
    ret[i] = val;
  }
  setVectorNames(ret, marketNames);
  return ret;
}

NumericVector SolutionDebugger::getDemand(const bool aScaled) {
  NumericVector ret(createVector<double, NumericVector>(nsolv));
  for(int i = 0; i < nsolv; ++i) {
      double val = solnInfoSet.getSolvable(i).getDemand();
      if(aScaled) {
          val /= quantityScaleFactor[i];
      }
    ret[i] = val;
  }
  Interp::setVectorNames(ret, marketNames);
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
  UBVECTOR x_restore;
  UBVECTOR fx_restore = fx;
  if(aResetAfterCalc) {
    x_restore = x;
    fx_restore = fx;
  }
  setPrices(aPrices, aScaled);

  if(aResetAfterCalc) {
    scenario->getManageStateVariables()->setPartialDeriv(true);
    F.partial(1);
  }
  F(x,fx);
  NumericVector fx_ret = getFX();
  if(aResetAfterCalc) {
    F.partial(-1);
    x = x_restore;
    fx = fx_restore;
  }

  return fx_ret;
}

NumericVector SolutionDebugger::evaluatePartial(const double aPrice, const int aIndex, const bool aScaled) {
  double x_restore = x[aIndex];
  x[aIndex] = aScaled ? aPrice : aPrice / priceScaleFactor[aIndex];
  scenario->getManageStateVariables()->setPartialDeriv(true);
  F.partial(aIndex);
  UBVECTOR fx_restore = fx;
  F(x,fx,aIndex);
  F.partial(-1);
  x[aIndex] = x_restore;
  NumericVector fx_ret = getFX();
  fx = fx_restore;

  return fx_ret;
}

NumericMatrix SolutionDebugger::calcDerivative() {
  std::list<int> indicies;
  for(int i = 0; i < nsolv; ++i) {
    indicies.push_back(i);
  }
  UBMATRIX jac(nsolv,nsolv);
  fdjac(F, x, fx, jac, indicies, true);
  NumericMatrix jacRet = wrapMatrix(jac, nsolv);
  Interp::setMatrixNames(jacRet, marketNames);

  return jacRet;
}

NumericVector SolutionDebugger::getSlope() {
  NumericVector slope(createVector<double, NumericVector>(nsolv));
  for(int i = 0; i < nsolv; ++i) {
    slope[i] = solnInfoSet.getSolvable(i).getCorrectionSlope(priceScaleFactor[i], 1.0/quantityScaleFactor[i]);
  }
  setVectorNames(slope, marketNames);
  return slope;
}

void SolutionDebugger::setSlope(const NumericVector& aDX) {
  UBVECTOR dx(nsolv);
  for(int i = 0; i < nsolv; ++i) {
    dx[i] = aDX[i];
  }
  F.setSlope(dx);
}

