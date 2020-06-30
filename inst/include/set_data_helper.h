#ifndef __SET_DATA_HELPER_H__
#define __SET_DATA_HELPER_H__

#define STRICT_R_HEADERS
#include "Rcpp.h"
#include <vector>

class Scenario;
class FilterStep;

class RSetDataHelper {
public:
  RSetDataHelper(const Rcpp::DataFrame& aData, const Rcpp::String& aHeader):
      mData(aData),
      mRow(0),
      mDataVector(aData.at(aData.ncol()-1)),
      mFilterSteps(parseFilterString(aHeader))
  {
  }
  ~RSetDataHelper();
  void run( Scenario* aScenario);
  template<typename T>
  void processData(T& aData);
private:
  const Rcpp::DataFrame mData;
  const Rcpp::DoubleVector mDataVector;
  int mRow;
  std::vector<FilterStep*> mFilterSteps;

  std::vector<FilterStep*> parseFilterString(const std::string& aFilterStr );
  FilterStep* parseFilterStepStr( const std::string& aFilterStepStr, int& aCol );
};

#endif // __SET_DATA_HELPER_H__
