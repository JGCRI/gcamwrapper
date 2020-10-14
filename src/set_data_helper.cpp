#include "interp_interface.h"

#include "set_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Interp;

class StringVecEquals : public AMatchesValue {
public:
  StringVecEquals( const Interp::StringVector& aStr, int& row ):mStr( aStr ), mRow( row ) {}
  virtual ~StringVecEquals() {}
  virtual bool matchesString( const std::string& aStrToTest ) const {
    return mStr[mRow] == aStrToTest;
  }
private:
  const Interp::StringVector mStr;
  int& mRow;
};

class IntVecEquals : public AMatchesValue {
public:
  IntVecEquals( const Interp::IntegerVector& aInt, int& row ):mInt( aInt ), mRow( row ) {}
  virtual ~IntVecEquals() {}
  virtual bool matchesInt( const int aIntToTest ) const {
    return mInt[mRow] == aIntToTest;
  }
private:
  const Interp::IntegerVector mInt;
  int& mRow;
};

void SetDataHelper::run(Scenario* aScenario) {
  GCAMFusion<SetDataHelper> fusion(*this, mFilterSteps);
  for(mRow = 0; mRow < getDataFrameNumRows(mData); ++mRow) {
    fusion.startFilter(aScenario);
  }
}

SetDataHelper::~SetDataHelper() {
  for(auto step : mFilterSteps) {
    delete step;
  }
}

template<>
void SetDataHelper::processData(double& aData) {
  aData = mDataVector[mRow];
}
template<>
void SetDataHelper::processData(Value& aData) {
  aData = mDataVector[mRow];
}
template<>
void SetDataHelper::processData(int& aData) {
  aData = mDataVector[mRow];
}
template<>
void SetDataHelper::processData(std::pair<unsigned int const, double>& aData) {
  aData.second = mDataVector[mRow];
}
template<typename T>
void SetDataHelper::processData(T& aData) {
  Interp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
}

FilterStep* SetDataHelper::parseFilterStepStr( const std::string& aFilterStepStr, int& aCol ) {
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

    AMatchesValue* matcher = 0;
    FilterStep* filterStep = 0;
    if( filterStr == "name" ) {
      matcher = new StringVecEquals( getDataFrameAt(mData, aCol), mRow );
      filterStep = new FilterStep( dataName, new NamedFilter( matcher ) );
    }
    else if( filterStr == "year" ) {
      matcher = new IntVecEquals( getDataFrameAt(mData, aCol), mRow );
      filterStep = new FilterStep( dataName, new YearFilter( matcher ) );
    }
    else {
      ILogger& mainLog = ILogger::getLogger( "main_log" );
      mainLog.setLevel( ILogger::WARNING );
      mainLog << "Unknown subclass of AMatchesValue: " << filterStr << std::endl;
    }

    if(isRead) {
      ++aCol;
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
std::vector<FilterStep*> SetDataHelper::parseFilterString(const std::string& aFilterStr ) {
  std::vector<std::string> filterStepsStr;
  boost::split( filterStepsStr, aFilterStr, boost::is_any_of( "/" ) );
  std::vector<FilterStep*> filterSteps( filterStepsStr.size() );
  int col = 0;
  for( size_t i = 0; i < filterStepsStr.size(); ++i ) {
    filterSteps[ i ] = parseFilterStepStr( filterStepsStr[ i ], col );
  }
  return filterSteps;
}

