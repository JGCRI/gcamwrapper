% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/gcamwrapper.R
\name{run_to_period}
\alias{run_to_period}
\title{Run model period}
\usage{
run_to_period(gcam, period = NULL)
}
\arguments{
\item{gcam}{(gcam) An initialized GCAM instance}

\item{period}{(integer) The GCAM model period to run up to or the}
}
\value{
GCAM instance
}
\description{
Run model period
}
\details{
This will run the given GCAM instance for all periods up
and including the given period.  Model periods which have already
been run will be kept track of and will not be run again.  HOWEVER,
we do not attempt to keep track of if those model periods are "dirty"
such as if a user has called `set_data` in such a way that would invalidate
that solution.
}
