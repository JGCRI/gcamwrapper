import os, platform

from setuptools import setup, find_packages, Extension

def readme():
    with open('README.md') as f:
        return f.read()


def get_requirements():
    with open('requirements.txt') as f:
        return f.read().split()

GCAM_INCLUDE = os.environ["GCAM_INCLUDE"]
GCAM_LIB = os.environ["GCAM_LIB"]
BOOST_INCLUDE = os.environ["BOOST_INCLUDE"]
BOOST_LIB = os.environ["BOOST_LIB"]
XERCES_INCLUDE = os.environ["XERCES_INCLUDE"]
XERCES_LIB = os.environ["XERCES_LIB"]
JAVA_INCLUDE = os.environ["JAVA_INCLUDE"]
JAVA_LIB = os.environ["JAVA_LIB"]

# the exact set of libs to link will be platform specific due to boost
gcam_libs = ['gcam', 'hector', 'jvm', 'xerces-c']
gcam_compile_args = []
gcam_link_args = []
if platform.system() == "Windows" :
    # boost "auto links" on Windows with compiler specific names
    # so do not include any of them
    gcam_compile_args = []
else :
    gcam_libs += ['boost_system', 'boost_filesystem']
    # boost appends the python version to the boost python library name
    py_version_suffix = ''.join(platform.python_version_tuple()[0:2])
    gcam_libs += [lib + py_version_suffix for lib in ['boost_python', 'boost_numpy']]
    # ensure we use the correct c++ std
    gcam_compile_args += ['-std=c++14']
    # add rpath info to find the dynamic linked libs
    gcam_link_args += ['-Wl,-rpath,'+XERCES_LIB, '-Wl,-rpath,'+JAVA_LIB, '-Wl,-rpath,'+BOOST_LIB]

gcam_module = Extension(
    'gcam_module',
    sources = ['src/gcam.cpp', 'src/solution_debugger.cpp', 'src/set_data_helper.cpp', 'src/get_data_helper.cpp'],
    include_dirs=['inst/include', GCAM_INCLUDE, BOOST_INCLUDE, XERCES_INCLUDE, JAVA_INCLUDE],
    library_dirs=[GCAM_LIB, BOOST_LIB, XERCES_LIB, JAVA_LIB],
    libraries=gcam_libs,
    define_macros=[('NDEBUG', '1'),
                   ('BOOST_DATE_TIME_NO_LIB', '1'),
                   ('FUSION_MAX_VECTOR_SIZE', '30'),
                   ('BOOST_MATH_TR1_NO_LIB', '1'),
                   ('BOOST_PYTHON_STATIC_LIB', '1'),
                   ('BOOST_NUMPY_STATIC_LIB', '1')],
    extra_link_args=gcam_link_args,
    language='c++',
    extra_compile_args = gcam_compile_args
    )


setup(
    name='gcamwrapper',
    version='0.1.0',
    packages=find_packages(),
    ext_modules=[gcam_module],
    install_requires=get_requirements(),
    url='https://stash.pnnl.gov/scm/jgcri/gcamwrapper.git',
    license='ECL 2',
    author='Pralit Patel, Chris R. Vernon',
    author_email='pralit.patel@pnnl.gov, chris.vernon@pnnl.gov',
    description='Python API for GCAM',
    long_description=readme(),
    python_requires='>=3.6.*, <4'
)
 
