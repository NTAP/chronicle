# Main "libtopology" library
add_library(topology
            Cache.cc
            LogicalProcessor.cc
            TopologyMap.cc
            )

add_executable(sampleTopologyMapApp
               sampleTopologyMapApp.cc)

target_link_libraries(sampleTopologyMapApp
                      topology)

add_executable(libtopology_unit_tests
               CacheTest.cc
               LogicalProcessorTest.cc
               TopologyMapTest.cc)

target_link_libraries(libtopology_unit_tests
                      ${GCOV_LIB}
                      gmock_main
                      topology)
add_test(libtopology_unit_tests
         libtopology_unit_tests --gtest_shuffle --gtest_output=xml)
