import yaml
import warnings
import re
import pkg_resources

def read_yaml(yaml_file):
    '''Read a YAML file.
       
    :param yaml_file: The file path to the YAML file to load
    :type yaml_file: str

    :returns: A dict representing the YAML data
    '''

    with open(yaml_file, 'r') as yml:
        return yaml.load(yml, Loader=yaml.FullLoader)

'''The parsed query file included with the package'''
PACKAGE_QUERIES = yaml.load(pkg_resources.resource_stream(__name__, 'query_library.yml'), Loader=yaml.FullLoader)

class Query(str):
    '''A Simple extension to str to be able to associate units meta data'''
    units: None

class QuerySyntaxException(Exception):
    '''An Exception type used to signal gcamwrapper Query syntax error either in the
       Query itself or in the place holder replacements provided by the users.
    '''
    pass

def get_query(*args, query_file=None):
    '''Look up a query from a YAML file.
       The YAML query files will often be organized into nested categories
       so users must provide the full path as individual strings such as:
       \code{get_query("emissions", "co2_emissions")}

    :param query_file: The path to a YAML file to read if not None otherwise
                       the package query file will be used.
    :type query_file: str
    :param *args: Strings specifiying the path to the query to lookup
    :type *args: str

    :returns: The raw query from the query file with units set as an attribute
              if specified
    '''

    if query_file is None:
        queries = PACKAGE_QUERIES
    else:
        queries = read_yaml(query_file)
    query = queries
    for path in args:
        try:
            query = query[path]
        except KeyError:
            raise(f"Could not find query:  {args}") 

    if len(query) == 0:
        raise Exception(f"Could not find query:  {args}")
    query_str = Query(query[0])
    if len(query) > 1:
        query_str.units = query[1]
    if len(query) > 2:
        warnings.warn(f"Additional elements for {args} are ignored, expecting only <query> <units>")

    return query_str

# TODO: Remove when we build Sphinx docs
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

def find_placeholders(query_str):
    '''Find all placeholder in a raw query and return them in a list.
       Find query placeholders of the form \code{'{tag@type}'} and put them into a
       dict such as \code{dict('tag1': 'name', 'tag2': 'year')} for instance.

    :param query_str: A raw query string
    :type query_str: str

    :returns: A dict of all the placeholders found in the query where the keys
              are the tags and the values are the types
    '''

    raw_placeholders = re.findall(r'{([\w@]+)}', query_str)
    ret = dict()
    for placeholder in raw_placeholders:
        ph_split = placeholder.split('@')
        if len(ph_split) != 2:
            raise QuerySyntaxException(f"Invalid placeholder syntax: {placeholder} expecting two values split by @")
        elif ph_split[1] != 'name' and ph_split[1] != 'year':
            raise QuerySyntaxException(f"Invalid placeholder syntax, unknown type: {ph_split[1]} expecting name or year")
        else:
            ret[ph_split[0]] = ph_split[1]

    if len(raw_placeholders) != len(ret):
        raise QuerySyntaxException(f"Duplicate placeholder tags in {query_str}")

    return ret

def parse_int_query_param(param_operands, is_get_data):
    '''Parse user options for integer operators and generate GCAM Fusion syntax.
       The currently supported operators are:
       `+`, `-`: Indicates to read/write to a DataFrame if `+` or not to read/write if `-`
       `*`: which always matches and so no additional operands are necessary
       The standard comparison operators: `=`, `<`, `<=`, `>`, `>=`. Note if `is_get_data` is true
       or the `-` is set an additional argument must be supplied which is the integer (or a
       string that can be converted to integer) to be used as the RHS operand in the comparison.
       Finally if param_operands is NULL or empty then if `is_get_data` then ['+', '*'] is assumed
       otherwise ['+', '='].

    :param param_operands: An array containing operators and potentially an operand to be used with that
                           operator
    :type param_operands: array of str
    :param is_get_data: A boolean if true follows get data symantics and set data if false
    :type is_get_data: boolean

    :returns:  A GCAM Fusion filter string representing the parameters given
    '''

    wrapper_to_fusion_lookup = {'*': 'MatchesAny', '<': 'IntLessThan', '<=': 'IntLessThanEq', '>': 'IntGreaterThan', '>=': 'IntGreaterThanEq', '=': 'IntEquals'}
    # the default behavior is to set the '+' operator
    plus_op = '+'
    try:
        minus_index = param_operands.index('-')
        # if we do not get an exception that means it was set
        plus_op = ''
        # try to remove the - (if set) for easier error checking later
        param_operands.pop(minus_index)
    except (ValueError, AttributeError):
        pass

    try:
        # try to remove the + (if set) for easier error checking later
        plus_index = param_operands.index('+')
        # this handles the case if both - and + are set the + takes prescedance
        plus_op = param_operands.pop(plus_index)
    except (ValueError, AttributeError):
        pass

    ret = '[' + plus_op
    # use default behavior if no param_operands were given
    if param_operands is None or len(param_operands) == 0:
        if is_get_data:
            # for get data the default is to match any
            param_operands = ['*']
        else:
            # for set data the default is to match =
            param_operands = ['=']

    if param_operands[0] == '*':
        if not is_get_data and plus_op == '+':
            raise QuerySyntaxException(f"Using * without explictly not reading from columns with - is not valid in set_data: {param_operands}")
        ret += 'YearFilter,' + wrapper_to_fusion_lookup['*']
    elif not is_get_data and plus_op == '+':
        if len(param_operands) < 1:
            raise QuerySyntaxException(f"Invalid query parameter spec: {param_operands}")
        ret += 'YearFilter,' + wrapper_to_fusion_lookup[param_operands[0]]
    elif len(param_operands) == 2 and param_operands[0] in wrapper_to_fusion_lookup.keys():
        try:
            operandAsInt = int(param_operands[1])
        except ValueError:
            raise QuerySyntaxException(f"Expecting integer operand, got: {param_operands}")
        # if the int value looks like a date assume YearFilter otherwise it is
        # a model period
        if operandAsInt > 1000:
            ret += 'YearFilter,'
        else:
            ret += 'IndexFilter,'
        ret += wrapper_to_fusion_lookup[param_operands[0]] + ',' + str(operandAsInt)
    else:
        raise QuerySyntaxException(f"Invalid query parameter spec: {param_operands}")
    ret += ']'
    return ret

def parse_str_query_param(param_operands, is_get_data):
    '''Parse user options for string operators and generate GCAM Fusion syntax
       The currently supported operators are:
       `+`, `-`: Indicates to read/write to a DataFrame if `+` or not to read/write if `-`
       `*`: which always matches and so no additional operands are necessary
       The operators: `=`, `=~` (regular expression matching). Note if `is_get_data` is true
       or the `-` is set an additional argument must be supplied which is the string to
       be used as the RHS operand in the comparison.
       Finally if param_operands is NULL or empty then if `is_get_data` then ['+', '*'] is assumed
       otherwise ['+', '='].

    :param param_operands: An array containing operators and potentially an operand to be used with that
                           operator
    :type param_operands: array of str
    :param is_get_data: A boolean if true follows get data symantics and set data if false
    :type is_get_data: boolean

    :returns:  A GCAM Fusion filter string representing the parameters given
    '''

    wrapper_to_fusion_lookup = {'*': 'MatchesAny', '=': 'StringEquals','=~': 'StringRegexMatches'}
    # the default behavior is to set the '+' operator
    plus_op = '+'
    try:
        minus_index = param_operands.index('-')
        # if we do not get an exception that means it was set
        plus_op = ''
        # try to remove the - (if set) for easier error checking later
        param_operands.pop(minus_index)
    except (ValueError, AttributeError):
        pass

    try:
        # try to remove the + (if set) for easier error checking later
        plus_index = param_operands.index('+')
        # this handles the case if both - and + are set the + takes prescedance
        plus_op = param_operands.pop(plus_index)
    except (ValueError, AttributeError):
        pass

    ret = '[' + plus_op + 'NamedFilter,'
    # use default behavior if no param_operands were given
    if param_operands is None or len(param_operands) == 0:
        if is_get_data:
            # for get data the default is to match any
            param_operands = ['*']
        else:
            # for set data the default is to match =
            param_operands = ['=']

    if param_operands[0] == '*':
        if not is_get_data and plus_op == '+':
            raise QuerySyntaxException(f"Using * without explictly not reading from columns with - is not valid in set_data: {param_operands}")
        ret += wrapper_to_fusion_lookup['*']
    elif not is_get_data and plus_op == '+':
        if len(param_operands) < 1:
            raise QuerySyntaxException(f"Invalid query parameter spec: {param_operands}")
        ret += wrapper_to_fusion_lookup[param_operands[0]]
    elif len(param_operands) == 2 and param_operands[0] in wrapper_to_fusion_lookup.keys():
        ret += wrapper_to_fusion_lookup[param_operands[0]] + ',' + param_operands[1]
    else:
        raise QuerySyntaxException(f"Invalid query parameter spec: {param_operands}")
    ret += ']'
    return ret

def apply_query_params(query, query_params, is_get_data):
    '''Translate a raw query looked up from a query library into a GCAM Fusion query.
       Raw queries will contain placeholders such as \code{'{arg_tag@arg_type}'} and those
       will get replaced user supplied filter options supplied as a list in \code{query_params
       where we match those keys to `arg_type`.  If `arg_type` is "name" then `parse_str_query_param`
       will be used to process the value of query_params[[arg_tag]] and if "year" then
       `parse_int_query_param` is used.
       Note symantics are slightly different if is_get_data is true as described in
       parse_.*_query_params functions.
       For any arg_tag which has no entry in query_params it will be given the results of passing `None`
       to parse_int/str_query_param.

    :param query: The raw query which needs to have it's placeholders translated.
    :type query: str
    :param query_params: The user options provided as a list of arg_tags to and array of
                         operators and potentially operands which will get translated to GCAM
                         Fusion syntax.
    :type query_params: dict of str to array
    :param is_get_data: A boolean if true follows get data symantics and set data if false.
    :type is_get_data: boolean

    :returns: A translated query into GCAM Fusion syntax that is ready to run.
    '''

    placeholders = find_placeholders(query)
    parsed_params = dict()
    for param, args in query_params.items():
        if not param in placeholders.keys():
            warnings.warn(param+' has no placeholders in '+query)
        else:
            # note error checking on placeholder types has already occurred
            if placeholders[param] == 'year':
                parsed_params[param+'@'+placeholders[param]] = parse_int_query_param(args, is_get_data)
            else:
                parsed_params[param+'@'+placeholders[param]] = parse_str_query_param(args, is_get_data)

    # double check if we have any placeholders for which the user did not explicitly
    # provide a parameter
    for param, ptype in placeholders.items():
        if not param in query_params.keys():
            # if no param was provided get the default value by passing None to the
            # appropriate parse_XXX_query_param
            if ptype == 'year':
                parsed_params[param+'@'+ptype] = parse_int_query_param(None, is_get_data)
            else:
                parsed_params[param+'@'+ptype] = parse_str_query_param(None, is_get_data)

    return query.format(**parsed_params)

