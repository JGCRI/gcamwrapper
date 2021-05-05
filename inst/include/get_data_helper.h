#ifndef __GET_DATA_HELPER_H__
#define __GET_DATA_HELPER_H__

#include "interp_interface.h"
#include "query_processor_base.h"
#include <string>
#include <vector>

class Scenario;
class AMatcherWrapper;

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
class GetDataHelper : public QueryProcessorBase {
public:
  GetDataHelper(const std::string& aQuery);

  Interp::DataFrame run( Scenario* aScenario);

  template<typename T>
  void processData(T& aData);
protected:
  //! Keep track of "+" filters which will be doing the recording
  std::vector<AMatcherWrapper*> mPathTracker;

  //! The data for the value column which match the query
  std::vector<double> mDataVector;

  //! A flag to help with the implicit behavior that if a user forgot
  //! to add year filter on a period vector of data we can implicitly
  //! add one that matches all for them
  bool mHasYearInPath;

  virtual AMatchesValue* wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt);
  template<typename VecType>
  void vectorDataHelper(VecType& aDataVec);
};

#endif // __GET_DATA_HELPER_H__
