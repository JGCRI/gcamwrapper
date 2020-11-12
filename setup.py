import os, sys

from setuptools import setup, find_packages, Extension

GCAM_INCLUDE = os.environ["GCAM_INCLUDE"]
GCAM_LIB = os.environ["GCAM_LIB"]
BOOST_INCLUDE = os.environ["BOOST_INCLUDE"]
BOOST_LIB = os.environ["BOOST_LIB"]
XERCES_INCLUDE = os.environ["XERCES_INCLUDE"]
XERCES_LIB = os.environ["XERCES_LIB"]
JAVA_INCLUDE = os.environ["JAVA_INCLUDE"]
JAVA_LIB = os.environ["JAVA_LIB"]

gcam_module = Extension(
    'gcam_module',
    sources = ['src/gcam.cpp', 'src/solution_debugger.cpp', 'src/set_data_helper.cpp', 'src/get_data_helper.cpp'],
    include_dirs=['inst/include', GCAM_INCLUDE, BOOST_INCLUDE, XERCES_INCLUDE, JAVA_INCLUDE],
    library_dirs=[GCAM_LIB, BOOST_LIB, XERCES_LIB, JAVA_LIB],
    libraries=['gcam', 'hector', 'boost_python36', 'boost_numpy36', 'boost_system', 'boost_filesystem', 'jvm', 'xerces-c'],
    define_macros=[('NDEBUG', '1')],
    extra_link_args=['-Wl,-rpath,'+XERCES_LIB, '-Wl,-rpath,'+JAVA_LIB, '-Wl,-rpath,'+BOOST_LIB],
    language='c++',
    extra_compile_args = ['-std=c++14', '-stdlib=libc++', '-mmacosx-version-min=10.9'],
    )


setup(
    name='gcamdebugr',
    version='0.1.0',
    packages=find_packages(),
    ext_modules=[gcam_module],
    install_requires=["pandas"],
    url='https://stash.pnnl.gov/scm/jgcri/gcamdebugr.git',
    license='BSD 2-Clause',
    author='Pralit Patel, Chris R. Vernon',
    author_email='pralit.patel@pnnl.gov, chris.vernon@pnnl.gov',
    description='Python API for GCAM',
    python_requires='>=3.6.*, <4'
)
 
