#ifndef __INTERP_INTERFACE_H__
#define __INTERP_INTERFACE_H__

#include <exception>
#include <string>

#if __has_include("Rcpp.h")
#define IS_INTERP_R
#define STRICT_R_HEADERS
#include "Rcpp.h"
#elif __has_include(<boost/python.hpp>)
#define IS_INTERP_PYTHON
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#endif

class gcam_exception : public std::exception {
    public:
        gcam_exception(const char* aMessage):mMessage(aMessage) {}
        inline virtual const char* what() const _NOEXCEPT {
            return mMessage;
        }
    private:
        const char* mMessage;
};

#if defined(USING_R)
namespace Interp {
    using Rcpp::stop;
    using Rcpp::warning;
    using Rcpp::DataFrame;
    using Rcpp::NumericVector;
    using Rcpp::StringVector;
    using Rcpp::IntegerVector;
    using Rcpp::NumericMatrix;

    inline DataFrame createDataFrame() {
      return DataFrame::create();
    }
    template<typename VectorType>
    inline void setDataFrameCol(DataFrame& aDataFrame, const std::string& aCol, const VectorType& aVector) {
      aDataFrame[aCol] = aVector;
    }
    template<typename VectorType>
    inline VectorType getDataFrameAt(const DataFrame& aDataFrame, const int aIndex) {
      return aDataFrame.at(aIndex < 0 ? aIndex + aDataFrame.length() : aIndex);
    }
    inline int getDataFrameNumRows(const DataFrame& aDataFrame) {
      return aDataFrame.nrow();
    }

    template<typename DataType, typename VectorType>
    VectorType createVector(int aSize) {
      return VectorType(aSize);
    }
    inline NumericMatrix createNumericMatrix(int aSize) {
      return NumericMatrix(aSize, aSize);
    }
    template<typename VectorType>
    inline void setVectorNames(VectorType& aVector, const StringVector& aNames) {
      aVector.names() = aNames;
    }
    inline void setMatrixNames(NumericMatrix& aMatrix, const StringVector& aNames) {
      Rcpp::rownames(aMatrix) = aNames;
      Rcpp::colnames(aMatrix) = aNames;
    }
    using Rcpp::wrap;
    template<typename MatrixType>
    NumericMatrix wrapMatrix(const MatrixType& aData, int aSize) {
      NumericMatrix ret = createNumericMatrix(aSize);
      for(int row = 0; row < aSize; ++row) {
        for(int col = 0; col < aSize; ++col) {
          ret.at(row, col) = aData(row, col);
        }
      }
      return ret;
    }
}

#elif defined(PY_VERSION_HEX)
namespace Interp {
    static void stop(const std::string& aMessage) {
        throw new gcam_exception(aMessage.c_str());
    }
    static void warning(const std::string& aMessage) {
        PyErr_WarnEx(PyExc_UserWarning, aMessage.c_str(), 1);
    }
    namespace bp = boost::python;
    namespace bnp = boost::python::numpy;
    template<typename T>
    struct NumpyVecWrapper {
        bnp::ndarray mNPArr;
        T* mRawArr;
        // TODO: error checking
        NumpyVecWrapper(bnp::ndarray aNPArr):
            mNPArr(aNPArr),
            mRawArr(reinterpret_cast<T*>(aNPArr.get_data()))
        {
        }
        T& operator[](const size_t aIndex) {
            return mRawArr[aIndex];
        }
        T& operator[](const size_t aIndex) const {
            return mRawArr[aIndex];
        }
        operator bnp::ndarray() const {
            return mNPArr;
        }
        operator bnp::ndarray&() {
            return mNPArr;
        }
        operator bp::object() const {
            return mNPArr;
        }
    };
    template<typename VecType>
        struct vec_to_python {
            static PyObject* convert( VecType const& aVec) {
                PyObject* ret = aVec.mNPArr.ptr();
                Py_INCREF(ret);
                return ret;
            }
        };


    using DataFrame = bp::dict;
    using NumericVector = NumpyVecWrapper<double>;
    using StringVector = NumpyVecWrapper<bp::str>;
    using IntegerVector = NumpyVecWrapper<int>;
    using NumericMatrix = bnp::ndarray;

    inline DataFrame createDataFrame() {
        DataFrame ret;
        return ret;
    }
    inline void setDataFrameCol(DataFrame& aDataFrame, const std::string& aCol, const bnp::ndarray& aVector) {
        aDataFrame[aCol] = aVector;
    }
    template<typename VecType>
    inline VecType getDataFrameAt(const DataFrame& aDataFrame, const int aIndex) {
        return VecType(bp::extract<bnp::ndarray>(aDataFrame.values()[aIndex]));
    }
    inline int getDataFrameNumRows(const DataFrame& aDataFrame) {
        bnp::ndarray vec0 = bp::extract<bnp::ndarray>(aDataFrame.values()[0]);
        return vec0.shape(0);
    }

    template<typename DataType>
    bnp::dtype interp_get_dtype() {
        // default is assume built in type
        return bnp::dtype::get_builtin<DataType>();
    }
    template<>
    inline bnp::dtype interp_get_dtype<std::string>() {
        // strings are just generic "object" dtypes
        return bnp::dtype(bp::str("object"));
    }

    template<typename DataType, typename VectorType>
    VectorType createVector(int aSize) {
        return bnp::empty(bp::make_tuple(aSize), interp_get_dtype<DataType>());
    }
    inline NumericMatrix createNumericMatrix(int aSize) {
        return bnp::empty(bp::make_tuple(aSize, aSize), bnp::dtype::get_builtin<double>());
    }
    inline void setVectorNames(bnp::ndarray& aVector, const StringVector& aNames) {
    }
    inline void setMatrixNames(bnp::ndarray& aMatrix, const StringVector& aNames) {
    }

    template<typename DataType>
    bnp::ndarray wrap(const std::vector<DataType>& aData) {
        bnp::dtype dtype = interp_get_dtype<DataType>();
        bp::tuple shape = bp::make_tuple(aData.size());
        bp::tuple stride = bp::make_tuple(dtype.get_itemsize());
        bp::object owner;
        // this will *not* copy results which perhaps may be useful but we
        // would need an owner with the correct scope
        // but to be consistent with R we do want to copy
        bnp::ndarray wrapRef = bnp::from_data(&aData[0], dtype, shape, stride, owner);
        return wrapRef.copy();
    }
    template<>
    inline bnp::ndarray wrap(const std::vector<std::string>& aData) {
        bnp::ndarray ret = createVector<std::string, StringVector>(aData.size());
        bp::str* retData = reinterpret_cast<bp::str*>(ret.get_data());
        for(size_t i = 0; i < aData.size(); ++i) {
            retData[i] = aData[i].c_str();
        }
        return ret;
    }

    template<typename MatrixType>
    NumericMatrix wrapMatrix(const MatrixType& aData, int aSize) {
        NumericMatrix ret = createNumericMatrix(aSize);
        double* retData = reinterpret_cast<double*>(ret.get_data());
        const size_t row_stride = aSize;
        double* row_iter = retData;
        for(int i = 0; i < aSize; ++i, row_iter += row_stride) {
            double* col_iter = row_iter;
            for (int j = 0; j < aSize; ++j, ++col_iter) {
                *col_iter = aData(i, j);
            }
        }
        return ret;
    }

}

#endif

#endif // __INTERP_INTERFACE_H__
