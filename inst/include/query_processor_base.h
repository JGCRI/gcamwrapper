#ifndef __QUERY_PROCESSOR_BASE_H__
#define __QUERY_PROCESSOR_BASE_H__

#include <string>
#include <vector>

struct FilterStep;
class AMatchesValue;

/*!
 * \brief The base class for the Get and Set Data query processor
 *        helper classes.
 * \details Really this class provides the extended GCAM Fusion query parsing
 *          which is really similar between get and set data although not exactly
 *          the same, for instance:  The standard GCAM Fusion filter syntax applies
 *          equally to both, such as: `[NamedFilter,RegexEquals,Africa]` however
 *          when the "extended" syntax (including the `+` at the start) the behavior
 *          is slightly different.  For Get Data, almost the full syntax:
 *          `[+NamedFilter,RegexEquals,Africa]` is required however the actual value
 *          that matches will be recorded into a DataFrame.  For Set Data the syntax
 *          only requires `[+NamedFilter,RegexEquals]` and the value to match against
 *          will be read out of a DataFrame, row by row.
 */
class QueryProcessorBase {
public:
  QueryProcessorBase() {}
  virtual ~QueryProcessorBase();
protected:
  //! The GCAM Fusion Filter Steps which is a fully parsed query
  std::vector<FilterStep*> mFilterSteps;

  //! The column name corresponding to the data to be get/set
  std::string mDataColName;

  void parseFilterString(const std::string& aFilterStr );

  FilterStep* parseFilterStepStr( const std::string& aFilterStepStr, int& aCol );

  AMatchesValue* createMatchesAny() const;

  virtual AMatchesValue* parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const;

  virtual AMatchesValue* wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt);
};

#endif // __QUERY_PROCESSOR_BASE_H__
