#' @useDynLib gcamwrapper
#' @importFrom Rcpp sourceCpp
NULL

#' @importFrom Rcpp loadModule
loadModule("gcam_module", TRUE)

#' Create GCAM instance
#' @param configuration (string) The configuration XML to use.
#' @param workdir (string) The working directory to use which may be important if the
#' paths in \code{configuration} are relative.
#' @return GCAM instance
#' @export
create_and_initialize <- function(configuration = "configuration.xml", workdir = ".") {
  new(gcam, configuration, workdir)
}

#' Run model period
#' @details This will run the given GCAM instance for all periods up
#' and including the given period
#' @param gcam (gcam) An initialized GCAM instance
#' @param period (integer) The GCAM model period to run up to
#' @return GCAM instance
#' @importFrom Rcpp cpp_object_initializer
#' @export
run_to_period <- function(gcam, period) {
  gcam$run_to_period(period)

  gcam
}

#' Set some aribtrary data into GCAM
#' @details Use GCAM Fusion to set some table of data into GCAM.
#' @param gcam (gcam) An initialized GCAM instance
#' @param data (data.frame) A data.frame with the data to set
#' @param path (string) A GCAM fusion-ish search path to determine where to set the data.
#' @return GCAM instance
#' @export
set_data <- function(gcam, data, path) {
  gcam$set_data(data, path)
}

#' Get some arbitrary data out of GCAM
#' @details Use GCAM Fusion to get some table of data out of GCAM.
#' @param gcam (gcam) An initialized GCAM instance
#' @param path (string) A GCAM fusion-ish search path to determine where to get the data.
#' @return A tibble containing the requested data
#' @export
#' @importFrom dplyr group_by_at vars summarize_at ungroup as_tibble
#' @importFrom magrittr %>%
get_data <- function(gcam, path) {
  data <- gcam$get_data(path)
  # The data comming out of gcam is unaggregated so we will need to do that now
  # first figure out what the "value" column is, group by everything else, and summarize
  col_names <- names(data)
  value_col <- ifelse(col_names[length(col_names)] == "year", col_names[length(col_names)-1], col_names[length(col_names)])
  group_cols <- col_names[col_names != value_col]
  as_tibble(data) %>%
    group_by_at(vars(group_cols)) %>%
    summarize_at(vars(value_col), list(sum)) %>%
    ungroup()
}

#' Create a solution debugging object
#' @details Create a solution debugging object which can be used a single
#' evaluation of the model and see how it affects prices, supplies,
#' and demands amongst other things.
#' @param gcam (gcam) An initialized GCAM instance
#' @param period (integer) GCAM model period to create the debugger
#' @return A SolutionDebugger object
#' @export
create_solution_debugger <- function(gcam, period) {
  gcam$create_solution_debugger(period)
}

#' Gets the prices
#' @details Gets the prices of solvable markets as an array with market names
#' as the index.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param scaled (boolean) If the prices should be returned in the scaled form (native
#' to the solver) or unscaled (native to the model)
#' @return An array of prices indexed by market name.
#' @export
get_prices <- function(sd, scaled) {
  sd$get_prices(scaled)
}

#' Gets the F(x)
#' @details Gets the F(X), or supply - demand, of solvable markets as an array
#' with market names as the index.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @return An array of F(x) indexed by market name.
#' @export
get_fx <- function(sd) {
  sd$get_fx()
}

#' Gets the supplies
#' @details Gets the supply of solvable markets as an array with market names
#' as the index.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param scaled (boolean) If the supply should be returned in the scaled form (native
#' to the solver) or unscaled (native to the model).
#' @return An array of supplies indexed by market name.
#' @export
get_supply <- function(sd, scaled) {
  sd$get_supply(scaled)
}

#' Gets the demands
#' @details Gets the demand of solvable markets as a Series with market names
#' as the index.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param scaled (boolean) If the demands should be returned in the scaled form (native
#' to the solver) or unscaled (native to the model).
#' @return An array of demands indexed by market name.
#' @export
get_demand <- function(sd, scaled) {
  sd$get_demand(scaled)
}

#' Gets the price scale factor
#' @details Gets the price scale factor which attempts to normalize GCAM prices
#' to ideally be around 1.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @return An array of price scale factors indexed by market name.
#' @export
get_price_scale_factor <- function(sd) {
  sd$get_price_scale_factor()
}

#' Gets the quantity scale factor
#' @details Gets the quantity scale factor which attempts to normalize GCAM supplies
#' and demands to ideally be around 1.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @return An array of supply/demand scale factors indexed by market name.
#' @export
get_quantity_scale_factor <- function(sd) {
  sd$get_quantity_scale_factor()
}

#' Sets an array of prices into the model
#' @details Sets an aray of prices into the model but does not immediately evaluate them.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param prices (numeric array) An array of prices to be set.
#' @param scaled (boolean) If the given prices are already scaled or not.  If they are not
#' they will be before getting set into the solver.
#' @export
set_prices <- function(sd, prices, scaled) {
  sd$set_prices(prices, scaled)
}

#' Evaluate the model at the given prices
#' @details Sets an array of prices into the model and evaluate them and returns the
#' resulting F(x).
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param prices (numeric array) An array of prices to be set.
#' @param scaled (boolean) If the given prices are already scaled or not.  If they are not
#' they will be before getting set into the solver.
#' @param reset (boolean) If the STATE of the model should be reset after doing this evaluation.
#' @return An array of the F(x) which results from the evaluation.
#' @export
evaluate <- function(sd, prices, scaled, reset) {
  sd$evaluate(prices, scaled, reset)
}

#' Sets a single price into the model and evaluate it
#' @details Sets a single price into the model and evaluate it and returns the resulting F(x).
#' This is similar to calling evauluation with reset = True however it can optimize
#' by knowing only a single market is changing and therefore only calculate the subset
#' of the model which is affected which can be significantly faster to evaluate.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param price (numeric) A price to be set.
#' @param index (integer) An R based index (i.e. starts at 1) at which to update the price.
#' @param scaled (boolean) If the given price is already scaled or not.  If it is not
#' it will be before getting set into the solver.
#' @return An array of the F(x) which results from the evaluation.
#' @export
evaluate_partial <- function(sd, price, index, scaled) {
  # the C++ will be expecting a zero based index so adjust here
  sd$evaluate_partial(price, index-1, scaled)
}

#' Calculates the Jacobian matrix
#' @details Calculates the Jacobian matrix from the set of prices currently set in
#' the solver.  This is a finite difference derivative for each market
#' represented by column.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @return A matrix where the column and rows are indexed by market names.
#' @export
calc_derivative <- function(sd) {
  sd$calc_derivative()
}

#' Get the "correction" slope
#' @details Get the "correction" slope, which is used by the solver to give
#' continous behavior when a price for a market falls below the
#' "lower bound supply" price, or the price below which GCAM would
#' produce no supply.  The idea being to add a negative supply to
#' ensure there is always continous behavior and we would ideally match
#' the slope to what it was just above the "lower bound price" so the
#' solver can move back into the proper range of prices.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @return An array of the correction slope indexed by market name.
#' @export
get_slope <- function(sd) {
  sd$get_slope()
}

#' Set the "correction" slope
#' @details Set the "correction" slope array into the model.
#' @param sd (SolutionDebugger) A SolutionDebugger instance
#' @param slope (numeric array) An array of negative correction slopes to be set.
#' @export
set_slope <- function(sd, slope) {
  sd$set_slope(slope)
}
