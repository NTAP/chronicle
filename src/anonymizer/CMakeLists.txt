add_library(dsanonymizer
            AnonHelper.cc
            FieldAnonymizer.cc
            FileExtAnonymizer.cc
            FpeAnonymizer.cc
            HashAnonymizer.cc
            PathAnonymizer.cc
            RowAnonymizer.cc)

add_executable(anonymizer
               anonymizer.cc)
target_link_libraries(anonymizer
                      dsanonymizer
                      ${DSLIBS})
# man page for anonymizer command
gen_pod_man(anonymizer.cc)

# Anonymization scripts
configure_file(anonymizeNfs3Trace.in anonymizeNfs3Trace @ONLY)
configure_file(anonymizeNfs3TraceWithUid.in anonymizeNfs3TraceWithUid @ONLY)
configure_file(anonymizeSnapshot.in anonymizeSnapshot @ONLY)
configure_file(anonymizerDaemon.py anonymizerDaemon @ONLY)
configure_file(fswalkAnonymizerDaemon.py fswalkAnonymizerDaemon @ONLY)
configure_file(encryptionDaemon.py encryptionDaemon @ONLY)
configure_file(decryptTraces.py decryptTraces @ONLY)

# Unit test executable
#add_executable(libanonymize_unit_tests
#               MyModule1Test.cc)
#target_link_libraries(libanonymize_unit_tests
#                      anonymize
#                      ${GCOV_LIB}
#                      gmock_main
#                      task)
#add_test(libanonymize_unit_tests
#         libanonymize_unit_tests --gtest_shuffle --gtest_output=xml)

