// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "Scheduler.h"
#include "Process.h"
#include "FDWatcher.h"
#ifdef USE_TOPOLOGY
#include "TopologyMap.h"
#endif // USE_TOPOLOGY
#ifdef USE_GCPUPROF
#include <gperftools/profiler.h>
#endif // USE_GCPUPROF
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h> // for cpu profiler
#include <sys/eventfd.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <list>
#include <map>

pthread_key_t Scheduler::_currSchedKey;
pthread_key_t Scheduler::_isSchedKey;
pthread_once_t Scheduler::_currSchedKeyOnce = PTHREAD_ONCE_INIT;

static const unsigned MSGS_TO_RUN = 100;

// The scheduler responsible for handling FDWatcher
static Scheduler *fdScheduler = 0;

typedef std::list<unsigned> CpuList;
typedef std::vector<unsigned> CpuVector;
typedef std::list<CpuList> ListCpuList;
typedef std::map<unsigned, Scheduler *> SchedMap;

#ifdef USE_GCPUPROF
void
profilerSignalHandler(int signum)
{
    static const char *filename = "profile.prof";
    switch (signum) {
    case SIGUSR1: // start
        ProfilerStart(filename);
        break;
    case SIGUSR2: // stop
        ProfilerStop();
        break;
    }
}
#endif // USE_GCPUPROF

#ifdef USE_TOPOLOGY
// Create a list of all processors on each socket.
static ListCpuList
getProcsBySocket()
{
    TopologyMap tm;
    unsigned processors = tm.init();
    
    std::map<unsigned, CpuList > socketToProc;
    for (unsigned p = 0; p < processors; ++p) {
        LogicalProcessor *lp = tm.getProcessor(p);
        socketToProc[lp->getSocketID()].push_back(p);
    }

    ListCpuList pBySList;
    while (!socketToProc.empty()) {
        pBySList.push_back(socketToProc.begin()->second);
        socketToProc.erase(socketToProc.begin());
    }

    return pBySList;
}

// sort the list of processors such that we separate the hyperthreads
// from each other
static CpuList
sortProcsByThread(CpuList procs)
{
    TopologyMap tm;
    tm.init();
    
    CpuList primaryList, secondaryList;
    while (!procs.empty()) {
        unsigned primary = procs.front();
        procs.pop_front();
        primaryList.push_back(primary);
        
        std::list<int> ht;
        tm.sameCore(primary, ht);
        assert(ht.size() < 2); // only works w/ 0 or 1 hyperthreads
        if (!ht.empty()) {
            unsigned secondary = ht.front();
            secondaryList.push_back(secondary);
            procs.remove(secondary);
        }
    }
    primaryList.splice(primaryList.end(), secondaryList);
    return primaryList;
}


// build the steal list from the sorted arrays of processors
static CpuList
makeStealList(unsigned proc, ListCpuList procList)
{
    // find the processor we want to build the list for
    ListCpuList::iterator s;
    CpuList::iterator p;
    for (s = procList.begin(); s != procList.end(); ++s) {
        for (p = s->begin(); p != s->end(); ++p) {
            if (*p == proc) break;
        }
        if (p != s->end()) break;
    }
    assert(s != procList.end() && p != s->end()); // make sure we found it

    // see if this one is the last on the socket, meaning we should
    // steal from the other socket first
    CpuList::iterator next = p;
    ++next;
    bool stealOtherSocket = (next == s->end());

    //rotate the lists
    if (s != procList.begin()) {
        procList.splice(procList.begin(), procList, s, procList.end());
    }
    while (p != s->begin()) {
        for (ListCpuList::iterator i = procList.begin();
             i != procList.end(); ++i) {
            CpuList::iterator second = i->begin();
            ++second;
            i->splice(i->end(), *i, i->begin(), second);
        }
    }

    CpuList stealList;
    if (stealOtherSocket) {
        ListCpuList::iterator i = procList.begin();
        stealList.push_back(i->front());
        i->pop_front();
        ++i;
        if (i != procList.end()) {
            CpuList::iterator j = i->begin();
            stealList.push_back(*(++j));
            i->erase(j);
        }
    }

    for (ListCpuList::iterator i = procList.begin();
         i != procList.end(); ++i) {
        stealList.splice(stealList.end(), *i, i->begin(), i->end());
    }

    return stealList;
}

static CpuList
procsToUse(unsigned numProcessors, ListCpuList plist)
{
    CpuList procsToUse = makeStealList(0, plist);
    while (procsToUse.size() > numProcessors) {
        procsToUse.pop_back();
    }
    return procsToUse;
}

static CpuList
constrainTo(CpuList original, CpuList constraint)
{
    CpuList::iterator o = original.begin();
    while (o != original.end()) {
        bool inConstraintList = false;
        for(CpuList::iterator c = constraint.begin();
            c != constraint.end(); ++c) {
            if (*o == *c) {
                inConstraintList = true;
                break;
            }
        }
        if (!inConstraintList) {
            CpuList::iterator toErase = o;
            ++o;
            original.erase(toErase);
        } else {
            ++o;
        }
    }

    return original;
}
#endif // USE_TOPOLOGY

bool
Scheduler::initScheduler()
{
    pthread_once(&_currSchedKeyOnce, allocSchedKey);
    return true;
}

unsigned
Scheduler::numProcessors()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

Scheduler *
Scheduler::getCurrentScheduler()
{
    Scheduler *s =
        static_cast<Scheduler *>(pthread_getspecific(_currSchedKey));
    return s;
}

void
Scheduler::setCurrentScheduler(Scheduler *s)
{
    pthread_setspecific(_currSchedKey, s);
}

bool
Scheduler::isInSchedulerContext()
{
    return pthread_getspecific(_isSchedKey);
}

void
Scheduler::setInSchedulerContext(bool isInScheduler)
{
    pthread_setspecific(_isSchedKey, reinterpret_cast<void *>(isInScheduler));
}

void
Scheduler::allocSchedKey()
{
    pthread_key_create(&_currSchedKey, NULL);
    pthread_setspecific(_currSchedKey, NULL);
    pthread_key_create(&_isSchedKey, NULL);
    pthread_setspecific(_isSchedKey, NULL);
}

Scheduler::Scheduler() :
    _run_queue_lock("SchedRunQ"), _currentProcess(NULL),
    _local(0), _steals(0), _msgs(0)
{
    _magicNumber = SCHEDULER_MAGIC;
    _neighbor_fd = eventfd(0, 0);
}

Scheduler::~Scheduler()
{
    close(_neighbor_fd);
    std::cout << "Scheduler stats - processes: " << _local + _steals
              << " (" << std::setprecision(3)
              << (100.0*_local)/(_local+_steals) << "% local)"
              << " \tmessages: " << _msgs
              << " (" << 1.0*_msgs/(_local + _steals) << " per proc)"
              << std::endl;
}

void
Scheduler::setStealList(const SchedList &list)
{
    _stealList = list;
}

Process *
Scheduler::getNextProcess()
{
    Process *p = NULL;

    _run_queue_lock.lock();
    if (!_run_queue.empty()) {
        p = _run_queue.front();
        _run_queue.pop_front();
    }
    _run_queue_lock.unlock();

    return p;
}

Process *
Scheduler::getNextGlobalProcess()
{
    Process *p = NULL;
    Scheduler *n = _stealList.front(); // my neighbor
    bool wasSleeping = false;

    while (!p && Process::numProcesses()) {
        if (this == fdScheduler) { // poll FDWatcher
            FDWatcher *fdw = FDWatcher::getFDWatcher();
            fdw->waitFds(n->_neighbor_fd, false);
        }

        // check locally
        if (NULL != (p = getNextProcess())) {
            ++_local;
            break;
        }
        // check globally
        for (SchedList::iterator i = _stealList.begin();
             i != _stealList.end(); ++i) {
            if (NULL != (p = (*i)->getNextProcess())) {
                //std::cout << this << " stole from " << *i << std::endl;
                break;
            }
        }
        if (p) { // steal was successful
            ++_steals;
            break;
        }

        // wait for our neighbor to get some work so we can steal it
        assert("My run-queue wasn't empty!" && 0 == getNextProcess());
        uint64_t efd_val;
        wasSleeping = true;
        if (this == fdScheduler) {
            FDWatcher *fdw = FDWatcher::getFDWatcher();
            fdw->waitFds(n->_neighbor_fd, true);
        } else {
            read(n->_neighbor_fd, &efd_val, sizeof(efd_val));
        }
    }

    if (wasSleeping) {
        // I was sleeping, but found work, so I should wake my neighbor, too
        uint64_t efd_val = 1;
        write(_neighbor_fd, &efd_val, sizeof(efd_val));
    }

    return p;
}

void
Scheduler::enqueueProcess(Process *p)
{
    p->assertValid();

    bool needToWakeSelf = !isInSchedulerContext();

    _run_queue_lock.lock();
    bool isFirstEntry = _run_queue.empty();
    _run_queue.push_back(p);
    _run_queue_lock.unlock();

    // wake neighbor if necessary
    if (isFirstEntry) {
        uint64_t efd_val = 1;
        write(_neighbor_fd, &efd_val, sizeof(efd_val));
    }

    if (needToWakeSelf) {
        //std::cout << this << " I need to wake myself!" << std::endl;
        if (!_stealList.empty()) {
            Scheduler *n = _stealList.front();
            uint64_t efd_val = 1;
            write(n->_neighbor_fd, &efd_val, sizeof(efd_val));
        }
    }
}

void
Scheduler::run()
{
    setCurrentScheduler(this);
    setInSchedulerContext(true);
    while (0 < Process::numProcesses()) {
        _currentProcess = getNextGlobalProcess();
        if (_currentProcess) {
            _currentProcess->assertValid();
            _currentProcess->setOwner(this);
            _currentProcess->logMessageCount();
			//unsigned numToRun = MIN_MSGS_TO_RUN +
            //    (_currentProcess->pendingMessages() >> 1);
            unsigned numToRun = MSGS_TO_RUN;
            while(numToRun-- && _currentProcess->runOneMessage()) { ++_msgs; }
            if (_currentProcess->hasExited()) {
                delete _currentProcess;
                _currentProcess = NULL;
                if (0 == Process::numProcesses()) {
                    //std::cout << "DONE" << std::endl;
                    // nobody left release everyone
                    uint64_t efd_val = 1;
                    for (SchedList::iterator i = _stealList.begin();
                         i != _stealList.end(); ++i) {
                        write((*i)->_neighbor_fd, &efd_val, sizeof(efd_val));
                    }
                }
            } else {
                _currentProcess->requeueProcess();
                _currentProcess = NULL;
            }
        }
    }
}

/*
static void
printCpuMask(cpu_set_t cpuset)
{
    for (int i = 0; i < 16; ++i) {
        std::cout << CPU_ISSET(i, &cpuset);
    }
    std::cout << std::endl;
}
*/

void *
Scheduler::schedThreadStart(void *arg)
{
    SchedulerInfo *i = static_cast<SchedulerInfo *>(arg);
    if (i->bind) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i->whichCpu, &cpuset);
        if (int rc = pthread_setaffinity_np(pthread_self(),
                                            sizeof(cpu_set_t), &cpuset)) {
            std::cout << strerror(rc) << std::endl;
        }
    }
    Scheduler *s = i->self;
    s->run();
    delete i;
    return NULL;
}

template<class T> static std::vector<T>
getRotatedList(unsigned start, const std::vector<T> &l)
{
    std::vector<T> out;
    for (unsigned i = start; i < l.size(); ++i)
        out.push_back(l.at(i));
    for (unsigned i = 0; i < start; ++i)
        out.push_back(l.at(i));
    return out;
}

void
Scheduler::startSchedulers(unsigned num, Process *initial, bool bindCpu)
{
    std::vector<Scheduler *> schedulers;
    std::list<pthread_t> threads;

    for (unsigned i=0; i < num; ++i) {
        schedulers.push_back(new Scheduler());
    }

#ifdef USE_TOPOLOGY
    // Create CPU list structure for building steal lists
    ListCpuList plist = getProcsBySocket();
    for (ListCpuList::iterator i = plist.begin(); i != plist.end(); ++i) {
        *i = sortProcsByThread(*i);
    }

    // Cut the list of virtual processors down to "num"
    CpuList pToUse = procsToUse(num, plist);
    // pVector is a mapping of scheduler# to cpu#
    CpuVector pVector(pToUse.begin(), pToUse.end());
    ListCpuList::iterator pli = plist.begin();
    while (pli != plist.end()) {
        *pli = constrainTo(*pli, pToUse);
        if (pli->empty()) {
            ListCpuList::iterator toErase = pli;
            ++pli;
            plist.erase(toErase);
        } else {
            ++pli;
        }
    }

    // Create the cpu# -> scheduler* mapping
    SchedMap pToS;
    for (unsigned i = 0; i < num; ++i) {
        pToS[pVector[i]] = schedulers[i];
    }

    /*
    // Print the steal lists
    for (unsigned i = 0; i < num; ++i) {
        CpuList slist = makeStealList(pVector[i], plist);

        for (CpuList::iterator j = slist.begin();
             j != slist.end(); ++j) {
            std::cout << *j << " ";
        }
        std::cout << std::endl;
    }
    */
#else
    
#endif // USE_TOPOLOGY

    for (unsigned i=0; i < num; ++i) {
        SchedulerInfo *inf = new SchedulerInfo;
        inf->bind = bindCpu;
        inf->self = schedulers[i];
#ifdef USE_TOPOLOGY
        inf->whichCpu = pVector[i];
        // Create the SchedList for the scheduler & call setStealList()
        CpuList slist = makeStealList(pVector[i], plist);
        SchedList stealList;
        for (CpuList::iterator sli = slist.begin();
             sli != slist.end(); ++sli) {
            stealList.push_back(pToS[*sli]);
        }
        stealList.push_back(stealList.front());
        stealList.pop_front();
#else
        inf->whichCpu = i % numProcessors();
        std::vector<Scheduler *> rotatedSchedulers =
            getRotatedList(i, schedulers);
        SchedList stealList(rotatedSchedulers.begin(), rotatedSchedulers.end());
        stealList.push_back(stealList.front());
        stealList.pop_front();
#endif // USE_TOPOLOGY
        schedulers[i]->setStealList(stealList);
        pthread_t pth;
        pthread_create(&pth, NULL, schedThreadStart, inf);
        threads.push_back(pth);
    }
    fdScheduler = schedulers[0];
    schedulers[0]->enqueueProcess(initial);

#ifdef USE_GCPUPROF
    struct sigaction sa;
    sa.sa_handler = profilerSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, 0);
    sigaction(SIGUSR2, &sa, 0);
#endif // USE_GCPUPROF

    while (!threads.empty()) {
        pthread_join(threads.front(), NULL);
        threads.pop_front();
    }
    for (std::vector<Scheduler *>::iterator i = schedulers.begin();
         i != schedulers.end(); i++) {
        delete *i;
    }
    delete FDWatcher::getFDWatcher();
}

Process *
Scheduler::getCurrentProcess()
{
    return _currentProcess;
}
