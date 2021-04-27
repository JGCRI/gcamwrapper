#ifndef __QUERY_PROCESSOR_BASE_H__
#define __QUERY_PROCESSOR_BASE_H__

//#include "interp_interface.h"
#include <string>
#include <vector>

struct FilterStep;
class AMatchesValue;

class QueryProcessorBase {
public:
  QueryProcessorBase() {}
  virtual ~QueryProcessorBase();
protected:
  //std::vector<std::string> mColNames;
  //! The GCAM Fusion Filter Steps which is a fully parsed query
  std::vector<FilterStep*> mFilterSteps;

  std::string mDataColName;

  void parseFilterString(const std::string& aFilterStr );
  FilterStep* parseFilterStepStr( const std::string& aFilterStepStr, int& aCol );
  AMatchesValue* createMatchesAny() const;
  virtual AMatchesValue* parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const;
  virtual AMatchesValue* wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt);
};

#endif // __QUERY_PROCESSOR_BASE_H__
