# A R / Python Package Wrapper Around The Global Change Analysis Model (GCAM)

The gcamwrapper contains all C++ and R/python source code to wrap the [GCAM](https://github.com/JGCRI/gcam-core) model such that simulations can be run interactively.  Users can query a running instance of GCAM to get and set arbitrary parameters and outputs.  In addition it includes methods to interact with the GCAM solver to diagnose solution issues or just "poke" the model to see how it reacts.

## Building the package

This package **does not** contain the GCAM model.  In other words users still need to [build GCAM](http://jgcri.github.io/gcam-doc/gcam-build.html) and [run the data system](http://jgcri.github.io/gcam-doc/data-system.html) before building this package as it will need to compile against it.  In addition it also implies an install of this package is tied to that specific version of GCAM and users should not expect it to be compatible with any other versions of GCAM.

### Changes required in GCAM

At the time of this writing one change is required to GCAM itself to be compatible with gcamwrapper which is to modify `cvs/objects/containers/include/scenario.h`:
```diff
diff --git a/cvs/objects/containers/include/scenario.h b/cvs/objects/containers/include/scenario.h
index 7f93c85b9..43d980916 100644
--- a/cvs/objects/containers/include/scenario.h
+++ b/cvs/objects/containers/include/scenario.h
@@ -88,6 +88,7 @@ class ManageStateVariables;
 class Scenario: public IParsable, public IVisitable
 {
     friend class LogEDFun;
+    friend class gcam;
 public:
     Scenario();
     ~Scenario();
```

### Creating a libgcam to link to

At the time of this writing some build systems may not generate a suitable libgcam, a compiled library of GCAM's C++ source, to link gcamwrapper to.  However some simple steps could be taken to create it depending on which system you are using to compile GCAM:

#### Makefile

The standalone Makefiles do in fact create a static library in the process of creating the `gcam.exe`.  That being said, on Linux you may have to compile GCAM with the flag `-fPIC`.  The easiest way to do that is to include it with the C++ compiler environment variables:
```bash
export CXX=g++ -fPIC
```

#### Xcode

To create a static library you can run the following command:
```bash
ar -ru libgcam.a exe/objects.build/Release/objects.build/Objects-normal/x86_64/*o
```

And for the sake of consistency with `Makefile` move them and libhector into:
```bash
mv libgcam.a cvs/objects/build/linux/
cp cvs/objects/climate/source/hector/project_files/Xcode/build/Release/libhector-lib.a cvs/objects/build/linux/libhector.a
```

#### Visual Studio

To create a static library you can run the following command:
```bat
cd exe
lib /out:gcam.lib Release\*.obj
```

And copy the libhector into the same place:
```bat
copy ..\cvs\objects\build\vc10\x64\Release\hector-lib.lib hector.lib
```

### Setting up the environment variables to build gcamwrapper
Both R and Python builds are configured to find GCAM and the third party libraries which GCAM uses by checking the environment variables noted below.  TODO: R on windows uses `mingw` as it's C++ compiler (a Windows build of GCC) and it seems unlikely it will be able to link with a Visual Studio built library.  In addition not until R 4.0 does it use a version of `mingw` new enough to support the C++ 14 standard which GCAM uses.

Python users will need to compile Boost.python if they have not done so already.  To do so they should ensure `python` is found in their `PATH` and that the `numpy` package has been installed.  Then they can simply:
```
cd <path to boost library>
./b2 --with-python address-model=64 stage
```

If your `python` executable is called something else, such as `python3` you will need to create a file called `project-config.jam` with something like:
```
import python ;
if ! [ python.configured ]
{ 
    using python : 3.9 : python3 :  ;
}
```

#### Unix

The following environment variables must be set.  The paths below are an example and you should substitute the appropriate paths for your system.
```bash
export GCAM_INCLUDE=/Users/Pralit/models/gcam-core/cvs/objects
export GCAM_LIB=/Users/Pralit/models/gcam-core/cvs/objects/build/linux
export BOOST_INCLUDE=/Users/Pralit/models/gcam-core/libs/boost_1_67_0
export BOOST_LIB=/Users/Pralit/models/gcam-core/libs/boost_1_67_0/stage/lib
export XERCES_INCLUDE=/Users/Pralit/models/gcam-core/libs/xercesc/include
export XERCES_LIB=/Users/Pralit/models/gcam-core/libs/xercesc/lib
export JAVA_INCLUDE=/Library/Java/JavaVirtualMachines/jdk-12.0.1.jdk/Contents/Home/include
export JAVA_LIB=/Library/Java/JavaVirtualMachines/jdk-12.0.1.jdk/Contents/Home/lib/server
export CXX='c++ -std=c++14'
```

At this point you can build and install with:
```bash
cd gcamwrapper
# To build the R package:
R CMD install .
$ To build the Python package:
pip3 install .
```
Note: Rstudio can be used to build as well and I have had luck opening the project from the terminal with something like `open gcamwrapper.Rproj` and having it inherit the necessary environment variables set above.  Although I think it may be an undocumented feature.

#### Windows

On Windows, first you will need to launch the "x64 Native Tools Command Prompt" to ensure all of the Visual Studio environment is set up to compile the gcamwrapper package.  In addition you should ensure `python` and `pip` are found on your `PATH`.  
The following environment variables must be set.  The paths below are an example and you should substitute the appropriate paths for your system.
```bat
SET GCAM_INCLUDE=C:\GCAM\gcam-core\cvs\objects
SET GCAM_LIB=C:\GCAM\gcam-core\exe
SET BOOST_INCLUDE=C:\GCAM\libs\boost-lib
SET BOOST_LIB=C:\GCAM\libs\boost-lib\stage\lib
SET XERCES_INCLUDE=C:\GCAM\libs\xercesc\include
SET XERCES_LIB=C:\GCAM\libs\xercesc\lib
SET JAVA_INCLUDE=C:\Program Files\Java\jdk-15.0.1\include
SET JAVA_LIB=C:\Program Files\Java\jdk-15.0.1\lib
```

At this point you can build and install with:
```bat
pip install .
```

### A simple example on using gcamwrapper

The following is an example R script:
```R
devtools::load_all()
library(dplyr)
library(tidyr)
library(ggplot2)

# Create a Gcam instance by giving it a configuration file and the
# appropriate working directory
g <- create_and_initialize("configuration_ref.xml", "/Users/pralitp/model/gcam-core-git/exe")
# Run the model up to period 5, the year 2020
run_to_period(g, 5L)

# Do a simple experiment: see how CO2 emissions change after arbitrarily changing
# the GDP

# First save the CO2 emissions before
# Note we use the [GCAM Fusion](http://jgcri.github.io/gcam-doc/dev-guide/examples.html)
# syntax to query results with a few additional features:
# If we include a `+` in a filter it is interpreted by gcamwrapper to
# mean we want to record the name/year of the object seen at the step
# We add a filter predicate `MatchesAny` which always matches

# To ease the burden on users to write queries, we include a query library which is specified
# in `inst/extdata/query_library.yml` and can be accessed with the `get_query` utility:
co2_query <- get_query("emissions", "co2_emissions")
# which returns: world/region{region@name}//ghg[NamedFilter,StringEquals,CO2]/emissions{year@year}
# But note the filters on region and emissions year are just place holders.  To simplify this
# aspect as well we allow users to specify query parameters using a short hand notation provided
# as a list:
query_params <- list(
    "region" = # The key is the place holder "tag"
        c("=", "USA"), # The value is an array with the first value an operator and the second the
                       # RHS operand.  Note for get_data the "+" is implied but could be added explicitly
                       # The available operators include:
                       # For strings (@name): "*" (any) "="  or "=~" (regular expression matches)
                       # For ints (@year): "*" (any), "=", "<", "<=", ">", ">="
    "year" = c("=", 2020))
# The placeholders will then get transformed into:
# "world/region[+NamedFilter,MatchesAny]//ghg[+NamedFilter,StringEquals,CO2]/emissions[+YearFilter,IntEquals,2020]")
# We can then pass the query and query_params and retrieve the results in a DataFrame.
# Note: if no query_params are given we assume the user is providing GCAM Fusion query
# and no placeholders need to be translated.
co2_core <- get_data(g, co2_query, query_params)
# Returns A tibble: 32 x 4
#   region                        ghg    year emissions
#   <chr>                         <chr> <int>       <dbl>
# 1 Africa_Eastern                CO2    2020        27.7
# 2 Africa_Northern               CO2    2020       159. 
# 3 Africa_Southern               CO2    2020        24.5
# 4 Africa_Western                CO2    2020        53.9
# 5 Argentina                     CO2    2020        54.6
# 6 Australia_NZ                  CO2    2020       131. 
# 7 Brazil                        CO2    2020       146. 
# 8 Canada                        CO2    2020       165. 
# 9 Central America and Caribbean CO2    2020        59.7
#10 Central Asia                  CO2    2020       153. 
# ... with 22 more rows

# get the current GDP labor productivity value, note: laborproductivity is a vector by year
# and since we didn't explicitly filter we will get a value for all years AND the accompanied
# year column
labor_prod_query <- get_query("socioeconomic", "labor_productivity")
# note the shorthand here, when we have just the key and no value it assumes you want +MatchesAny
labor_prod <- get_data(g, labor_prod_query, list("region"))
# Returns A tibble: 704 x 3
#   region          year laborproductivity
#   <chr>          <int>             <dbl>
# 1 Africa_Eastern  1975           0.00154
# 2 Africa_Eastern  1990           0.00154
# 3 Africa_Eastern  2005           0.0122 
# 4 Africa_Eastern  2010           0.0393 
# 5 Africa_Eastern  2015           0.0208 
# 6 Africa_Eastern  2020           0.0195 
# 7 Africa_Eastern  2025           0.0287 
# 8 Africa_Eastern  2030           0.0413 
# 9 Africa_Eastern  2035           0.0412 
#10 Africa_Eastern  2040           0.0434 
# ... with 694 more rows

# and arbitrarily scale it
labor_prod %>%
    filter(year == 2020) %>%
    mutate(laborproductivity = laborproductivity*1.2) ->
    change_prod
# set the updated labor productivity values back into the model
# Note: the syntax for setting data is similar to get_data only now the
# + indicates to read the value to match from the current row of the table
# and for that reason, when we are doing set_data you must be explicit on
# where to include the `+` as apposed to get_data which will implicity add
# it to your query_params
set_data(g, change_prod, labor_prod_query, list("region" = c("+", "="), "year" = c("+", "=")))
# double check that the values got set
double_check <- get_data(g, labor_prod_query, list("region", "year" = c("=", 2020)))

# we have only set the parameters at this point, to see how it effects
# results we must re-run period 5
run_to_period(g, 5L)

# Get the CO2 emissions again and see how they have changed
co2_change <- get_data(g, co2_query, query_params)
co2_core %>%
  left_join(co2_change, by=c("region", "ghg", "year")) %>%
  mutate(diff = emissions.x - emissions.y)


# Create a SolutionDebugger object so that we can manually run iterations
# this is useful to debug solution issues or just poke the model to see
# how it reacts by either adjusting prices and looking at the impacts of
# supplies and demand or combining with the set_data/get_data capabilities
# to isolate specific objects
sd <- create_solution_debugger(g, 5)
# get prices, F(x) aka supply - demand, supplies, demand,
# and scale factors.  All of these methods are index by "market name"
# so a user can easily check the price for some specific markets, for instance.
# See the documentation on more information about scaling, and what it means.
x <- get_prices(sd, TRUE)
sort(abs(x), decreasing = TRUE)[1:5]
fx <- get_fx(sd)
sort(abs(fx), decreasing = TRUE)[1:5]
prices <- get_prices(sd, FALSE)
sort(abs(prices), decreasing = TRUE)[1:5]
get_supply(sd, FALSE)[1:5]
get_demand(sd, FALSE)[1:5]
get_price_scale_factor(sd)[1:5]
get_quantity_scale_factor(sd)[1:5]
# Run a single evaluation of the model and see how F(x)
# changes.  Note: users should take care to keep track of
# the the values they have received and what the "internal"
# state of GCAM is.  We provide options to control when we do
# or do not reset the internal state of GCAM after an evaluation
# to help with this
fx2 <- evaluate(sd, x, TRUE, TRUE)
sum(abs(fx) - abs(fx2))
# Change the first price and see how it changes F(x)
# evaluate_partial is a special case of evaluate which may be
# faster in some cases.  See the documentation for evaluate_partial
# for more details
fx3 <- evaluate_partial(sd, x[1]*1.5, 1, T)
abs(fx[1]) - abs(fx3[1])

# Change GCAM calculate the Jacobian matrix, aka partial derivatives,
# which can be used in the newton family of solution algorithms or just
# a static analysis to understand which markets have a first order impact
# on other markets
J <- calc_derivative(sd)
# As mentioned earlier we can index by market names or numeric index
J['USAtraded oilcropDemand_int', 'USAtraded oilcropDemand_int']
J[1:5, 1:5]
```

The following is an example python script:
```python
import os, platform

if platform.system() == "Windows" :
    # Since python 3.8 it will no longer use the PATH environment
    # variable to find DLLs on Windows.
    # The recommended alternative is to use os.add_dll_directory
    # however it means we have to insert system specific info into
    # our .py script.  TODO: find a better solution 
    os.add_dll_directory("C:/Program Files/Java/jdk-15.0.1/bin")
    os.add_dll_directory("C:/Program Files/Java/jdk-15.0.1/bin/server")
    os.add_dll_directory("C:/GCAM/gcam-core/exe")
    # Now that we can find the DLLs, we will be able to load our
    # package


import gcamwrapper

# Create a Gcam instance by giving it a configuration file and the
# appropriate working directory
g = gcamwrapper.Gcam("configuration_ref.xml", "C:/GCAM/gcam-core/exe")
# Run the model up to period 5, the year 2020
g.run_to_period(5)

# Do a simple experiment: see how CO2 emissions change after arbitrarily changing
# the GDP

# First save the CO2 emissions before
# Note we use the [GCAM Fusion](http://jgcri.github.io/gcam-doc/dev-guide/examples.html)
# syntax to query results with a few additional features:
# If we include a `+` in a filter it is interpreted by gcamwrapper to
# mean we want to record the name/year of the object seen at the step
# We add a filter predicate `MatchesAny` which always matches

# To ease the burden on users to write queries, we include a query library which is specified
# in `inst/extdata/query_library.yml` and can be accessed with the `get_query` utility:
co2_query = gcamwrapper.get_query("emissions", "co2_emissions")
# which returns: world/region{region@name}//ghg[NamedFilter,StringEquals,CO2]/emissions{year@year}
# But note the filters on region and emissions year are just place holders.  To simplify this
# aspect as well we allow users to specify query parameters using a short hand notation provided
# as a dict:
query_params = dict(
    "region": # The key is the place holder "tag"
        ["=", "USA"], # The value is an array with the first value an operator and the second the
                       # RHS operand.  Note for get_data the "+" is implied but could be added explicitly
                       # The available operators include:
                       # For strings (@name): "*" (any) "="  or "=~" (regular expression matches)
                       # For ints (@year): "*" (any), "=", "<", "<=", ">", ">="
    "year": ["=", 2020])
# The placeholders will then get transformed into:
# "world/region[+NamedFilter,MatchesAny]//ghg[+NamedFilter,StringEquals,CO2]/emissions[+YearFilter,IntEquals,2020]")
# We can then pass the query and query_params and retrieve the results in a DataFrame.
# Note: if no query_params are given we assume the user is providing GCAM Fusion query
# and no placeholders need to be translated.
co2_core = g.get_data(co2_query, query_params)
# Returns a Pandas.DataFrame: co2_core.head()
#             region  ghg  year   emissions
# 0   Africa_Eastern  CO2  2020   27.749129
# 1  Africa_Northern  CO2  2020  158.604126
# 2  Africa_Southern  CO2  2020   24.488256
# 3   Africa_Western  CO2  2020   53.900300
# 4        Argentina  CO2  2020   54.641103


# get the current GDP labor productivity value, note: laborproductivity is a vector by year
# and since we didn't explicitly filter we will get a value for all years AND the accompanied
# year column
labor_prod_query <- get_query("socioeconomic", "labor_productivity")
# note the shorthand here, when we have just the key and no value it assumes you want +MatchesAny
labor_prod = g.get_data(labor_prod_query, {"region": None})
# Returns a Pandas.DataFrame: labor_prod.head()
#            region  year  laborproductivity
# 0  Africa_Eastern  1975            0.00154
# 1  Africa_Eastern  1990            0.00154
# 2  Africa_Eastern  2005            0.01216
# 3  Africa_Eastern  2010            0.03926
# 4  Africa_Eastern  2015            0.02085

# and arbitrarily scale it
labor_prod_change = labor_prod[labor_prod.year == 2020].copy()
labor_prod_change.loc[:,'laborproductivity'] = labor_prod_change['laborproductivity'] * 1.2
# set the updated labor productivity values back into the model
# Note: the syntax for setting data is similar to get_data only now the
# + indicates to read the value to match from the current row of the table
# and for that reason, when we are doing set_data you must be explicit on
# where to include the `+` as apposed to get_data which will implicity add
# it to your query_params
g.set_data(labor_prod_change, labor_prod_query, {"region": ["+", "="], "year": ["+", "="]})
# double check that the values got set
double_check = g.get_data(labor_prod_query, {"region": None, "year": ["=", 2020]})

# we have only set the parameters at this point, to see how it effects
# results we must re-run period 5
g.run_to_period(5)

# Get the CO2 emissions again and see how they have changed
co2_change = g.get_data(co2_query, query_params)
co2_diff = co2_core.merge(co2_change, on=["region", "ghg", "year"])
co2_diff["diff"] = co2_diff["emissions_x"] - co2_diff["emissions_y"]



# Create a SolutionDebugger object so that we can manually run iterations
# this is useful to debug solution issues or just poke the model to see
# how it reacts by either adjusting prices and looking at the impacts of
# supplies and demand or combining with the set_data/get_data capabilities
# to isolate specific objects
sd = g.create_solution_debugger(5)
# get prices, F(x) aka supply - demand, supplies, demand,
# and scale factors.  All of these methods are index by "market name"
# so a user can easily check the price for some specific markets, for instance.
# See the documentation on more information about scaling, and what it means.
x = sd.get_prices(True)
x.abs().sort_values(ascending=False)[0:5]
fx = sd.get_fx()
fx.abs().sort_values(ascending=False)[0:5]
prices = sd.get_prices(False)
prices.abs().sort_values(ascending=False)[0:5]
sd.get_supply(False)[0:5]
sd.get_demand(False)[0:5]
sd.get_price_scale_factor()[0:5]
sd.get_quantity_scale_factor()[0:5]

# Run a single evaluation of the model and see how F(x)
# changes.  Note: users should take care to keep track of
# the the values they have received and what the "internal"
# state of GCAM is.  We provide options to control when we do
# or do not reset the internal state of GCAM after an evaluation
# to help with this
fx2 = sd.evaluate(x, True, True)
sum(fx.abs() - fx2.abs())
# Change the first price and see how it changes F(x)
# evaluate_partial is a special case of evaluate which may be
# faster in some cases.  See the documentation for evaluate_partial
# for more details
fx3 = sd.evaluate_partial(x[0]*1.5, 0, True)
sum(fx.abs() - fx3.abs())

# Change GCAM calculate the Jacobian matrix, aka partial derivatives,
# which can be used in the newton family of solution algorithms or just
# a static analysis to understand which markets have a first order impact
# on other markets
J = sd.calc_derivative()
# As mentioned earlier we can index by market names or numeric index
J['USAtraded oilcropDemand_int']['USAtraded oilcropDemand_int']
J[0:5][0:5]
```
