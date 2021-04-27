#include "interp_interface.h"

#include "set_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Interp;

class StringVecEquals : public AMatchesValue {
public:
  StringVecEquals( const Interp::StringVector& aStr, const int& aRow ):mStr( aStr ), mRow( aRow ) {}
  virtual ~StringVecEquals() {}
  virtual bool matchesString( const std::string& aStrToTest ) const {
    return mStr[mRow] == aStrToTest;
  }
protected:
  const Interp::StringVector mStr;
  const int& mRow;
};
class StringVecRegexMatches : public StringVecEquals {
public:
  StringVecRegexMatches( const Interp::StringVector& aStr, const int& aRow ):StringVecEquals( aStr, aRow ) {}
  virtual bool matchesString( const std::string& aStrToTest ) const {
      // TODO: performance is likely terrible, might want to convert mStr to regex ahead of time
      std::regex matcher( Interp::extract(mStr[mRow]), std::regex::nosubs | std::regex::optimize | std::regex::egrep );
      return std::regex_search( aStrToTest, matcher );
  }
};

class IntVecEquals : public AMatchesValue {
public:
  IntVecEquals( const Interp::IntegerVector& aInt, const int& aRow ):mInt( aInt ), mRow( aRow ) {}
  virtual ~IntVecEquals() {}
  virtual bool matchesInt( const int aIntToTest ) const {
    return mInt[mRow] == aIntToTest;
  }
protected:
  const Interp::IntegerVector mInt;
  const int& mRow;
};
class IntVecGreaterThan: public IntVecEquals {
public:
  IntVecGreaterThan( const Interp::IntegerVector& aInt, const int& aRow ):IntVecEquals( aInt, aRow ) {}
  virtual bool matchesInt( const int aIntToTest ) const {
      return aIntToTest > mInt[mRow];
  }
};
class IntVecGreaterThanEq: public IntVecEquals {
public:
  IntVecGreaterThanEq( const Interp::IntegerVector& aInt, const int& aRow ):IntVecEquals( aInt, aRow ) {}
  virtual bool matchesInt( const int aIntToTest ) const {
      return aIntToTest >= mInt[mRow];
  }
};
class IntVecLessThan: public IntVecEquals {
public:
  IntVecLessThan( const Interp::IntegerVector& aInt, const int& aRow ):IntVecEquals( aInt, aRow ) {}
  virtual bool matchesInt( const int aIntToTest ) const {
      return aIntToTest < mInt[mRow];
  }
};
class IntVecLessThanEq: public IntVecEquals {
public:
  IntVecLessThanEq( const Interp::IntegerVector& aInt, const int& aRow ):IntVecEquals( aInt, aRow ) {}
  virtual bool matchesInt( const int aIntToTest ) const {
      return aIntToTest <= mInt[mRow];
  }
};

SetDataHelper::SetDataHelper(const Interp::DataFrame& aData, const std::string& aHeader):
    QueryProcessorBase(),
    mData(aData),
    mDataVector(Interp::getDataFrameAt<Interp::NumericVector>(aData, -1)),
    mRow(0)
{
    parseFilterString(aHeader);
}

void SetDataHelper::run(Scenario* aScenario) {
  GCAMFusion<SetDataHelper> fusion(*this, mFilterSteps);
  for(mRow = 0; mRow < getDataFrameNumRows(mData); ++mRow) {
    fusion.startFilter(aScenario);
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

AMatchesValue* SetDataHelper::parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const {
    // [0] = filter type (name, year, index)
    // [1] = match type
    // [2:] = match type options
    AMatchesValue* matcher = 0;
    if(!aIsRead) {
        matcher = QueryProcessorBase::parsePredicate(aFilterOptions, aCol, aIsRead);
    }
    else if( aFilterOptions[ 1 ] == "StringEquals" ) {
            matcher = new StringVecEquals(getDataFrameAt<StringVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "StringRegexMatches" ) {
            matcher = new StringVecRegexMatches(getDataFrameAt<StringVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "IntEquals" ) {
            matcher = new IntVecEquals(getDataFrameAt<IntegerVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "IntGreaterThan" ) {
            matcher = new IntVecGreaterThan(getDataFrameAt<IntegerVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "IntGreaterThanEq" ) {
            matcher = new IntVecGreaterThanEq(getDataFrameAt<IntegerVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "IntLessThan" ) {
            matcher = new IntVecLessThan(getDataFrameAt<IntegerVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "IntLessThanEq" ) {
            matcher = new IntVecLessThanEq(getDataFrameAt<IntegerVector>(mData, aCol), mRow);
        }
        else if( aFilterOptions[ 1 ] == "MatchesAny" ) {
            matcher = createMatchesAny();
        }
    else {
        Interp::stop("Unknown filter operand: " + aFilterOptions[ 1 ]);
    }

    return matcher;
}

