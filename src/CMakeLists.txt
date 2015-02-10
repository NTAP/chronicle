cmake_minimum_required(VERSION 2.8)

project(Chronicle)
set(CHRONICLE_VERSION "1.0")

set(EXTRA "${PROJECT_SOURCE_DIR}/../extra")

if(DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING
    "The type of build: Debug, Coverage, Optimized")
else()
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
    "The type of build: Debug, Coverage, Optimized")
endif()

if("Debug" STREQUAL ${CMAKE_BUILD_TYPE})
  # disable tcmalloc in debug builds so we can use valgrind
  set(TCMALLOC "")
  #find_library(TCMALLOC tcmalloc_debug
  #             doc "Path to tcmalloc library")
else()
  find_library(TCMALLOC tcmalloc
               doc "Path to tcmalloc library")
  if(TCMALLOC-NOTFOUND STREQUAL ${TCMALLOC})
    message("Couldn't find tcmalloc, using default malloc")
    set(TCMALLOC "")
  else()
    message("TCMALLOC enabled")
  endif()
endif()

#-- Enable unit testing
enable_testing()
#-- Change the default to be to build gtest/mock's internal tests
option(gtest_build_tests "Build all of gtest's own tests." ON)
option(gmock_build_tests "Build all of gmock's own tests." ON)
add_subdirectory(../extra "${CMAKE_CURRENT_BINARY_DIR}/extra")
	
option(libtask_topology "Use CPU topology information for schedulers" OFF)
if (libtask_topology)
  message("Enabling libtopology for schedulers")
  set(CONFIG_DEFINES "${CONFIG_DEFINES} -DUSE_TOPOLOGY")
else()
  message("Disabling libtopology for schedulers")
endif()

#-- google cpu profiler
option(gperftools "Include Google cpu profiler" OFF)
if (gperftools)
  find_library(GCPUPROFILER profiler)
  if (GCPUPROFILER_FOUND)
    message("Enabling cpuprofiler")
    set(CONFIG_DEFINES "${CONFIG_DEFINES} -DUSE_GCPUPROF")
  endif(GCPUPROFILER_FOUND)
else()
  message("Disabling cpuprofiler")
endif()
#find_library(GCPUPROFILER profiler)

#option(libtask_fifolist "Use lock-free FIFOs instead of C++ deque." OFF)
#if (libtask_fifolist)
#  message("Enabling lock-free FIFOs in libtask")
#  set(CONFIG_DEFINES "${CONFIG_DEFINES} -DUSE_FIFO")
#else()
#  message("Disabling lock-free FIFOs in libtask")
#endif()

#-- The "Coverage" build type allows generating code coverage
#-- metrics. We only enable the CodeCoverage target if we built with that
#-- configuration.
if("Coverage" STREQUAL ${CMAKE_BUILD_TYPE})
  set(GCOV_LIB "gcov")
  #-- Calculate code coverage metrics
  #-- make CodeCoverage
  add_custom_command(
    OUTPUT code_coverage.xml
    COMMAND ${EXTRA}/misc/gcovr -r ${CMAKE_CURRENT_SOURCE_DIR} -e gmock -e .*Test.cc -e .*Test.c -x -o code_coverage.xml
    VERBATIM
    )
  add_custom_command(
    OUTPUT code_coverage.txt
    COMMAND ${EXTRA}/misc/gcovr -r ${CMAKE_CURRENT_SOURCE_DIR} -e gmock -e .*Test.cc -e .*Test.c -o code_coverage.txt
    VERBATIM
    )
  add_custom_target(CodeCoverage
    DEPENDS code_coverage.txt
    DEPENDS code_coverage.xml
    )
else()
  set(GCOV_LIB "")
endif()

#-- Enable Doxygen if it exists
#-- Adds a 'make doc' target
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    @ONLY
  )
  add_custom_target(doc
    COMMENT "Generating API documentation with Doxygen"
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_logo.gif
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_header-bg.gif
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
  )
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_header-bg.gif
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/doc/html
    COMMAND cp ${EXTRA}/misc/doxygen/netapp_header-bg.gif ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_header-bg.gif
    COMMAND chmod 644 ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_header-bg.gif
    VERBATIM
  )
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_logo.gif
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/doc/html
    COMMAND cp ${EXTRA}/misc/doxygen/netapp_logo.gif ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_logo.gif
    COMMAND chmod 644 ${CMAKE_CURRENT_BINARY_DIR}/doc/html/netapp_logo.gif
    VERBATIM
  )
endif()

#-- Commands for generating man pages from embedded pod documentation
macro(gen_pod_man source_file)
  string(REGEX REPLACE "\\.[^\\.]*$" "" basename ${source_file})
  set(man_file ${basename}.1)
  set(source_file_full ${CMAKE_CURRENT_SOURCE_DIR}/${source_file})
  add_custom_command(
    OUTPUT ${man_file}
    DEPENDS ${source_file_full}
    COMMAND pod2man -c "Chronicle Documentation" -n ${basename} -r "Chronicle ${CHRONICLE_VERSION}" -s 1 ${source_file_full} ${man_file}
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/doc
    COMMAND cp ${man_file} ${CMAKE_BINARY_DIR}/doc
    VERBATIM)
  add_custom_target(man_${man_file}
    DEPENDS ${man_file})
  add_dependencies(doc man_${man_file})
endmacro()

#-- Generate sloccount output
#-- make sloccount
find_program(SLOCCOUNT sloccount
  DOC "Path to sloccount executable"
)
if(SLOCCOUNT)
  add_custom_target(sloccount
    DEPENDS sloccount.sc
  )
  add_custom_command(
    OUTPUT sloccount.sc
    COMMAND ${SLOCCOUNT} --wide --details ${CMAKE_CURRENT_SOURCE_DIR} > sloccount.sc
    VERBATIM
  )
endif()

#-- Compiler flags
set(CMAKE_C_FLAGS "-Wall -std=c++11 ${CONFIG_DEFINES}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-Werror -g3 -O -DDEBUG -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_COVERAGE
  "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_C_FLAGS_COVERAGE}")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "-fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS_OPTIMIZED "-g -O3 -march=native")
set(CMAKE_CXX_FLAGS_OPTIMIZED "${CMAKE_C_FLAGS_OPTIMIZED}")

#-- Bring in Lintel & DataSeries
find_library(LINTEL
             NAMES "Lintel" "libLintel.so.0"
             REQUIRED)
message("LINTEL: ${LINTEL}")
find_library(DATASERIES
             NAMES DataSeries libDataSeries.so.0
             REQUIRED)
message("DS: ${DATASERIES}")
find_library(LINTELPTH
             NAMES LintelPThread libLintelPThread.so.0
             REQUIRED)
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
set(DSLIBS ${DATASERIES} ${LINTEL} ${LINTELPTH} ${LIBXML2_LIBRARIES})

include_directories(libtask)
if (libtask_topology)
  include_directories(libtopology)
endif()
include_directories(chronicle)
include_directories("${EXTRA}/gmock/gtest/include")
include_directories("${EXTRA}/gmock/include")
include_directories("${EXTRA}/misc")

add_subdirectory(anonymizer)
add_subdirectory(chronicle)
add_subdirectory(fswalk)
add_subdirectory(libtask)
add_subdirectory(misc-tools)
add_subdirectory(DataSeries-tools)
if (libtask_topology)
  add_subdirectory(libtopology)
endif()

