#ifndef __GET_DATA_HELPER_H__
#define __GET_DATA_HELPER_H__

#include "interp_interface.h"
#include "query_processor_base.h"
#include <string>
#include <vector>

class Scenario;
class AMatcherWrapper;

class GetDataHelper : public QueryProcessorBase {
public:
  GetDataHelper(const std::string& aHeader);
  Interp::DataFrame run( Scenario* aScenario);
  template<typename T>
  void processData(T& aData);
protected:
  std::vector<double> mDataVector;
  bool mHasYearInPath;
  //! Keep track of "+" filters which will be doing the recording
  std::vector<AMatcherWrapper*> mPathTracker;

  //std::vector<int> mYearVector;
  //std::vector<std::string> mColNames;
  //std::vector<AMatcherWrapper*> mPathTracker;
  //std::vector<FilterStep*> mFilterSteps;

  virtual AMatchesValue* wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt);
  template<typename VecType>
  void vectorDataHelper(VecType& aDataVec);
};

#endif // __GET_DATA_HELPER_H__
