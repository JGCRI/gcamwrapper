#include "interp_interface.h"

#include "set_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Interp;

// Implementations of all the standard GCAM Fusion predicates however with the
// value to be compared against comping from a vector instead of specified explicitly

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

/*!
 * \brief Prepare to run the given query.
 * \param aData The DataFrame to read name/year values to compare against,
 *              as well as the values to set.
 * \param aQuery The GCAM Fusion query to be parsed
 */
SetDataHelper::SetDataHelper(const Interp::DataFrame& aData, const std::string& aHeader):
    QueryProcessorBase(),
    mData(aData),
    mDataVector(Interp::getDataFrameAt<Interp::NumericVector>(aData, -1)),
    mRow(0)
{
    parseFilterString(aHeader);
}

/*!
 * \brief Run the query against the given Scenario context and
 *        set the data processing the DataFrame row by row.
 * \param aScenario The Scenario object which will serve as the
 *                  query context from which to evaluate the query.
 */
void SetDataHelper::run(Scenario* aScenario) {
  GCAMFusion<SetDataHelper> fusion(*this, mFilterSteps);
  for(mRow = 0; mRow < getDataFrameNumRows(mData); ++mRow) {
    fusion.startFilter(aScenario);
  }
}

/*!
 * \brief Parse individual components of a filter.
 * \details If aIsRead is set we return a "vectorized" implementation of the
 *          requested predicate and the value to be compared against will come
 *          from the columns of mData.  If not the base class implementation is
 *          called instead.
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
AMatchesValue* SetDataHelper::parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const {
    // [0] = filter type (name, year, index)
    // [1] = match type
    // [2:] = match type options (ignored if aIsRead)
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

// GCAM Fusion callbacks to set the data with specializations for all of the
// types that we support:

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

struct GrHelper {
  const int mPeriod;
  const std::string mGRName;
  const Rcpp::IntegerVector iterVec;
  const Rcpp::List ldc_iter_data;
  const Rcpp::List pv_iter_data;
  const Rcpp::List wind_iter_data;
  const Rcpp::List wind_off_iter_data;

  Rcpp::DataFrame out;
  GrHelper(const int aPeriod,
           const std::string aGRName,
           const Interp::DataFrame aLDCData,
           const Interp::DataFrame aPVData,
           const Interp::DataFrame aWindData,
           const Interp::DataFrame aWindOffData):mPeriod(aPeriod),
           mGRName(aGRName),
           iterVec(aLDCData[0]),
           ldc_iter_data(aLDCData[1]),
           pv_iter_data(aPVData[1]),
           wind_iter_data(aWindData[1]),
           wind_off_iter_data(aWindOffData[1]) {}
  template<typename T>
  void processData(T& aData) {
    Interp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
  }
  TechSegCFType repackCFData(const Rcpp::DataFrame aIterDF) {
    Rcpp::StringVector state = aIterDF[0];
    Rcpp::List stateNest = aIterDF[1];
    TechSegCFType ret;
    for(int rowState = 0; rowState < aIterDF.nrows(); ++rowState) {
      Rcpp::DataFrame stateData = static_cast<Rcpp::DataFrame>(stateNest[rowState]);
      Rcpp::StringVector seg = stateData[0];
      Rcpp::NumericVector value = stateData[1];
      std::list<std::pair<std::string, double> > segMapTemp;
      for(int row = 0; row < stateData.nrows(); ++row) {
        segMapTemp.push_back(std::make_pair(static_cast<std::string>(seg[row]), value[row]));
      }
      ret[static_cast<std::string>(state[rowState])] = segMapTemp;
    }
    return ret;
  }
  template<>
  void processData(Sector*& aData) {
    DispatchSector* dispSector = dynamic_cast<DispatchSector*>(aData);
    std::list<UncData> allDataByIter;
    const int NUM_ITER = ldc_iter_data.length();
    for(int iter = 0; iter < NUM_ITER; ++iter) {
      UncData currData;
      const Interp::DataFrame& ldc_seg_data = static_cast<Interp::DataFrame>(ldc_iter_data[iter]);
      Rcpp::StringVector seg = ldc_seg_data[0];
      Rcpp::NumericVector value = ldc_seg_data[1];
      for(int row = 0; row < ldc_seg_data.nrows(); ++row) {
        currData.aNewLDC[static_cast<std::string>(seg[row])] = value[row];
      }
      currData.aNewPV = repackCFData(static_cast<Interp::DataFrame>(pv_iter_data[iter]));
      currData.aNewWind = repackCFData(static_cast<Interp::DataFrame>(wind_iter_data[iter]));
      currData.aNewOffWind = repackCFData(static_cast<Interp::DataFrame>(wind_off_iter_data[iter]));
      allDataByIter.push_back(currData);
    }

    std::pair<std::vector<double>, std::vector<double> > expBack = dispSector->runUncertainty(allDataByIter, mPeriod);

    Rcpp::StringVector grCol(NUM_ITER, mGRName);
    out = Rcpp::DataFrame::create(Rcpp::Named("grid_region") = grCol,
                                  Rcpp::Named("iter") = iterVec,
                                  Rcpp::Named("price") = expBack.first,
                                  Rcpp::Named("remain") = expBack.second);
    /*Rcpp::StringVector grCol(NUM_ITER, mGRName);
    out = Rcpp::DataFrame::create(Rcpp::Named("grid_region") = grCol,
                                  Rcpp::Named("iter") = iterVec,
                                  Rcpp::Named("price") = NumericVector::create(NUM_ITER, 0),
                                  Rcpp::Named("remain") = NumericVector::create(NUM_ITER, 0));*/
  }

};

Interp::DataFrame run_disp_unsec(Scenario* aScenario,
                                 const int aPeriod,
                                 const Interp::DataFrame& aLDCData,
                                 const Interp::DataFrame& aPVData,
                                 const Interp::DataFrame& aWindData,
                                 const Interp::DataFrame& aWindOffData)
{
  cout << "In func" << endl;
  const Rcpp::StringVector& gr_names = aLDCData[0];
  cout << "Get names" << endl;
  const Rcpp::List& ldc_gr_data = aLDCData[1];
  cout << "Get ldc" << endl;
  const Rcpp::List& pv_gr_data = aPVData[1];
  const Rcpp::List& wind_gr_data = aWindData[1];
  const Rcpp::List& wind_off_gr_data = aWindOffData[1];
  int grInd = 0;
  cout << "Before create filter steps" << endl;
  std::vector<FilterStep*> filterSteps {
    new FilterStep("world"),
    new FilterStep("region", new NamedFilter(new StringVecEquals(gr_names, grInd))),
    new FilterStep("sector", new NamedFilter(new StringEquals("electricity")))
  };
  cout << "Before loop" << endl;
  Rcpp::Function rClassFn("class");
  Rcpp::Environment dplyr_env = Rcpp::Environment::namespace_env("dplyr");
  Rcpp::Function bind_rows = dplyr_env["bind_rows"];
  Rcpp::List outList;
  for(grInd = 0; grInd < aLDCData.nrows(); ++grInd) {
    cout << "Saw: " << static_cast<std::string>(gr_names[grInd]) << endl;
    /*std::string colClass(Rcpp::as<std::string>(rClassFn(ldc_gr_data)));
    std::string col1Class(Rcpp::as<std::string>(rClassFn(ldc_gr_data[grInd])));
    cout << "c1: " << colClass << " c2: " << col1Class << endl;*/

    GrHelper currGr(aPeriod, static_cast<std::string>(gr_names[grInd]), static_cast<Interp::DataFrame>(ldc_gr_data[grInd]), static_cast<Interp::DataFrame>(pv_gr_data[grInd]), static_cast<Interp::DataFrame>(wind_gr_data[grInd]), static_cast<Interp::DataFrame>(wind_off_gr_data[grInd]));
    GCAMFusion<GrHelper> fusion(currGr, filterSteps);
    fusion.startFilter(aScenario);
    outList.push_back(currGr.out);
    cout << "Here " << grInd << endl;
  }
  return static_cast<Rcpp::DataFrame>(bind_rows(outList));
}
