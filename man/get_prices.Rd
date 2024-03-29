% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/gcamwrapper.R
\name{get_prices}
\alias{get_prices}
\title{Gets the prices}
\usage{
get_prices(sd, scaled)
}
\arguments{
\item{sd}{(SolutionDebugger) A SolutionDebugger instance}

\item{scaled}{(boolean) If the prices should be returned in the scaled form (native
to the solver) or unscaled (native to the model)}
}
\value{
An array of prices indexed by market name.
}
\description{
Gets the prices
}
\details{
Gets the prices of solvable markets as an array with market names
as the index.
}
