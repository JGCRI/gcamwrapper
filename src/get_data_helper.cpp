#include "interp_interface.h"

#include "get_data_helper.h"

#include "util/base/include/gcam_fusion.hpp"
#include "util/base/include/gcam_data_containers.h"

using namespace std;
using namespace Interp;

/*!
 * \brief A wrapper around an AMatchesValue so that we can record to / read from
 *        a DataFrame the current values that may be matched by AMatchesValue.
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

GetDataHelper::GetDataHelper(const std::string& aHeader):QueryProcessorBase() {
    parseFilterString(aHeader);
    for(auto tracker : mPathTracker) {
        if(tracker->getDataName() == "year") {
            mHasYearInPath = true;
        }
    }
}

DataFrame GetDataHelper::run(Scenario* aScenario) {
  GCAMFusion<GetDataHelper> fusion(*this, mFilterSteps);
  fusion.startFilter(aScenario);
  DataFrame ret = Interp::createDataFrame();
  size_t i = 0;
  for(i = 0; i < mPathTracker.size(); ++i) {
      mPathTracker[i]->updateDataFrame(ret);
  }
  ret[mDataColName] = Interp::wrap(mDataVector);
  return ret;
}

template<>
void GetDataHelper::processData(double& aData) {
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void GetDataHelper::processData(Value& aData) {
    mDataVector.push_back(aData);
    for(auto path: mPathTracker) {
        path->recordPath();
    }
}
template<>
void GetDataHelper::processData(int& aData) {
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
  if(!mHasYearInPath) {
      mPathTracker.push_back(new IntMatcherWrapper(createMatchesAny(), "year"));
      mHasYearInPath = true;
  }
  for(auto iter = aData.begin(); iter != aData.end(); ++iter) {
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aData, iter));
    processData((*iter).second);
  }
}
template<typename VecType>
void GetDataHelper::vectorDataHelper(VecType& aDataVec) {
  if(!mHasYearInPath) {
      mPathTracker.push_back(new IntMatcherWrapper(createMatchesAny(), "year"));
      mHasYearInPath = true;
  }
    for(auto iter = aDataVec.begin(); iter != aDataVec.end(); ++iter) {
    (*mPathTracker.rbegin())->matchesInt(GetIndexAsYear::convertIterToYear(aDataVec, iter));
        processData(*iter);
    }
}

template<typename T>
void GetDataHelper::processData(T& aData) {
  Interp::stop(string("Search found unexpected type: ")+string(typeid(T).name()));
}

AMatchesValue* GetDataHelper::wrapPredicate(AMatchesValue* aToWrap, const std::string& aDataName, const bool aIsInt) {
    AMatcherWrapper* ret = aIsInt ?
        static_cast<AMatcherWrapper*>(new IntMatcherWrapper(aToWrap, aDataName )) :
        static_cast<AMatcherWrapper*>(new StrMatcherWrapper( aToWrap, aDataName ));
    mPathTracker.push_back(ret);
    return ret;
}

