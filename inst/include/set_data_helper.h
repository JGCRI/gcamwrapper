#ifndef __SET_DATA_HELPER_H__
#define __SET_DATA_HELPER_H__

#include "interp_interface.h"
#include <vector>
#include <string>

class Scenario;
struct FilterStep;

class SetDataHelper {
public:
  SetDataHelper(const Interp::DataFrame& aData, const std::string& aHeader):
      mData(aData),
      mRow(0),
      mDataVector(Interp::getDataFrameAt<Interp::NumericVector>(aData, -1)),
      mFilterSteps(parseFilterString(aHeader))
  {
  }
  ~SetDataHelper();
  void run( Scenario* aScenario);
  template<typename T>
  void processData(T& aData);
private:
  const Interp::DataFrame mData;
  Interp::NumericVector mDataVector;
  int mRow;
  std::vector<FilterStep*> mFilterSteps;

  std::vector<FilterStep*> parseFilterString(const std::string& aFilterStr );
  FilterStep* parseFilterStepStr( const std::string& aFilterStepStr, int& aCol );
};

#endif // __SET_DATA_HELPER_H__
