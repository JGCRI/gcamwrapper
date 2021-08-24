#include "interp_interface.h"

#include "get_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Interp;

/*!
 * \brief A wrapper around an AMatchesValue so that we can record to a DataFrame
 *        the current values that may be matched by AMatchesValue.
 */
class AMatcherWrapper : public AMatchesValue {
public:
    AMatcherWrapper(AMatchesValue* aToWrap, const std::string& aDataName):mToWrap(aToWrap), mDataName(aDataName)
    {
    }
    virtual ~AMatcherWrapper() {
        delete mToWrap;
    }
    const std::string getDataName() const {
        return mDataName;
    }
    virtual void recordPath() {};
    virtual void updateDataFrame(DataFrame& aDataFrame) const {};

protected:
    //! The actual AMatchesValue which determines if the current path matches the query
    AMatchesValue* mToWrap;
    //! The data name which is useful to keep track of as a mapping back to DataFrame columns
    const std::string mDataName;
};

class StrMatcherWrapper : public AMatcherWrapper {
public:
  StrMatcherWrapper(AMatchesValue* aToWrap, const std::string& aDataName):AMatcherWrapper(aToWrap, aDataName)
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
    virtual void updateDataFrame(DataFrame& aDataFrame) const {
        aDataFrame[mDataName] = Interp::wrap(mData);
    }
    private:
    //! A temporary holding the last matched value which may get copied
    //! into mData if recordPath is called
    string mCurrValue;
    //! The list of values that matched when a query successfully found data
    //! which can be transformed into a column of a DataFrame
    //TODO: probably better as a std::list
    vector<string> mData;
};

class IntMatcherWrapper : public AMatcherWrapper {
public:
  IntMatcherWrapper(AMatchesValue* aToWrap, const std::string& aDataName):AMatcherWrapper(aToWrap, aDataName)
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
    virtual void updateDataFrame(DataFrame& aDataFrame) const {
        aDataFrame[mDataName] = Interp::wrap(mData);
    }
    private:
    //! A temporary holding the last matched value which may get copied
    //! into mData if recordPath is called
    int mCurrValue;
    //! The list of values that matched when a query successfully found data
    //! which can be transformed into a column of a DataFrame
    //TODO: probably better as a std::list
    vector<int> mData;
};

/*!
 * \brief Prepare to run the given query.
 * \param aQuery The GCAM Fusion query to be parsed
 */
GetDataHelper::GetDataHelper(const std::string& aQuery):QueryProcessorBase() {
    // parse the query into filter steps
    parseFilterString(aQuery);

    // determine if a user already filtered a period vector of data
    // if not, and we are in fact querying a vector of data, we will
    // add it for them
    mHasYearInPath = false;
    for(auto tracker : mPathTracker) {
        if(tracker->getDataName() == "year") {
            mHasYearInPath = true;
        }
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
DataFrame GetDataHelper::run(Scenario* aScenario) {
  // run the query, the specialized filters will keep track
  // of matching data to use as columns as it processes
  GCAMFusion<GetDataHelper> fusion(*this, mFilterSteps);
  fusion.startFilter(aScenario);

  // extract the data from the path tracking filters and
  // organize them as columns in a DataFrame
  DataFrame ret = Interp::createDataFrame();
  size_t i = 0;
  for(i = 0; i < mPathTracker.size(); ++i) {
      mPathTracker[i]->updateDataFrame(ret);
  }
  // the actual values will be the final column
  ret[mDataColName] = Interp::wrap(mDataVector);
  return ret;
}

AMatchesValue* GetDataHelper::wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt) {
    // if the user intended to record the value at this filter then we just
    // wrap whatever filter they set with the path tracking filter
    AMatcherWrapper* ret = aIsInt ?
        static_cast<AMatcherWrapper*>(new IntMatcherWrapper(aToWrap, aDataName )) :
        static_cast<AMatcherWrapper*>(new StrMatcherWrapper( aToWrap, aDataName ));
    // and add it to the list so we can trigger them to record values when a query
    // successfully matches data
    mPathTracker.push_back(ret);
    return ret;
}

// GCAM Fusion callbacks with specializations for all of the types that
// we support:

template<>
void GetDataHelper::processData(double& aData) {
    // add the value which matched and have the tracking filters
    // record their current values to include in this row
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void GetDataHelper::processData(Value& aData) {
    // add the value which matched and have the tracking filters
    // record their current values to include in this row
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void GetDataHelper::processData(int& aData) {
    // add the value which matched and have the tracking filters
    // record their current values to include in this row
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void GetDataHelper::processData(std::vector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(std::vector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(std::vector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::PeriodVector<int>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::PeriodVector<double>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::PeriodVector<Value>& aData) {
    vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::TechVintageVector<int>& aData) {
  vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::TechVintageVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::TechVintageVector<Value>& aData) {
  vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(objects::YearVector<double>& aData) {
  vectorDataHelper(aData);
}
template<>
void GetDataHelper::processData(std::map<unsigned int, double>& aData) {
  // the user did not explicitly specify a year filter for this data but
  // it seems better to assume they did rather than implicitly aggregate
  // accross model periods
  // so let's add the path tracking filter to record all years the first
  // time we see this for them
  if(!mHasYearInPath) {
      mPathTracker.push_back(new IntMatcherWrapper(createMatchesAny(), "year"));
      mHasYearInPath = true;
  }
  for(auto iter = aData.begin(); iter != aData.end(); ++iter) {
    // update the year path tracker which would not have had to match thus far
    // as an ARRAY is not a CONTAINER
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aData, iter));
    // deletegate to processData to take care of the rest
    processData((*iter).second);
  }
}
template<typename VecType>
void GetDataHelper::vectorDataHelper(VecType& aDataVec) {
  // the user did not explicitly specify a year filter for this data but
  // it seems better to assume they did rather than implicitly aggregate
  // accross model periods
  // so let's add the path tracking filter to record all years the first
  // time we see this for them
  if(!mHasYearInPath) {
      mPathTracker.push_back(new IntMatcherWrapper(createMatchesAny(), "year"));
      mHasYearInPath = true;
  }
  for(auto iter = aDataVec.begin(); iter != aDataVec.end(); ++iter) {
    // update the year path tracker which would not have had to match thus far
    // as an ARRAY is not a CONTAINER
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aDataVec, iter));
    // deletegate to processData to take care of the rest
    processData(*iter);
  }
}

template<typename T>
void GetDataHelper::processData(T& aData) {
  Interp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
}

