#ifndef __SET_DATA_HELPER_H__
#define __SET_DATA_HELPER_H__

#include "interp_interface.h"
#include "query_processor_base.h"
#include <vector>
#include <string>

class Scenario;
struct FilterStep;

class SetDataHelper : public QueryProcessorBase {
public:
  SetDataHelper(const Interp::DataFrame& aData, const std::string& aHeader);
  void run( Scenario* aScenario);
  template<typename T>
  void processData(T& aData);
protected:
  //! The DataFrame containing the data we will use to look up values
  //! to filter against and data to set as we process row by row
  const Interp::DataFrame mData;

  //! The column from mData that contains the data to update in GCAM
  Interp::NumericVector mDataVector;

  //! The current row of mData which is being processed
  int mRow;

  virtual AMatchesValue* parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const;
};

#endif // __SET_DATA_HELPER_H__
