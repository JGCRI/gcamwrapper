#include "interp_interface.h"

#include "query_processor_base.h"

#include "util/base/include/gcam_fusion.hpp"

using namespace std;
using namespace Interp;

/*!
 * \brief An implementation of AMatchesValue that always returns true.
 * \details This becomes useful to pair with AMatcherWrapper so it can
 *          be used to record data as the query finds results.
 */
class MatchesAny : public AMatchesValue {
public:
    virtual bool matchesString( const std::string& aStrToTest ) const {
        return true;
    }
    virtual bool matchesInt( const int aIntToTest ) const {
        return true;
    }
};

QueryProcessorBase::~QueryProcessorBase() {
    // note mPathTracker's memory is managed by mFilterSteps
  for(auto step : mFilterSteps) {
    delete step;
  }
}

AMatchesValue* QueryProcessorBase::createMatchesAny() const {
    return new MatchesAny();
}

AMatchesValue* QueryProcessorBase::parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const {
    // [0] = filter type (name, year, index)
    // [1] = match type
    // [2:] = match type options
    AMatchesValue* matcher = 0;
        if( aFilterOptions[ 1 ] == "StringEquals" ) {
            matcher = new StringEquals( aFilterOptions[ 2 ] );
        }
        else if( aFilterOptions[ 1 ] == "StringRegexMatches" ) {
            matcher = new StringRegexMatches( aFilterOptions[ 2 ] );
        }
        else if( aFilterOptions[ 1 ] == "IntEquals" ) {
            matcher = new IntEquals( boost::lexical_cast<int>( aFilterOptions[ 2 ] ) );
        }
        else if( aFilterOptions[ 1 ] == "IntGreaterThan" ) {
            matcher = new IntGreaterThan( boost::lexical_cast<int>( aFilterOptions[ 2 ] ) );
        }
        else if( aFilterOptions[ 1 ] == "IntGreaterThanEq" ) {
            matcher = new IntGreaterThanEq( boost::lexical_cast<int>( aFilterOptions[ 2 ] ) );
        }
        else if( aFilterOptions[ 1 ] == "IntLessThan" ) {
            matcher = new IntLessThan( boost::lexical_cast<int>( aFilterOptions[ 2 ] ) );
        }
        else if( aFilterOptions[ 1 ] == "IntLessThanEq" ) {
            matcher = new IntLessThanEq( boost::lexical_cast<int>( aFilterOptions[ 2 ] ) );
        }
        else if( aFilterOptions[ 1 ] == "MatchesAny" ) {
            matcher = new MatchesAny();
        }
    else {
        Interp::stop("Unknown filter operand: " + aFilterOptions[ 1 ]);
    }

    return matcher;
}

AMatchesValue* QueryProcessorBase::wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt) {
    // do nothing by default
    return aToWrap;
}

FilterStep* QueryProcessorBase::parseFilterStepStr( const std::string& aFilterStepStr, int& aCol ) {
  auto openBracketIter = std::find( aFilterStepStr.begin(), aFilterStepStr.end(), '[' );
  if( openBracketIter == aFilterStepStr.end() ) {
    // no filter just the data name
    return new FilterStep( aFilterStepStr );
  }
  else {
    std::string dataName( aFilterStepStr.begin(), openBracketIter );
    bool isRead = *(openBracketIter + 1) == '+';
    int filterOffset = isRead ? 2 : 1;
    std::string filterStr( openBracketIter + filterOffset, std::find( openBracketIter, aFilterStepStr.end(), ']' ) );
    std::vector<std::string> filterOptions;
    boost::split( filterOptions, filterStr, boost::is_any_of( "," ) );
    AMatchesValue* matcher = parsePredicate( filterOptions, aCol, isRead );
    if(isRead) {
        ++aCol;
    }

    FilterStep* filterStep = 0;
        if( filterOptions[ 0 ] == "IndexFilter" ) {
            if(isRead) {
                //AMatcherWrapper* wrap = new IntMatcherWrapper(matcher, "index");
                AMatchesValue* wrap = wrapPredicate(matcher, "index", true);
                //mPathTracker.push_back(wrap);
                //mColNames.push_back("index");
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new IndexFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "NamedFilter" ) {
            if(isRead) {
                //AMatcherWrapper* wrap = new StrMatcherWrapper(matcher, dataName);
                AMatchesValue* wrap = wrapPredicate(matcher, dataName, false);
                //mColNames.push_back(dataName);
                //mPathTracker.push_back(wrap);
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new NamedFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "YearFilter" ) {
            if(isRead) {
                //AMatcherWrapper* wrap = new IntMatcherWrapper(matcher, "year");
                AMatchesValue* wrap = wrapPredicate(matcher, "year", true);
                //mColNames.push_back("year");
                //mPathTracker.push_back(wrap);
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new YearFilter( matcher ) );
        }
        else {
            Interp::stop("Unknown filter attribute: " + filterOptions[ 0 ]);
        }


    return filterStep;
  }
}

/*!
 * \brief Parse a string to create a list of FilterSteps.
 * \details The string is split on the '/' character so that the contents of each is
 *          assumed to be one FilterStep definition.  Each split string is therefore
 *          parsed further using the helper function parseFilterStepStr.
 * \param aFilterStr A string representing a series of FilterSteps.
 * \return A list of FilterSteps parsed from aFilterStr as detailed above.
 */
void QueryProcessorBase::parseFilterString(const std::string& aFilterStr ) {
  std::vector<std::string> filterStepsStr;
  boost::split( filterStepsStr, aFilterStr, boost::is_any_of( "/" ) );
  std::vector<FilterStep*> filterSteps( filterStepsStr.size() );
  mFilterSteps.resize(filterStepsStr.size());
  int col = 0;
  for( size_t i = 0; i < filterStepsStr.size(); ++i ) {
    mFilterSteps[ i ] = parseFilterStepStr( filterStepsStr[ i ], col );
  }
  mDataColName = mFilterSteps.back()->mDataName;
}
