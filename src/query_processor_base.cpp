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
  for(auto step : mFilterSteps) {
    delete step;
  }
}

/*!
 * \brief A utility to create an instance of MatchesAny.
 * \details MatchesAny is an implementation of AMatchesValue which always
 *          returns true.  This can be useful to record all values in get
 *          data or to "skip" a column for set data.
 * \return A new instance of MatchesAny.
 */
AMatchesValue* QueryProcessorBase::createMatchesAny() const {
    return new MatchesAny();
}

/*!
 * \brief Parse individual components of a filter.
 * \details The base class implementation basically provides the standard GCAM
 *          fusion predicates.  However MatchesAny is also available here as a
 *          "match type" for the second option in aFilterOptions.
 * \param aFilterOptions The parsed components of a filter where the first value
 *                       sets the type (name, year, index), the second is the
 *                       predicate (equals, greater than, etc), and any other
 *                       values are additional arguments to the predicate.
 * \param aCol The current column number which should correspond to a DataFrame
 *             which is useful should this be a Set Data operation.
 * \param aIsRead If the `+` option was specified in which case the specialized
 *                Get / Set Data behavior will be enabled.
 * \return The corresponding AMatchesValue instance for the options given.
 */
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

/*!
 * \brief Wraps the given predicate with specialized Get / Set Data behavior.
 * \details The base class implementation returns the predicate unwrapped.
 * \param aToWrap The predicate to wrap.
 * \param aDataName The data name which may be useful to create a mapping to column names.
 * \param aIsInt To help decide if we should apply an int or string based wrapper.
 * \return An instance of AMatchesValue which may be wrapped by an AMatcherWrapper to enable
 *         additional access to record matching data as we query.
 */
AMatchesValue* QueryProcessorBase::wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt) {
    // do nothing by default
    return aToWrap;
}

/*!
 * \brief Parses a single filter step.
 * \details Follows the same basic processing as the standard GCAM Fusion 
 *          implementation of parseFilterStepStr however adds some additional
 *          support for the `+` extension to map filters to a get / set operation.
 * \param aFilterStepStr The unparsed filter step string.
 * \param aCol The current column number which should correspond to a DataFrame
 *             which is useful should this be a Set Data operation.
 * \return A FilterStep which is the parsed representation of the given aFilterStepStr.
 */
FilterStep* QueryProcessorBase::parseFilterStepStr( const std::string& aFilterStepStr, int& aCol ) {
    auto openBracketIter = std::find( aFilterStepStr.begin(), aFilterStepStr.end(), '[' );
    if( openBracketIter == aFilterStepStr.end() ) {
        // no filter just the data name
        return new FilterStep( aFilterStepStr );
    }
    else {
        // everything up to the open bracket is the data name
        std::string dataName( aFilterStepStr.begin(), openBracketIter );
        // if the first character is `+` that means we need to enable special
        // get / set functionality
        bool isRead = *(openBracketIter + 1) == '+';
        // parse out the individual components of the filter into filter options by
        // spliting on `,`
        int filterOffset = isRead ? 2 : 1;
        std::string filterStr( openBracketIter + filterOffset, std::find( openBracketIter, aFilterStepStr.end(), ']' ) );
        std::vector<std::string> filterOptions;
        boost::split( filterOptions, filterStr, boost::is_any_of( "," ) );
        // parse the filter options into an AMatchesValue
        AMatchesValue* matcher = parsePredicate( filterOptions, aCol, isRead );
        if(isRead) {
            // the column would have just gotten saved if this was a set data
            // operation so we can increment to the next columh of the DataFrame
            // now
            ++aCol;
        }

        // set the filter type and wrap the predicate to enable special get data
        // functionality if the `+` option was set
        FilterStep* filterStep = 0;
        if( filterOptions[ 0 ] == "IndexFilter" ) {
            if(isRead) {
                matcher = wrapPredicate(matcher, "index", true);
            }

            filterStep = new FilterStep( dataName, new IndexFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "NamedFilter" ) {
            if(isRead) {
                matcher = wrapPredicate(matcher, dataName, false);
            }

            filterStep = new FilterStep( dataName, new NamedFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "YearFilter" ) {
            if(isRead) {
                matcher = wrapPredicate(matcher, "year", true);
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

