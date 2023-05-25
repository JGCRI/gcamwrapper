
# we wrap the get_query function so as to hide the instance of the
# PACKAGE_QUERIES which will get loaded from file the first time it
# is needed and not again
wrap_get_query <- function() {
  PACKAGE_QUERIES <- NULL
  function(..., query_file = NULL) {
    if(is.null(query_file)) {
      if(is.null(PACKAGE_QUERIES)) {
        PACKAGE_QUERIES <<- read_yaml(system.file('extdata', 'query_library.yml', package="gcamwrapper"))
      }
      queries = PACKAGE_QUERIES
    } else {
      queries = read_yaml(query_file)
    }
    query_path <- list(...)
    query <- queries
    for(path in query_path) {
      query <- query[[path]]
      if(is.null(query)) {
        stop(paste0("Could not find query: ", query_path))
      }
    }
    if(length(query) == 0) {
      stop(paste0("Could not find query: ", query_path))
    }
    query_str = query[1]
    if(length(query) > 1) {
      attr(query_str, "units") <- query[2]
    }
    if(length(query) > 2) {
      warning(paste0("Additional elements for ", query_path, " are ignored, expecting only <query> <units>"))
    }

    return(query_str)
  }
}

#' Look up a query from a YAML file.
#'
#' The YAML query files will often be organized into nested categories
#' so users must provide the full path as individual strings such as:
#' \code{get_query("emissions", "co2_emissions")}.
#' @param query_file (string) The path to a YAML file to read if not NULL otherwise
#' the package query file will be used.
#' @param ... (string) Strings specifiying the path to the query to lookup.
#' @return (string) The raw query from the query file with units set as an attribute
#' if specified.
#' @importFrom yaml read_yaml
#' @export
get_query <- wrap_get_query()


# TODO: move to some external documentation
# Design doc:
# change get/set data to accept a query_args argument that is a dict/list
# used to substitute {arg_tag@arg_type} tags from the query database with the rules:
# 1) arg_type = name -> NamedFilter; year -> YearFilter
# 2) key: None -> +MatchesAny
# 3) key: [operator, int-ish]
#    where int-ish is ether an int or '.' support others?
#    and operators are </>|= or * for any
#    NOTE: we should support IndexFilter instead of YearFilter but to keep it simple
#    for the users we will try to detect which they meant by assuming only years will
#    be four digits long
# 4) key: [ operator,char]
#    where operators are either = or =~ for regex or * for any
# 5) For query tags not specified by the user it will be replaced with nothing:
# i.e. matches any but collapse

#' Find all placeholder in a raw query and return them in a list.
#'
#' Find query placeholders of the form \code{'{tag@type}'} and put them into a
#' list such as \code{list('tag1' = 'name', 'tag2' = 'year')} for instance.
#' @param query_str A raw query string.
#' @return A list of all the placeholders found in the query where the names
#' are the tags and the values are the types.
#' @importFrom stringr str_match_all str_split
find_placeholders <- function(query_str) {
    raw_placeholders <- str_match_all(query_str, '\\{([\\w@]+)\\}')[[1]][,2]
    ret <- list()
    for(placeholder in raw_placeholders) {
        ph_split = str_split(placeholder, '@')[[1]]
        if(length(ph_split) != 2) {
            stop(paste0('Invalid placeholder syntax: ', placeholder, 'expecting two values split by @'))
        } else if(ph_split[2] != 'name' && ph_split[2] != 'year') {
            stop(paste0('Invalid placeholder syntax, unknown type: ', ph_split[1], 'expecting name or year'))
        } else {
            ret[ph_split[1]] = ph_split[2]
        }
    }
    if(length(raw_placeholders) != length(ret)) {
        stop(paste0('Duplicate placeholder tags in ', query_str))
    }

    return(ret)
}

#' Parse user options for integer operators and generate GCAM Fusion syntax
#'
#' The currently supported operators are:
#' `+`, `-`: Indicates to read/write to a DataFrame if `+` or not to read/write if `-`
#' `*`: which always matches and so no additional operands are necessary
#' The standard comparison operators: `=`, `<`, `<=`, `>`, `>=`. Note if `is_get_data` is true
#' or the `-` is set an additional argument must be supplied which is the integer (or a
#' string that can be converted to integer) to be used as the RHS operand in the comparison.
#' Finally if param_operands is NULL or empty then if `is_get_data` then c('+', '*') is assumed
#' otherwise c('+', '=').
#' @param param_operands An array containing operators and potentially an operand to be used with that
#' operator.
#' @param is_get_data A boolean if true follows get data symantics and set data if false.
#' @return A GCAM Fusion filter string representing the parameters given.
parse_int_query_param <- function(param_operands, is_get_data) {
    wrapper_to_fusion_lookup = list('*'= 'MatchesAny', '<'= 'IntLessThan', '<='= 'IntLessThanEq', '>'= 'IntGreaterThan', '>='= 'IntGreaterThanEq', '='= 'IntEquals')
    # the default behavior is to set the '+' operator
    is_read = TRUE
    if('-' %in% param_operands) {
        ret = '['
        is_read = FALSE
        # remove the - (if set) for easier error checking later
        param_operands = param_operands[param_operands != '-']
    } else {
        ret = '[+'
        is_read = TRUE
        # remove the + (if set) for easier error checking later
        param_operands = param_operands[param_operands != '+']
    }

    # use default behavior if no param_operands were given
    if(is.na(param_operands) || length(param_operands) == 0) {
        # for get data the default is to match any
        # for set data the default is to match =
        param_operands = ifelse(is_get_data, c('*'), c('='))
    }

    if(param_operands[1] == '*') {
        if(!is_get_data && is_read) {
            stop(paste0('Using * without explictly not reading from columns with - is not valid in set_data: ', param_operands))
        }
        ret = paste0(ret, 'YearFilter,', wrapper_to_fusion_lookup['*'])
    } else if(!is_get_data && is_read) {
        if(length(param_operands) < 1) {
            stop(paste0('Invalid query parameter spec: ', param_operands))
        }
        ret = paste0(ret, 'YearFilter,', wrapper_to_fusion_lookup[param_operands[1]])
    } else if(length(param_operands) == 2 && param_operands[1] %in% names(wrapper_to_fusion_lookup)) {
        operandAsInt = as.integer(param_operands[2])
        if(is.na(operandAsInt)) {
            stop(paste0('Expecting integer operand, got: ', param_operands))
        }
        # if the int value looks like a date assume YearFilter otherwise it is
        # a model period
        ret = paste0(ret, ifelse(operandAsInt > 1000, 'YearFilter,', 'IndexFilter,'))
        ret = paste0(ret, wrapper_to_fusion_lookup[param_operands[1]], ',', operandAsInt)
    } else {
        stop(paste0('Invalid query parameter spec: ', param_operands))
    }
    ret = paste0(ret, ']')
    return(ret)
}

#' Parse user options for string operators and generate GCAM Fusion syntax
#'
#' The currently supported operators are:
#' `+`, `-`: Indicates to read/write to a DataFrame if `+` or not to read/write if `-`
#' `*`: which always matches and so no additional operands are necessary
#' The operators: `=`, `=~` (regular expression matching). Note if `is_get_data` is true
#' or the `-` is set an additional argument must be supplied which is the string to
#' be used as the RHS operand in the comparison.
#' Finally if param_operands is NULL or empty then if `is_get_data` then c('+', '*') is assumed
#' otherwise c('+', '=').
#' @param param_operands An array containing operators and potentially an operand to be used with that
#' operator.
#' @param is_get_data A boolean if true follows get data symantics and set data if false.
#' @return A GCAM Fusion filter string representing the parameters given.
parse_str_query_param <- function(param_operands, is_get_data) {
    wrapper_to_fusion_lookup = list('*'= 'MatchesAny', '='= 'StringEquals','=~'= 'StringRegexMatches')
    # the default behavior is to set the '+' operator
    is_read = TRUE
    if('-' %in% param_operands) {
        ret = '[NamedFilter,'
        is_read = FALSE
        # remove the - (if set) for easier error checking later
        param_operands = param_operands[param_operands != '-']
    } else {
        ret = '[+NamedFilter,'
        is_read = TRUE
        # remove the + (if set) for easier error checking later
        param_operands = param_operands[param_operands != '+']
    }

    # use default behavior if no param_operands were given
    if(is.na(param_operands) || length(param_operands) == 0) {
        # for get data the default is to match any
        # for set data the default is to match =
        param_operands = ifelse(is_get_data, c('*'), c('='))
    }

    if(param_operands[1] == '*') {
        if(!is_get_data && is_read) {
            stop(paste0('Using * without explictly not reading from columns with - is not valid in set_data: ', param_operands))
        }
        ret = paste0(ret, wrapper_to_fusion_lookup['*'])
    } else if(!is_get_data && is_read) {
        if(length(param_operands) < 1) {
            stop(paste0('Invalid query parameter spec: ', param_operands))
        }
        ret = paste0(ret, wrapper_to_fusion_lookup[param_operands[1]])
    } else if(length(param_operands) == 2 && param_operands[1] %in% names(wrapper_to_fusion_lookup)) {
        ret = paste0(ret, wrapper_to_fusion_lookup[param_operands[1]], ',', param_operands[2])
    } else {
        stop(paste0('Invalid query parameter spec: ', param_operands))
    }
    ret = paste0(ret, ']')
    return(ret)
}

#' Translate a raw query looked up from a query library into a GCAM Fusion query.
#'
#' Raw queries will contain placeholders such as \code{'{arg_tag@arg_type}'} and those
#' will get replaced user supplied filter options supplied as a list in \code{query_params}
#' where we match those keys to `arg_type`.  If `arg_type` is "name" then \link{parse_str_query_param}
#' will be used to process the value of query_params[[arg_tag]] and if "year" then
#' \link{parse_int_query_param} is used.
#' Note symantics are slightly different if is_get_data is true as described in parse_.*_query_params.
#' For any arg_tag which has no entry in query_params it will be given the results of passing `NULL`
#' to parse_int/str_query_param.
#' @param query The raw query which needs to have it's placeholders translated.
#' @param query_params The user options provided as a list of arg_tags to and array of
#' operators and potentially operands which will get translated to GCAM Fusion syntax.
#' @param is_get_data A boolean if true follows get data symantics and set data if false.
#' @return A translated query into GCAM Fusion syntax that is ready to run.
#' @importFrom stringr str_glue_data
apply_query_params <- function(query, query_params, is_get_data) {
    query_params_orig <- query_params
    names(query_params)[names(query_params_orig) == ""] <- query_params_orig[names(query_params_orig) == ""]
    query_params[names(query_params_orig) == ""] <- NA_character_
    placeholders = find_placeholders(query)
    parsed_params = list()
    for(param in names(query_params)) {
        args = query_params[[param]]
        if(!param %in% names(placeholders)) {
            warning(paste0(param,' has no placeholders in ',query))
        } else {
            # note error checking on placeholder types has already occurred
            parsed_params[param] = ifelse(placeholders[[param]] == 'year', parse_int_query_param(args, is_get_data), parse_str_query_param(args, is_get_data))
        }
    }

    # double check if we have any placeholders for which the user did not explicitly
    # provide a parameter
    for(param in names(placeholders)) {
        if(!param %in% names(query_params)) {
            # if no param was provided get the default value by passing NULL to the
            # appropriate parse_XXX_query_param
            parsed_params[param] = ifelse(placeholders[[param]] == 'year', parse_int_query_param(NULL, is_get_data), parse_str_query_param(NULL, is_get_data))
        }
    }

    return(str_glue_data(parsed_params, gsub('@.*?}', '}', query)))
}

