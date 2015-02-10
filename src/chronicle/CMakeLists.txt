
find_library(PCAP pcap doc "Path to libpcap library")

find_package(Boost COMPONENTS system REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

include_directories("${EXTRA}/netmap/sys/net")

# Library for reading and processing packets from pcap/netmap interfaces
add_library(chronicle
			AnalyticsModule.cc
			ChecksumModule.cc
			Chronicle.cc
			ChronicleConfig.cc
			ChronicleExtents.cc
			ChroniclePipeline.cc
			ChronicleProcessRequest.cc
			CommandChannel.cc
			DsWriter.cc
                        DsExtentAccess.cc
                        DsExtentCommit.cc
                        DsExtentCreate.cc
                        DsExtentFsinfo.cc
                        DsExtentFsstat.cc
                        DsExtentGetattr.cc
                        DsExtentIp.cc
                        DsExtentLink.cc
                        DsExtentLookup.cc
                        DsExtentPathconf.cc
                        DsExtentRead.cc
                        DsExtentReaddir.cc
                        DsExtentReadlink.cc
                        DsExtentRemove.cc
                        DsExtentRename.cc
                        DsExtentRpc.cc
                        DsExtentSetattr.cc
                        DsExtentStats.cc
			FlowDescriptor.cc
			FlowTable.cc
			${EXTRA}/misc/MurmurHash3.cpp
			NetmapInterface.cc
			NetworkHeaderParser.cc
			NfsParser.cc
			OutputModule.cc
			PacketBufferPool.cc			
			PacketReader.cc
			PcapInterface.cc
			PcapPacketBufferPool.cc
			PcapWriter.cc
			PcapPduWriter.cc
			RpcParser.cc
			StatGatherer.cc
			TcpStreamNavigator.cc)
target_link_libraries(chronicle
					  task
					  ${DSLIBS}
					  ${PCAP}
					  ${TCMALLOC}
					  ${Boost_LIBRARIES}
					  pthread
					  crypto)

# Stand-alone pcap reader application
add_executable(chronicle_pcap
			   chroniclePcapApp.cc)
target_link_libraries(chronicle_pcap
				  	  chronicle)

# Stand-alone netmap reader application
add_executable(chronicle_netmap
			   chronicleNetmapApp.cc)
target_link_libraries(chronicle_netmap
					  chronicle)

# Chronicle unit tests
add_executable(chronicle_unit_tests
			   FlowDescriptorTest.cc
			   FlowTableTest.cc
			   PcapBufferPoolTest.cc
			   MurmurHashTest.cc
			   TcpStreamNavigatorTest.cc)
target_link_libraries(chronicle_unit_tests
					  chronicle
					  ${GCOV_LIB}
                      gmock_main)
add_test(chronicle_unit_tests
         chronicle_unit_tests --gtest_shuffle --gtest_output=xml)

# Chronicle daemon
configure_file(chronicleDaemon.py chronicleDaemon @ONLY)

