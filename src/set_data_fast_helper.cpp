#include "interp_interface.h"

#include "set_data_fast_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

#include <boost/container_hash/hash.hpp>

using namespace std;
using namespace Interp;

/*!
 * \brief A wrapper around an AMatchesValue so that we can record to a DataFrame
 *        the current values that may be matched by AMatchesValue.
 */
class AMatcherHashWrapper : public AMatchesValue {
public:
    AMatcherHashWrapper(AMatchesValue* aToWrap, const std::string& aDataName):mToWrap(aToWrap), mDataName(aDataName)
    {
    }
    virtual ~AMatcherHashWrapper() {
        delete mToWrap;
    }
    const std::string getDataName() const {
        return mDataName;
    }
    virtual size_t getHash() const = 0;

protected:
    //! The actual AMatchesValue which determines if the current path matches the query
    AMatchesValue* mToWrap;
    //! The data name which is useful to keep track of as a mapping back to DataFrame columns
    const std::string mDataName;
};

class StrMatcherHashWrapper : public AMatcherHashWrapper {
public:
  StrMatcherHashWrapper(AMatchesValue* aToWrap, const std::string& aDataName):AMatcherHashWrapper(aToWrap, aDataName)
  {
  }
    virtual bool matchesString( const std::string& aStrToTest ) const {
        bool matches = mToWrap->matchesString(aStrToTest);
        if(matches) {
          const_cast<StrMatcherHashWrapper*>(this)->mCurrValue = aStrToTest;
        }
        return matches;
    }
    virtual size_t getHash() const {
        return boost::hash_value<std::string>(mCurrValue);
    }
    private:
    //! A temporary holding the last matched value which may get copied
    //! into mData if recordPath is called
    string mCurrValue;
};

class IntMatcherHashWrapper : public AMatcherHashWrapper {
public:
  IntMatcherHashWrapper(AMatchesValue* aToWrap, const std::string& aDataName):AMatcherHashWrapper(aToWrap, aDataName)
  {
  }
    virtual bool matchesInt( const int aIntToTest ) const {
        bool matches = mToWrap->matchesInt(aIntToTest);
        if(matches) {
            const_cast<IntMatcherHashWrapper*>(this)->mCurrValue = aIntToTest;
        }
        return matches;
    }
    virtual size_t getHash() const {
        return boost::hash_value<int>(mCurrValue);
    }
    private:
    //! A temporary holding the last matched value which may get copied
    //! into mData if recordPath is called
    int mCurrValue;
};

/*!
 * \brief Prepare to run the given query.
 * \param aQuery The GCAM Fusion query to be parsed
 */
SetDataFastHelper::SetDataFastHelper(const Interp::DataFrame& aData, const std::string& aHeader):QueryProcessorBase(), mData(aData) {
    // parse the query into filter steps
    parseFilterString(aHeader);

    // determine if a user already filtered a period vector of data
    // if not, and we are in fact querying a vector of data, we will
    // add it for them
    bool hasYearInPath = false;
    for(auto tracker : mPathTracker) {
        if(tracker->getDataName() == "year") {
            hasYearInPath = true;
        }
    }
    if(mPathTracker.size() < mTempDataID.size() && !hasYearInPath) {
      mPathTracker.push_back(new IntMatcherHashWrapper(createMatchesAny(), "year"));
    }
    if(mPathTracker.size() != mTempDataID.size()) {
        Interp::stop("Number of column reads did not align with path tracker");
    }

    Interp::NumericVector data(Interp::getDataFrameAt<Interp::NumericVector>(aData, -1));
    int len = getDataFrameNumRows(mData);

    for(size_t row = 0; row < len; ++row) {
        std::size_t seed = 0;
        for(const auto& col : mTempDataID) {
            boost::hash_combine(seed, col[row]);
        }
        
        mDataVector[seed] = data[row];
    }
    mTempDataID.clear();
    if(mDataVector.size() != len) {
        stop("Mismatch in length of hash table, possible collision");
    }
}

/*!
 * \brief Run the query against the given Scenario context and
 *        return the results as a DataFrame.
 * \param aScenario The Scenario object which will serve as the
 *                  query context from which to evaluate the query.
 * \return A DataFrame where the columns include all the name/year
 *         of the GCAM CONTAINER the user indicated they wanted to
 *         record and the last column holds the values that results
 *         from the query.
 */
void SetDataFastHelper::run(Scenario* aScenario) 

{
  // run the query, the specialized filters will keep track
  // of matching data to use as columns as it processes
  GCAMFusion<SetDataFastHelper> fusion(*this, mFilterSteps);
  fusion.startFilter(aScenario);
}

AMatchesValue* SetDataFastHelper::wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt) {
    // if the user intended to record the value at this filter then we just
    // wrap whatever filter they set with the path tracking filter
    AMatcherHashWrapper* ret = aIsInt ?
        static_cast<AMatcherHashWrapper*>(new IntMatcherHashWrapper(aToWrap, aDataName )) :
        static_cast<AMatcherHashWrapper*>(new StrMatcherHashWrapper( aToWrap, aDataName ));
    // and add it to the list so we can trigger them to record values when a query
    // successfully matches data
    mPathTracker.push_back(ret);
    return ret;
}

AMatchesValue* SetDataFastHelper::parsePredicate( const std::vector<std::string>& aFilterOptions, const int aCol, const bool aIsRead ) const {
    if(aIsRead) {
        size_t len = getDataFrameNumRows(mData);
        std::vector<size_t>& retHash = const_cast<SetDataFastHelper*>(this)->mTempDataID.emplace_back(len);
        if( aFilterOptions[ 0 ] == "EnumFilter" ) {
            StringVector enumNames(getDataFrameAt<StringVector>(mData, aCol));
            for(int i = 0; i < len; ++i) {
                std::string currName = Interp::extract(enumNames[i]);
                retHash[i] = boost::hash_value<int>(convertToEnum(aFilterOptions[1], currName));
            }
        }
        else if( aFilterOptions[ 0 ] == "NamedFilter" ) {
            StringVector strVals(getDataFrameAt<StringVector>(mData, aCol));
            for(int i = 0; i < len; ++i) {
                std::string currVal = Interp::extract(strVals[i]);
                retHash[i] = boost::hash_value<std::string>(currVal);
            }
        }
        else if( aFilterOptions[ 0 ] == "YearFilter" || aFilterOptions[ 0 ] == "IndexFilter" ) {
            IntegerVector strVals(getDataFrameAt<IntegerVector>(mData, aCol));
            for(int i = 0; i < len; ++i) {
                int currVal(strVals[i]);
                retHash[i] = boost::hash_value<int>(currVal);
            }
        }
        else {
            Interp::stop("Unknown filter operand: " + aFilterOptions[ 0 ]);
        }
    }
    return QueryProcessorBase::parsePredicate(aFilterOptions, aCol, aIsRead);
}

template<typename DataType>
void processSet(DataType& aDataToSet, const std::map<size_t, double>& aDataVec, const std::vector<AMatcherHashWrapper*>& aPath) {
    size_t seed = 0;
    for(auto tracker : aPath ) {
        boost::hash_combine(seed, tracker->getHash());
    }
    auto it = aDataVec.find(seed);
    if(it != aDataVec.end()) {
        aDataToSet = (*it).second;
    }
}


// GCAM Fusion callbacks with specializations for all of the types that
// we support:

template<>
void SetDataFastHelper::processData(double& aData) {
    processSet(aData, mDataVector, mPathTracker);
}
template<>
void SetDataFastHelper::processData(Value& aData) {
    processSet(aData, mDataVector, mPathTracker);
}
template<>
void SetDataFastHelper::processData(int& aData) {
    processSet(aData, mDataVector, mPathTracker);
}
template<>
void SetDataFastHelper::processData(std::vector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(std::vector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(std::vector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::PeriodVector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::PeriodVector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::PeriodVector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::TechVintageVector<int>& aData) {
  vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::TechVintageVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::TechVintageVector<Value>& aData) {
  vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(objects::YearVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void SetDataFastHelper::processData(std::map<unsigned int, double>& aData) {
  // the user did not explicitly specify a year filter for this data but
  // it seems better to assume they did rather than implicitly aggregate
  // across model periods
  // so let's add the path tracking filter to record all years the first
  // time we see this for them
  for(auto iter = aData.begin(); iter != aData.end(); ++iter) {
    // update the year path tracker which would not have had to match thus far
    // as an ARRAY is not a CONTAINER
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aData, iter));
    // delegate to processData to take care of the rest
    processData((*iter).second);
  }
}
template<typename VecType>
void SetDataFastHelper::vectorDataHelper(VecType& aDataVec) {
  // the user did not explicitly specify a year filter for this data but
  // it seems better to assume they did rather than implicitly aggregate
  // across model periods
  // so let's add the path tracking filter to record all years the first
  // time we see this for them
  for(auto iter = aDataVec.begin(); iter != aDataVec.end(); ++iter) {
    // update the year path tracker which would not have had to match thus far
    // as an ARRAY is not a CONTAINER
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aDataVec, iter));
    // delegate to processData to take care of the rest
    processData(*iter);
  }
}

template<typename T>
void SetDataFastHelper::processData(T& aData) {
  Interp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
}

