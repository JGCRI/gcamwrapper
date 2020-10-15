#' @useDynLib gcamdebugr
#' @importFrom Rcpp sourceCpp
NULL

#' @importFrom Rcpp loadModule
loadModule("gcam_module", TRUE)

#' Create GCAM instance
#' @param configuration The configuration XML to use.
#' @param workdir The working directory to use which may be important if the
#' paths in \code{configuration} are relative.
#' @return GCAM instance
#' @export
create_and_initialize <- function(configuration = "configuration.xml", workdir = ".") {
  new(gcam, configuration, workdir)
}

#' Run model period
#' @details This will run the given GCAM instance for all periods up
#' and including the given period
#' @param gcam An initialized GCAM instance
#' @param period The GCAM model period to run up to
#' @return GCAM instance
#' @importFrom Rcpp cpp_object_initializer
#' @export
run_to_period <- function(gcam, period) {
  gcam$runToPeriod(period)

  gcam
}

#' Set some aribtrary data into GCAM
#' @details Use GCAM Fusion to set some table of data into GCAM.
#' @param gcam An initialized GCAM instance
#' @param data A data.frame with the data to set
#' @param path A GCAM fusion-ish search path to determine where to set the data.
#' @return GCAM instance
#' @export
set_data <- function(gcam, data, path) {
  gcam$setData(data, path)
}

#' Get some arbitrary data out of GCAM
#' @details Use GCAM Fusion to get some table of data out of GCAM.
#' @param gcam An initialized GCAM instance
#' @param path A GCAM fusion-ish search path to determine where to get the data.
#' @return A tibble containing the requested data
#' @export
#' @importFrom dplyr group_by_at vars summarize_at ungroup
#' @importFrom magrittr %>%
get_data <- function(gcam, path) {
  data <- gcam$getData(path)
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
