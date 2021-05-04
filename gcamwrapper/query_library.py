import yaml
import warnings
import re
import os.path as path
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

def get_query(*args, query_file = None):
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
            raise Exception("Could not find query: ", args)

    if len(query) == 0:
        raise Exception("Could not find query: ", args)
    query_str = Query(query[0])
    if len(query) > 1:
        query_str.units = query[1]

    return query_str

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
            raise Exception('Invalid placeholder syntax: ', placeholder, 'expecting two values split by @')
        elif ph_split[1] != 'name' and ph_split[1] != 'year':
            raise Exception('Invalid placeholder syntax, unknown type: ', ph_split[1], 'expecting name or year')
        else:
            ret[ph_split[0]] = ph_split[1]

    return ret

def parse_int_query_param(param_operands, is_get_data):
    '''Parse user options for integer operators and generate GCAM Fusion syntax.
       The currently supported operators are:
       `+`: Indicates to read/write to a DataFrame.  Note if `is_get_data` this is always
       implied to be set.
       `*`: which always matches and so no additional operands are necessary (note if
       param_operands is empty this operator is assumed)
       The standard comparison operators: `=`, `<`, `<=`, `>`, `>=`. Note if `is_get_data` is true
       or the `+` is not set an additional argument must be supplied which is the integer (or a
       string that can be converted to integer) to be used as the RHS operand in the comparison.

    :param param_operands: An array containing operators and potentially an operand to be used with that
                           operator
    :type param_operands: array of str
    :param is_get_data: A boolean if true follows get data symantics and set data if false
    :type is_get_data: boolean

    :returns:  A GCAM Fusion filter string representing the parameters given
    '''

    wrapper_to_fusion_lookup = {'*': 'MatchesAny', '<': 'IntLessThan', '<=': 'IntLessThanEq', '>': 'IntGreaterThan', '>=': 'IntGreaterThanEq', '=': 'IntEquals'}
    try:
        plus_index = param_operands.index('+')
        plus_op = param_operands.pop(plus_index)
    except (ValueError, AttributeError) as e:
        plus_op = '+' if is_get_data else ''
    ret = '[' + plus_op
    if param_operands is None or len(param_operands) == 0 or param_operands[0] == '*':
        ret += 'YearFilter,' + wrapper_to_fusion_lookup['*']
    elif not is_get_data and plus_op == '+':
        if len(param_operands) < 1:
            raise Exception('Invalid query parameter spec: ', param_operands)
        ret += 'YearFilter,' + wrapper_to_fusion_lookup[param_operands[0]]
    elif len(param_operands) == 2 and param_operands[0] in wrapper_to_fusion_lookup.keys():
        try:
            operandAsInt = int(param_operands[1])
        except ValueError:
            raise Exception('Expecting integer operand, got: ', param_operands)
        # if the int value looks like a date assume YearFilter otherwise it is
        # a model period
        ret += 'YearFilter,' if operandAsInt > 1000 else 'IndexFilter,'
        ret += wrapper_to_fusion_lookup[param_operands[0]] + ',' + str(operandAsInt)
    else:
        raise Exception('Invalid query parameter spec: ', param_operands)
    ret += ']'
    return ret

def parse_str_query_param(param_operands, is_get_data):
    '''Parse user options for string operators and generate GCAM Fusion syntax
       The currently supported operators are:
       `+`: Indicates to read/write to a DataFrame.  Note if `is_get_data` this is always
       implied to be set.
       `*`: which always matches and so no additional operands are necessary (note if
       param_operands is empty this operator is assumed)
       The operators: `=`, `=~` (regular expression matching). Note if `is_get_data` is true
       or the `+` is not set an additional argument must be supplied which is the string to
       be used as the RHS operand in the comparison.

    :param param_operands: An array containing operators and potentially an operand to be used with that
                           operator
    :type param_operands: array of str
    :param is_get_data: A boolean if true follows get data symantics and set data if false
    :type is_get_data: boolean

    :returns:  A GCAM Fusion filter string representing the parameters given
    '''

    wrapper_to_fusion_lookup = {'*': 'MatchesAny', '=': 'StringEquals','=~': 'StringRegexMatches'}
    try:
        plus_index = param_operands.index('+')
        plus_op = param_operands.pop(plus_index)
    except (ValueError, AttributeError) as e:
        plus_op = '+' if is_get_data else ''
    ret = '[' + plus_op + 'NamedFilter,'
    if param_operands is None or len(param_operands) == 0 or param_operands[0] == '*':
        ret += wrapper_to_fusion_lookup['*']
    elif not is_get_data and plus_op == '+':
        if len(param_operands) < 1:
            raise Exception('Invalid query parameter spec: ', param_operands)
        ret += wrapper_to_fusion_lookup[param_operands[0]]
    elif len(param_operands) == 2 and param_operands[0] in wrapper_to_fusion_lookup.keys():
        ret += wrapper_to_fusion_lookup[param_operands[0]] + ',' + param_operands[1]
    else:
        raise Exception('Invalid query parameter spec: ', param_operands)
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
       For any arg_tag which has no entry in query_params it will be replaced with nothing
       which tells GCAM Fusion to match any but do not read/write to the DataFrame for that container.

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
            parsed_params[param+'@'+placeholders[param]] = parse_int_query_param(args, is_get_data) if placeholders[param] == 'year' else parse_str_query_param(args, is_get_data)

    # TODO: better syntax?
    for param, ptype in placeholders.items():
        if not param in query_params.keys():
            parsed_params[param+'@'+ptype] = ''

    return query.format(**parsed_params)


if __name__ == '__main__':

    config_file = 'get_data_queries.yml'
    yr = 2020
    #config = read_yaml(config_file)
    # get the query for CO2 emissions for year yr
    query_str = get_query('emissions', 'co2_emissions')#.format(region='USA', year=yr)
    print('Raw query:')
    print(query_str)

    # replaces all tqgs
    print('Set all args')
    print(apply_query_params(query_str, { 'region': ['=', 'USA'], 'year': ['=', yr] }, True))
    # omits region so should 'collapse' it
    print('Omit region')
    print(apply_query_params(query_str, { 'year': ['=', yr] }, True))
    print('Omit region, set data')
    print(apply_query_params(query_str, { 'year': ['=', yr] }, False))
    print(apply_query_params(query_str, { 'year': ['+', '=', yr] }, False))
    # tries to set some unknown tag so should get warning
    print('Set unknown instead of year:')
    print(apply_query_params(query_str, { 'region': ['=~', 'USA'], 'asdf': ['=', yr] }, False))
