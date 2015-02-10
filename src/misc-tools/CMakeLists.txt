add_executable(dumpextents
               dumpextents.cc
               ${PROJECT_SOURCE_DIR}/chronicle/ChronicleExtents.cc)

add_executable(dumppcap
               dumppcap.cc
               PcapFile.cc)

add_executable(pcapNsToUs
               pcapNsToUs.cc
               PcapFile.cc)

add_executable(pcapDuplicate
               pcapDuplicate.cc
               PcapFile.cc)
