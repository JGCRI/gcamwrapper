#ifndef __INTERP_INTERFACE_H__
#define __INTERP_INTERFACE_H__

#include <exception>
#include <string>

#if __has_include("Rcpp.h")
#define STRICT_R_HEADERS
#include "Rcpp.h"
#elif __has_include(<boost/python.hpp>)
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#endif

class gcam_exception : public std::exception {
    public:
        gcam_exception(const char* aMessage):mMessage(aMessage) {}
        virtual const char* what() const _NOEXCEPT {
            return mMessage;
        }
    private:
        const char* mMessage;
};

#if defined(USING_R)
namespace Interp = Rcpp;
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
    using DataFrame = bp::dict;
    using NumericVector = bnp::ndarray;
    using StringVector = bnp::ndarray;
    using IntegerVector = bnp::ndarray;
    using NumericMatrix = bnp::ndarray;

    DataFrame createDataFrame() {
        DataFrame ret;
        return ret;
    }
    DataFrame createDataFrame(std::vector<std::string>& aColNames) {
        DataFrame ret;
        bp::tuple emptyDim = bp::make_tuple(1);
        for(std::string name : aColNames) {
            ret[name] = bnp::empty(emptyDim, bnp::dtype::get_builtin<int>());
        }
        return ret;
    }
    /*
    void setDataFrameCol(DataFrame& aDataFrame, const int aIndex, const bnp::ndarray& aVector) {
        aDataFrame.values()[aIndex] = aVector;
    }
    */
    bnp::ndarray getDataFrameCol(const DataFrame& aDataFrame, const int aIndex) {
        return bp::extract<bnp::ndarray>(aDataFrame.values()[aIndex]);
    }

    template<typename DataType, typename VectorType>
    VectorType createVector(int aSize) {
        return bnp::empty(bp::make_tuple(aSize), bnp::dtype::get_builtin<DataType>());
    }
    template<>
    StringVector createVector<std::string, StringVector>(int aSize) {
        return bnp::empty(bp::make_tuple(aSize), bnp::dtype(bp::str("object")));
    }
    NumericMatrix createNumericMatrix(int aSize) {
        return bnp::empty(bp::make_tuple(aSize, aSize), bnp::dtype::get_builtin<double>());
    }
    void setVectorNames(bnp::ndarray& aVector, const std::vector<std::string>& aNames) {
    }
    void setMatrixNames(bnp::ndarray& aMatrix, const std::vector<std::string>& aNames) {
    }

}
#endif

#endif // __INTERP_INTERFACE_H__
