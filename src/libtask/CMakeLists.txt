# Main "libtask" library
add_library(task
            FDWatcher.cc
            Message.cc
            Process.cc
            Scheduler.cc)
if (libtask_topology)
  target_link_libraries(task
                        ${LINTEL}
                        pthread
                        topology)
else()
  target_link_libraries(task
                        ${LINTEL}
                        pthread)
endif()

if (gperftools)
  target_link_libraries(task
                        ${GCPUPROFILER})
endif()

# Library for the message ring application
add_library(msgring
            MsgRingApp.cc
            MsgRingWorker.cc)

# Stand-alone message ring benchmark
add_executable(bench_msgring
               msgring.cc)
target_link_libraries(bench_msgring
                      msgring
                      task
                      ${TCMALLOC})

add_library(a2a
            A2AMaster.cc
            A2AWorker.cc)
add_executable(bench_a2a
               bench_a2a.cc)
target_link_libraries(bench_a2a
                      a2a
                      task
                      ${TCMALLOC})
               

add_library(socketpair
            SocketPair.cc)
add_executable(bench_socketpair
               socketpair.cc)
target_link_libraries(bench_socketpair
                      socketpair
                      task
                      ${TCMALLOC})

add_executable(bench_aio
               bench_aio.cc)
target_link_libraries(bench_aio
                      aio)

add_executable(bench_fifolist
               bench_fifolist.cc)
target_link_libraries(bench_fifolist
                      ${TCMALLOC}
                      pthread)

add_executable(bench_fifoswap
               bench_fifoswap.cc)
target_link_libraries(bench_fifoswap
                      ${TCMALLOC}
                      pthread)

add_executable(bench_spinlock
               bench_spinlock.cc)
target_link_libraries(bench_spinlock
                      ${TCMALLOC}
                      pthread)

add_executable(fdmonopolize
               fdmonopolize.cc
               Monopolize.cc
               ConsoleWatcher.cc)
target_link_libraries(fdmonopolize
                      ${TCMALLOC}
                      task)
              
# Unit test executable
add_executable(libtask_unit_tests
               FDWatcherTest.cc
               FifoListTest.cc
               MsgRingTest.cc
               MutexTest.cc
               ProcessTest.cc
               SchedulerTest.cc
               SpinLockTest.cc
               TSQueueTest.cc)
target_link_libraries(libtask_unit_tests
                      ${GCOV_LIB}
                      gmock_main
                      msgring
                      task)
add_test(libtask_unit_tests
         libtask_unit_tests --gtest_shuffle --gtest_output=xml)

