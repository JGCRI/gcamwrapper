import gcam_module
from pandas import DataFrame, Series

class gcam (gcam_module.gcam):

    def getData(self, query):
        data_dict = super(gcam, self).getData(query)
        return DataFrame(data_dict)

    def setData(self, data_df, query):
        data_dict = dict()
        for key, value in data_df.items():
            data_dict[key] = value.to_numpy()
        super(gcam, self).setData(data_dict, query)

    def createSolutionDebugger(self, period):
        sd = super(gcam, self).createSolutionDebugger(period)
        sd.__class__ = SolutionDebugger
        return sd

class SolutionDebugger (gcam_module.SolutionDebugger):

    def getPrices(self, scaled):
        return Series(super(SolutionDebugger, self).getPrices(scaled),
                      super(SolutionDebugger, self).getMarketNames())

    def getFX(self):
        return Series(super(SolutionDebugger, self).getFX(),
                      super(SolutionDebugger, self).getMarketNames())

    def getSupply(self, scaled):
        return Series(super(SolutionDebugger, self).getSupply(scaled),
                      super(SolutionDebugger, self).getMarketNames())

    def getDemand(self, scaled):
        return Series(super(SolutionDebugger, self).getDemand(scaled),
                      super(SolutionDebugger, self).getMarketNames())

    def getPriceScaleFactor(self):
        return Series(super(SolutionDebugger, self).getPriceScaleFactor(),
                      super(SolutionDebugger, self).getMarketNames())

    def getQuantityScaleFactor(self):
        return Series(super(SolutionDebugger, self).getQuantityScaleFactor(),
                      super(SolutionDebugger, self).getMarketNames())

    def setPrices(self, prices, scaled):
        super(SolutionDebugger, self).setPrices(prices.to_numpy(), scaled)

    def evaluate(self, prices, scaled, reset):
        return Series(super(SolutionDebugger, self).evaluate(prices.to_numpy(), scaled, reset),
                      super(SolutionDebugger, self).getMarketNames())

    def evaluatePartial(self, price, index, scaled):
        return Series(super(SolutionDebugger, self).evaluatePartial(price, index, scaled),
                      super(SolutionDebugger, self).getMarketNames())

    def calcDerivative(self):
        return DataFrame(super(SolutionDebugger, self).calcDerivative(),
                         index=super(SolutionDebugger, self).getMarketNames(),
                         columns=super(SolutionDebugger, self).getMarketNames())

    def getSlope(self):
        return Series(super(SolutionDebugger, self).getSlope(),
                      super(SolutionDebugger, self).getMarketNames())

    def setSlope(self, slope):
        super(SolutionDebugger, self).setSlope(slope.to_numpy())

