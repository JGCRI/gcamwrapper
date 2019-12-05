#include "Rcpp.h"

#include "get_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Rcpp;

class MatchesAny : public AMatchesValue {
public:
    virtual bool matchesString( const std::string& aStrToTest ) const {
        return true;
    }
    virtual bool matchesInt( const int aIntToTest ) const {
        return true;
    }
};

class AMatcherWrapper : public AMatchesValue {
public:
    AMatcherWrapper(AMatchesValue* aToWrap):mToWrap(aToWrap)
    {
    }
    virtual ~AMatcherWrapper() {
        delete mToWrap;
    }
    virtual void recordPath() = 0;
    virtual void updateList(List& aList, size_t aCol) const = 0;

protected:
    AMatchesValue* mToWrap;
};

class StrMatcherWrapper : public AMatcherWrapper {
public:
  StrMatcherWrapper(AMatchesValue* aToWrap):AMatcherWrapper(aToWrap)
  {
  }
    virtual bool matchesString( const std::string& aStrToTest ) const {
        bool matches = mToWrap->matchesString(aStrToTest);
        if(matches) {
          const_cast<StrMatcherWrapper*>(this)->mCurrValue = aStrToTest;
        }
        return matches;
    }
    virtual void recordPath() {
        mData.push_back(mCurrValue);
    }
    virtual void updateList(List& aList, size_t aCol) const {
        aList[aCol] = Rcpp::wrap(mData);
    }
    private:
    string mCurrValue;
    vector<string> mData;
};

class IntMatcherWrapper : public AMatcherWrapper {
public:
  IntMatcherWrapper(AMatchesValue* aToWrap):AMatcherWrapper(aToWrap)
  {
  }
    virtual bool matchesInt( const int aIntToTest ) const {
        bool matches = mToWrap->matchesInt(aIntToTest);
        if(matches) {
            const_cast<IntMatcherWrapper*>(this)->mCurrValue = aIntToTest;
        }
        return matches;
    }
    virtual void recordPath() {
        mData.push_back(mCurrValue);
    }
    virtual void updateList(List& aList, size_t aCol) const {
        aList[aCol] = Rcpp::wrap(mData);
    }
    private:
    int mCurrValue;
    vector<int> mData;
};

List RGetDataHelper::run(Scenario* aScenario) {
  GCAMFusion<RGetDataHelper> fusion(*this, mFilterSteps);
  fusion.startFilter(aScenario);
  size_t nCol = mYearVector.empty() ? mPathTracker.size() + 1 : mPathTracker.size() + 2;
  List ret(nCol);
  ret.attr("class") = "data.frame";
  ret.attr("names") = mColNames;
  Rcpp::IntegerVector rnms(mDataVector.size()); std::iota(rnms.begin(), rnms.end(), 1);
  ret.attr("row.names") = rnms;
  size_t i = 0;
  for(i = 0; i < mPathTracker.size(); ++i) {
      mPathTracker[i]->updateList(ret, i);
  }
  ret[i++] = Rcpp::wrap(mDataVector);
  if(!mYearVector.empty()) {
      ret[i] = Rcpp::wrap(mYearVector);
  }
  return ret;
}

RGetDataHelper::~RGetDataHelper() {
    // note mPathTracker's memory is managed by mFilterSteps
  for(auto step : mFilterSteps) {
    delete step;
  }
}

template<>
void RGetDataHelper::processData(double& aData) {
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void RGetDataHelper::processData(Value& aData) {
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void RGetDataHelper::processData(int& aData) {
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void RGetDataHelper::processData(std::vector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(std::vector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(std::vector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::PeriodVector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::PeriodVector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::PeriodVector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::TechVintageVector<int>& aData) {
  vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::TechVintageVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::TechVintageVector<Value>& aData) {
  vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(objects::YearVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void RGetDataHelper::processData(std::map<unsigned int, double>& aData) {
  if(mYearVector.empty()) {
    mColNames.push_back("year");
  }
  for(auto iter = aData.begin(); iter != aData.end(); ++iter) {
    mYearVector.push_back(GetIndexAsYear::convertIterToYear(aData, iter));
    processData((*iter).second);
  }
}
template<typename VecType>
void RGetDataHelper::vectorDataHelper(VecType& aDataVec) {
    if(mYearVector.empty()) {
        mColNames.push_back("year");
    }
    for(auto iter = aDataVec.begin(); iter != aDataVec.end(); ++iter) {
        mYearVector.push_back(GetIndexAsYear::convertIterToYear(aDataVec, iter));
        processData(*iter);
    }
}

template<typename T>
void RGetDataHelper::processData(T& aData) {
  Rcpp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
}

FilterStep* RGetDataHelper::parseFilterStepStr( const std::string& aFilterStepStr, int& aCol ) {
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
    // [0] = filter type (name, year, index)
    // [1] = match type
    // [2:] = match type options
    AMatchesValue* matcher = 0;
        if( filterOptions[ 1 ] == "StringEquals" ) {
            matcher = new StringEquals( filterOptions[ 2 ] );
        }
        else if( filterOptions[ 1 ] == "StringRegexMatches" ) {
            matcher = new StringRegexMatches( filterOptions[ 2 ] );
        }
        else if( filterOptions[ 1 ] == "IntEquals" ) {
            matcher = new IntEquals( boost::lexical_cast<int>( filterOptions[ 2 ] ) );
        }
        else if( filterOptions[ 1 ] == "IntGreaterThan" ) {
            matcher = new IntGreaterThan( boost::lexical_cast<int>( filterOptions[ 2 ] ) );
        }
        else if( filterOptions[ 1 ] == "IntGreaterThanEq" ) {
            matcher = new IntGreaterThanEq( boost::lexical_cast<int>( filterOptions[ 2 ] ) );
        }
        else if( filterOptions[ 1 ] == "IntLessThan" ) {
            matcher = new IntLessThan( boost::lexical_cast<int>( filterOptions[ 2 ] ) );
        }
        else if( filterOptions[ 1 ] == "IntLessThanEq" ) {
            matcher = new IntLessThanEq( boost::lexical_cast<int>( filterOptions[ 2 ] ) );
        }
        else if( filterOptions[ 1 ] == "MatchesAny" ) {
            matcher = new MatchesAny();
        }
    else {
      ILogger& mainLog = ILogger::getLogger( "main_log" );
      mainLog.setLevel( ILogger::WARNING );
      mainLog << "Unknown subclass of AMatchesValue: " << filterStr << std::endl;
    }

    FilterStep* filterStep = 0;
        if( filterOptions[ 0 ] == "IndexFilter" ) {
            if(isRead) {
                AMatcherWrapper* wrap = new IntMatcherWrapper(matcher);
                mPathTracker.push_back(wrap);
                mColNames.push_back("index");
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new IndexFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "NamedFilter" ) {
            if(isRead) {
                AMatcherWrapper* wrap = new StrMatcherWrapper(matcher);
                mColNames.push_back(dataName);
                mPathTracker.push_back(wrap);
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new NamedFilter( matcher ) );
        }
        else if( filterOptions[ 0 ] == "YearFilter" ) {
            if(isRead) {
                AMatcherWrapper* wrap = new IntMatcherWrapper(matcher);
                mColNames.push_back("year");
                mPathTracker.push_back(wrap);
                matcher = wrap;
            }

            filterStep = new FilterStep( dataName, new YearFilter( matcher ) );
        }
        else {
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );
            mainLog << "Unknown filter: " << filterOptions[ 0 ] << std::endl;
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
void RGetDataHelper::parseFilterString(const std::string& aFilterStr ) {
  std::vector<std::string> filterStepsStr;
  boost::split( filterStepsStr, aFilterStr, boost::is_any_of( "/" ) );
  std::vector<FilterStep*> filterSteps( filterStepsStr.size() );
  mFilterSteps.resize(filterStepsStr.size());
  int col = 0;
  for( size_t i = 0; i < filterStepsStr.size(); ++i ) {
    mFilterSteps[ i ] = parseFilterStepStr( filterStepsStr[ i ], col );
  }
  mColNames.push_back(mFilterSteps.back()->mDataName);
}
