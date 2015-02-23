Chronicle
=========

**Chronicle** is a framework developed by the [Advanced Technology Group](https://atg.netapp.com/) at [NetApp](http://www.netapp.com/) to capture and analyze **NFSv3** workloads at **line rate** (i.e., 10Gb/s). The source code is released under an academic, non-commercial license. Please see the [copyright notice](src/LICENSE.PDF) for more information on the restrictions to use, modify, and distribute the software.

The key aspects of Chronicle that have advanced the state of the art in capturing NFS workloads are deep packet inspection (DPI), inline parsing, and efficient trace storage. For a more detailed overview of the framework and its capabilities, please refer to a [conference paper](https://www.usenix.org/conference/fast15/technical-sessions/presentation/kangarlou) presented at the 13th USENIX Conference on File and Storage Technologies ([FAST'15](https://www.usenix.org/conference/fast15/technical-sessions)). We note that the conference paper is based on earlier implementation of Chronicle. Therefore, there are a few minor differences between the implementation described in the paper vs. the implementation released on GitHub. Also, the release on GitHub only includes the trace capture functionality.

For any inquiries, please send emails to ng-chronicle@netapp.com.

What are the requirements to run Chronicle?
===========================================

Chronicle is implemented entirely in user-space and runs on commodity hardware; however, because Chronicle relies on [netmap](https://code.google.com/p/netmap/) for reading packets, it requires [netmap-aware network drivers](https://code.google.com/p/netmap/source/browse/README) (e.g., Intel ixgbe (10G), e1000/e1000e/igb (1G), Realtek 8169 (1G) and Nvidia (1G)).

We recommend at least 8 CPU cores and 64GB of RAM to support workload capture at line rate. 

What is included in the GitHub release?
=======================================

The release includes the following components under the `src` directory:

* libtask: An actor model messaging library;
* chronicle: A framework built on top of libtask for capturing NFSv3 traffic;
* fswalk: A tool to walk the file system tree and collect information about files and directories; and
* Trace anonymizer toolkit: A set of tools to anonymize, encrypt, and decrypt Chronicle traces

All software placed under the `src` directory is subject to the [Chronicle Software Academic End User License Agreement](src/LICENSE.PDF). Additionally, the release includes third party software that is not subject to this license. These software components are placed under the `extra` directory and are included mostly for maintaining cross-version compatibility.

How do I install and run Chronicle?
===================================

Please see the instructions under [INSTALL](src/INSTALL.md).

How can I contribute?
=====================

We encourage improvements to the software within the guidelines set by the [Chronicle Software Academic End User License Agreement](src/LICENSE.PDF). We particularly encourage extending support to parsing and capturing other network storage protocols.


