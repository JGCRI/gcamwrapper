#ifndef __INTERP_INTERFACE_H__
#define __INTERP_INTERFACE_H__

#include <exception>
#include <string>

#if __has_include("Rcpp.h")
#define STRICT_R_HEADERS
#include "Rcpp.h"
#elif __has_include(<boost/python.hpp>)
#include <boost/python.hpp>
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
    //using DataFrame = boost::python::dict;
    /*
    using DataFrame = std::map<std::string, boost::python::numpy::ndarray>;
    using NumericVector = boost::python::numpy::ndarray;
    using StringVector = boost::python::numpy::ndarray;
    using IntegerVector = boost::python::numpy::ndarray;
    using NumericMatrix = boost::python::numpy::ndarray;
    */
}
#endif

#endif // __INTERP_INTERFACE_H__
