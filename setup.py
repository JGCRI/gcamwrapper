import os
import platform
import shutil

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
TBB_INCLUDE = os.environ["TBB_INCLUDE"]
TBB_LIB = os.environ["TBB_LIB"]
EIGEN_INCLUDE = os.environ["EIGEN_INCLUDE"]
if 'HAVE_JAVA' in os.environ and os.environ['HAVE_JAVA'] == '0':
    HAVE_JAVA = False
    have_java_macro = '0'
else:
    HAVE_JAVA = True
    have_java_macro = '1'
    JAVA_INCLUDE = os.environ["JAVA_INCLUDE"]
    JAVA_PLATFORM_INCLUDE = JAVA_INCLUDE + '/' + os.uname()[0].lower()
    JAVA_LIB = os.environ["JAVA_LIB"]

gcam_include_dirs = ['inst/include', GCAM_INCLUDE, BOOST_INCLUDE, TBB_INCLUDE, EIGEN_INCLUDE]
gcam_lib_dirs = [GCAM_LIB, BOOST_LIB, TBB_LIB]
# the exact set of libs to link will be platform specific due to boost
gcam_libs = ['gcam', 'hector', 'tbb', 'tbbmalloc', 'tbbmalloc_proxy']
if HAVE_JAVA:
    gcam_include_dirs.extend([JAVA_INCLUDE, JAVA_PLATFORM_INCLUDE])
    gcam_libs.append(JAVA_LIB) 
    gcam_libs.append('jvm')
gcam_compile_args = []
gcam_link_args = []
if platform.system() == "Windows" :
    # boost "auto links" on Windows with compiler specific names
    # so do not include any of them
    gcam_compile_args = []
else :
    # boost appends the python version to the boost python library name
    py_version_suffix = ''.join(platform.python_version_tuple()[0:2])
    gcam_libs += [lib + py_version_suffix for lib in ['boost_python', 'boost_numpy']]
    # ensure we use the correct c++ std
    gcam_compile_args += ['-std=c++17']
    # add rpath info to find the dynamic linked libs
    gcam_link_args += ['-Wl,-rpath,'+BOOST_LIB, '-Wl,-rpath,'+TBB_LIB]
    if HAVE_JAVA:
        gcam_link_args.append('-Wl,-rpath,'+JAVA_LIB)

gcam_module = Extension(
    'gcam_module',
    sources = ['src/gcam.cpp',
        'src/solution_debugger.cpp',
        'src/query_processor_base.cpp',
        'src/set_data_helper.cpp',
        'src/get_data_helper.cpp'],
    include_dirs=gcam_include_dirs,
    library_dirs=gcam_lib_dirs,
    libraries=gcam_libs,
    define_macros=[('NDEBUG', '1'),
                   ('BOOST_DATE_TIME_NO_LIB', '1'),
                   ('FUSION_MAX_VECTOR_SIZE', '30'),
                   ('BOOST_MATH_TR1_NO_LIB', '1'),
                   ('BOOST_PYTHON_STATIC_LIB', '1'),
                   ('BOOST_NUMPY_STATIC_LIB', '1'),
                   ('__HAVE_JAVA__', have_java_macro)],
    extra_link_args=gcam_link_args,
    language='c++',
    extra_compile_args = gcam_compile_args
    )


try:
    # workaround to deal with R and Python being PITA about package data
    shutil.copy(os.path.join('inst', 'extdata', 'query_library.yml'), 'gcamwrapper')
    setup(
        name='gcamwrapper',
        version='0.1.0',
        packages=find_packages(),
        ext_modules=[gcam_module],
        install_requires=get_requirements(),
        #include_package_data=True,
        package_data={'gcamwrapper': ['query_library.yml']},
        url='https://github.com/JGCRI/gcamwrapper',
        license='ECL 2',
        author='Pralit Patel, Chris R. Vernon',
        author_email='pralit.patel@pnnl.gov, chris.vernon@pnnl.gov',
        description='Python API for GCAM',
        long_description=readme(),
        long_description_content_type='text/markdown',
        python_requires='>=3.6'
    )
finally:
    os.unlink(os.path.join('gcamwrapper', 'query_library.yml'))
     
