
find_library(PCAP pcap doc "Path to libpcap library")

find_package(Boost COMPONENTS system filesystem REQUIRED)

SET(CHRONICLE_SRC "${PROJECT_SOURCE_DIR}/chronicle/")

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

add_executable(dsconvert_v2_to_v1 dsconvert_v2_to_v1.cc ChronicleExtentsV1.cc)
target_link_libraries(dsconvert_v2_to_v1 ${DSLIBS} ${Boost_LIBRARIES} pthread)

add_executable(dsconvert_v1_to_v2 dsconvert_v1_to_v2.cc ChronicleExtentsV2.cc ${EXTRA}/misc/MurmurHash3.cpp)
target_link_libraries(dsconvert_v1_to_v2 ${DSLIBS} ${Boost_LIBRARIES} pthread)

add_executable(dsmerge dsmerge.cc)
target_link_libraries(dsmerge ${DSLIBS} ${Boost_LIBRARIES} pthread)
configure_file(dsmergeWrapper.py dsmergeWrapper @ONLY)

