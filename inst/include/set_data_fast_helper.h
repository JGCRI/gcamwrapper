#ifndef __SET_DATA_FAST_HELPER_H__
#define __SET_DATA_FAST_HELPER_H__

#include "interp_interface.h"
#include "query_processor_base.h"
#include <string>
#include <vector>
#include <map>
#include <list>

class Scenario;
class AMatcherHashWrapper;

/*!
 * \brief A GCAM Fusion class that will run arbitrary queries and organize the
 *        results into a DataFrame.
 * \details The query syntax is slightly modified from the standard GCAM Fusion
 *          syntax in that any filter step that starts with a `+` is interpereted
 *          to mean record the name/year of the CONTAINER as a column that will then
 *          be organized into a DataFrame.  Where each row of these columns then
 *          correspond with each value returned by the query result.  Note the DataFrame
 *          generated from this class will not be "aggregated" for unique identifying
 *          column combinations, that step will be left to be done in the interpreter.
 */
class SetDataFastHelper : public QueryProcessorBase {
public:
  SetDataFastHelper(const Interp::DataFrame& aData, const std::string& aQuery);

  void run( Scenario* aScenario );

  template<typename T>
  void processData(T& aData);
protected:
  Interp::DataFrame mData;

  //! Keep track of "+" filters which will be doing the recording
  std::vector<AMatcherHashWrapper*> mPathTracker;

  //! The data for the value column which match the query
  std::list<std::vector<size_t> > mTempDataID;
  std::map<size_t, double> mDataVector;

  virtual AMatchesValue* wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt);
  virtual AMatchesValue* parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const;

  template<typename VecType>
  void vectorDataHelper(VecType& aDataVec);
};

#endif // __SET_DATA_FAST_HELPER_H__
