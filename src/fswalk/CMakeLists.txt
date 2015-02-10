add_custom_target(fssnapshot ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/crawl_exports
  VERBATIM
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crawl_exports
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/crawl_exports crawl_exports
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/crawl_exports
  VERBATIM)

#add_executable(fssnapshot_unit_tests
#               DirScanner.cc
#               DirScannerTest.cc)
#target_link_libraries(fssnapshot_unit_tests
#                      ${GCOV_LIB}
#                      gmock_main
#                      task)
#add_test(fssnapshot_unit_tests
#         fssnapshot_unit_tests --gtest_shuffle --gtest_output=xml)

add_executable(fswalk
               fswalk.cc
               DirGatherer.cc
               DirScanner.cc
               ScannerFactory.cc
               StatGatherer.cc)
target_link_libraries(fswalk
                      ${TCMALLOC}
                      task
                      ${DSLIBS}
                      pthread)
gen_pod_man(fswalk.cc)
