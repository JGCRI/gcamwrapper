% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/gcamwrapper.R
\name{create_solution_debugger}
\alias{create_solution_debugger}
\title{Create a solution debugging object}
\usage{
create_solution_debugger(gcam, period = NULL, market_filter = "solvable")
}
\arguments{
\item{gcam}{(gcam) An initialized GCAM instance}

\item{period}{(integer) GCAM model period to create the debugger or if NULL
use the last run model period.}

\item{market_filter}{(string) A \code{<solution-info-filter>} string in the same
format as in the solver config XML.  The default is "solvable".}
}
\value{
A SolutionDebugger object
}
\description{
Create a solution debugging object
}
\details{
Create a solution debugging object which can be used a single
evaluation of the model and see how it affects prices, supplies,
and demands amongst other things.
}
