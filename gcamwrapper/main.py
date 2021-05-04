import gcam_module
from pandas import DataFrame, Series
from gcamwrapper.query_library import apply_query_params


class Gcam (gcam_module.gcam):
    """A wrapper around GCAM to interactively run a scenario and use
       GCAMFusion capabilities to get/set arbitrary data from a running
       instance.
    """

    def get_data(self, query, query_params = None):
        """Queries for arbitrary data from a running instance of GCAM.

        :param query:   GCAM fusion query
        :type query:    str
        :param query_params: User options to translate placeholder expressions in query should
                             it have any
        :type query_params:  dict(string: array(string))

        :returns:       DataFrame with the query results.

        """

        units = query.units if hasattr(query, "units") else None
        if query_params is not None:
            query = apply_query_params(query, query_params, True)
        data_dict = super(Gcam, self).get_data(query)
        data_df = DataFrame(data_dict)
        # The data comming out of gcam is unaggregated so we will need to do that now
        # first figure out what the "value" column is, group by everything else, and summarize
        # TODO: decide on failure mode when no results
        cols = data_df.columns
        value_col = cols[-2] if cols[-1] == "year" else cols[-1]
        data_df = data_df.groupby(cols.drop(value_col).to_list(), as_index=False).sum()
        if units is not None:
            data_df.meta = {'units': units}
        return data_df

    def set_data(self, data_df, query, query_params = None):
        """Changes arbitrary data in a running instance of GCAM.

        :param data_df:     DataFrame of data to set
        :type data_df:      DataFrame
        :param query:       GCAM fusion query
        :param query_params: User options to translate placeholder expressions in query should
                             it have any
        :type query_params:  dict(string: array(string))
        :type query:        str

        """

        if query_params is not None:
            query = apply_query_params(query, query_params, False)
        data_dict = dict()
        for key, value in data_df.items():
            data_dict[key] = value.to_numpy()
        super(Gcam, self).set_data(data_dict, query)

    def get_current_period(self):
        """Get the last run GCAM model period

        :returns: The last period used in `run_to_period` wheter it succeeded or failed
        """

        return super(Gcam, self).get_current_period()

    def convert_period_to_year(self, period):
        """Convert from a GCAM model period to year

        :param period: The model period to convert to year
        :type period: integer or list of integer

        :returns: The corresponding model year(s)
        """

        if type(period) == int:
            return super(Gcam, self).convert_period_to_year(period)
        else:
            return list(map(lambda x: self.convert_period_to_year(x), period))

    def convert_year_to_period(self, year):
        """Convert from a GCAM model year to model period

        :param year: The model year to convert to period
        :type year:integer or list of integer

        :returns: The corresponding model period(s)
        """

        if type(year) == int:
            return super(Gcam, self).convert_year_to_period(year)
        else:
            return list(map(lambda x: self.convert_year_to_period(x), year))

    def create_solution_debugger(self, period = None):
        """Create a solution debugging object which can be used a single
           evaluation of the model and see how it affects prices, supplies,
           and demands amongst other things..

        :param period:   GCAM model period to create the debugger
        :type period:    integer

        :returns:        A SolutionDebugger object

        """

        if period is None:
            period = self.get_current_period()
        sd = super(Gcam, self).create_solution_debugger(period)
        sd.__class__ = SolutionDebugger
        return sd


class SolutionDebugger (gcam_module.SolutionDebugger):
    """An object that exposes certain parts of the GCAM solver allowing users
       to debug solution issues or just arbitrarily poke the model and see what
       happens.
    """

    def get_prices(self, scaled):
        """Gets the prices of solvable markets as a Series with market names
           as the index.

        :param scaled:  If the prices should be returned in the scaled form (native
                        to the solver) or unscaled (native to the model).
        :type scaled:   boolean

        :returns:       A Series of prices indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_prices(scaled),
                      super(SolutionDebugger, self).get_market_names())

    def get_fx(self):
        """Gets the F(X), or supply - demand, of solvable markets as a Series
           with market names as the index.

        :returns:       A Series of F(x) indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_fx(),
                      super(SolutionDebugger, self).get_market_names())

    def get_supply(self, scaled):
        """Gets the supply of solvable markets as a Series with market names
           as the index.

        :param scaled:  If the supply should be returned in the scaled form (native
                        to the solver) or unscaled (native to the model).
        :type scaled:   boolean

        :returns:       A Series of supplies indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_supply(scaled),
                      super(SolutionDebugger, self).get_market_names())

    def get_demand(self, scaled):
        """Gets the demand of solvable markets as a Series with market names
           as the index.

        :param scaled:  If the demands should be returned in the scaled form (native
                        to the solver) or unscaled (native to the model).
        :type scaled:   boolean

        :returns:       A Series of demands indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_demand(scaled),
                      super(SolutionDebugger, self).get_market_names())

    def get_price_scale_factor(self):
        """Gets the price scale factor which attempts to normalize GCAM prices
           to ideally be around 1.

        :returns:   A Series of price scale factors indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_price_scale_factor(),
                      super(SolutionDebugger, self).get_market_names())

    def get_quantity_scale_factor(self):
        """Gets the quantity scale factor which attempts to normalize GCAM supplies
           and demands to ideally be around 1.

        :returns:   A Series of supply/demand scale factors indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_quantity_scale_factor(),
                      super(SolutionDebugger, self).get_market_names())

    def set_prices(self, prices, scaled):
        """Sets a Series of prices into the model but does not immediately evaluate them.

        :param prices:  A Series of prices to be set.
        :type prices:   Series
        :param scaled:  If the given prices are already scaled or not.  If they are not
                        they will be before getting set into the solver.
        :type scaled:   boolean

        """

        super(SolutionDebugger, self).set_prices(prices.to_numpy(), scaled)

    def evaluate(self, prices, scaled, reset):
        """Sets a Series of prices into the model and evaluate them and returns the
           resulting F(x).

        :param prices:  A Series of prices to be set.
        :type prices:   Series
        :param scaled:  If the given prices are already scaled or not.  If they are not
                        they will be before getting set into the solver.
        :type scaled:   boolean
        :param reset:   If the STATE of the model should be reset after doing this evaluation.
        :type reset:    boolean

        :returns:   A Series of the F(x) which results from the evaluation.

        """

        return Series(super(SolutionDebugger, self).evaluate(prices.to_numpy(), scaled, reset),
                      super(SolutionDebugger, self).get_market_names())

    def evaluate_partial(self, price, index, scaled):
        """Sets a single price into the model and evaluate it and returns the resulting F(x).
           This is similar to calling evauluation with reset = True however it can optimize
           by knowing only a single market is changing and therefore only calculate the subset
           of the model which is affected which can be significantly faster to evaluate.

        :param price:   A price to be set.
        :type price:    double
        :param index:   The numeric offset in which to set the price.
        :type scaled:   integer
        :param scaled:  If the given price is already scaled or not.  If it is not
                        it will be before getting set into the solver.
        :type scaled:   boolean

        :returns:   A Series of the F(x) which results from the evaluation.

        """

        return Series(super(SolutionDebugger, self).evaluate_partial(price, index, scaled),
                      super(SolutionDebugger, self).get_market_names())

    def calc_derivative(self):
        """Calculates the Jacobian matrix from the set of prices currently set in
           the solver.  This is a finite difference derivative for each market
           represented by column.

        :returns:   A DataFrame, matrix where the column and rows are indexed by
                    market names.
        """

        return DataFrame(super(SolutionDebugger, self).calc_derivative(),
                         index=super(SolutionDebugger, self).get_market_names(),
                         columns=super(SolutionDebugger, self).get_market_names())

    def get_slope(self):
        """Get the "correction" slope, which is used by the solver to give
           continous behavior when a price for a market falls below the
           "lower bound supply" price, or the price below which GCAM would
           produce no supply.  The idea being to add a negative supply to
           ensure there is always continous behavior and we would ideally match
           the slope to what it was just above the "lower bound price" so the
           solver can move back into the proper range of prices.

        :returns:   A Series of the correction slope indexed by market name.

        """

        return Series(super(SolutionDebugger, self).get_slope(),
                      super(SolutionDebugger, self).get_market_names())

    def set_slope(self, slope):
        """ Set the "correction" slope Series into the model.

        :param slope:   A Series of negative correction slopes to be set.
        :type slope:    Series

        """

        super(SolutionDebugger, self).set_slope(slope.to_numpy())

