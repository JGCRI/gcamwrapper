% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/gcamwrapper.R
\name{evaluate}
\alias{evaluate}
\title{Evaluate the model at the given prices}
\usage{
evaluate(sd, prices, scaled, reset)
}
\arguments{
\item{sd}{(SolutionDebugger) A SolutionDebugger instance}

\item{prices}{(numeric array) An array of prices to be set.}

\item{scaled}{(boolean) If the given prices are already scaled or not.  If they are not
they will be before getting set into the solver.}

\item{reset}{(boolean) If the STATE of the model should be reset after doing this evaluation.}
}
\value{
An array of the F(x) which results from the evaluation.
}
\description{
Evaluate the model at the given prices
}
\details{
Sets an array of prices into the model and evaluate them and returns the
resulting F(x).
}
