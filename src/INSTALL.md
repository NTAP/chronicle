
How do I install Chronicle?
===========================

Please first read the [README](../README.md) file for an overview of Chronicle.

Chronicle relies on a number of software packages to compile and run. Most dependencies can be installed using your Linux distribution's package manager. On a Debian/Ubuntu system, you can use the following instructions to install these dependencies:
      
    apt-get install git ethtool cmake libboost-dev libboost-thread-dev libboost-program-options-dev libboost-signals-dev libboost-system-dev libboost-filesystem-dev libxml2-dev libbz2-dev liblzo2-dev zlib1g-dev libssl-dev libpcre3-dev libpcap-dev doxygen g++ make libgoogle-perftools-dev libaio-dev python-pyinotify

Additionally, there are a few packages that require a manual install: 

* [Lintel](https://github.com/dataseries/Lintel) 
* [DataSeries](https://github.com/dataseries/DataSeries) 
* [netmap](https://code.google.com/p/netmap/) 

For the packages listed above, we suggest that you follow the instructions included in the respective packages to install them. 

Chronicle software distribution does include a slightly modified version of `netmap` to support jumbo frames. This version is included to ensure API compatibility between netmap and Chronicle. It also serves to demonstrate how future versions of netmap can be integrated with Chronicle. 

For instance, to install the included version of netmap with `linux-3.2.32` on a Debian/Ubuntu system, use the following steps:

First, install prerequisite packages:

    apt-get install libncurses5 libncurses5-dev kernel-package

Download and compile kernel source to create a .deb package:

    mkdir CHRONICLE_ROOT/linux-3.2.32; cd CHRONICLE_ROOT/linux-3.2.32
    wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.2.32.tar.bz2
    tar jxvf linux-3.2.32.tar.bz2
    cd linux-3.2.32
    make menuconfig (choose appropriate capabilities, exit, and save)
    make-kpkg --append-to-version=-netmap kernel-image -j16 (using 16 cores)

Install the .deb kernel package:
    
    cd ..
    dpkg -i linux-image-3.2.32-netmap_3.2.32-netmap-10.00.Custom_amd64.deb
    update-initramfs -c -k 3.2.32-netmap
    update-grub

Verify installation and reboot:

    ls -lt /boot/
    cat /boot/grub/grub.cfg
    reboot

Verify booting into the right kernel:

    uname -r (should show 3.2.32-netmap)
 
Compile netmap:

     cd CHRONICLE_ROOT/extra/netmap/LINUX
     make
     make install

Load the netmap and modified driver modules (preferably using KVM or other remote management utilties):

    cd CHRONICLE_ROOT/extra/netmap/CHRONICLE/linux3.2.32-modules
    ./netmap-script.sh start

Verify the netmap module and the custom network drivers are loaded:

    lsmod | grep netmap_lin

To compile Chronicle, follow these steps:

    mkdir CHRONICLE_ROOT/build; cd CHRONICLE_ROOT/build
    cmake ../src -DCMAKE_BUILD_TYPE=Optimized
    make -j

Verify the build:

    make test

How do I run Chronicle?
=======================

If the build was successful, you should see two binaries under `CHRONICLE_ROOT/build/chronicle`: `chronicle_netmap` and `chronicle_pcap`. The former uses the netmap interface for reading packets and should be used for capturing NFS workloads at line rate. The latter uses the standard pcap interface to read packets from NICs or from pcap savefiles (i.e., the output of tcpdump and Wireshark). Both binaries are capable of generating traces in the DataSeries and pcap formats:

    cd chronicle
    ./chronicle_netmap -h
    ./chronicle_pcap -h

For example, to capture traces in the DataSeries format by reading packets from eth1 and eth2 interfaces, and by using 4 Chronicle pipelines, use the following command:

    ./chronicle_netmap -ieth1 -ieth2 -n4 -D1


Similarly, to process pcap savefiles `file1.pcap` and `file2.pcap` with 4 Chronicle pipelines, use the following command:

    ./chronicle_pcap -s file1.pcap -s file2.pcap -n4 -D1

The command above allows us to convert NFS workloads captured by standard tools like tcpdump or Wireshark into the DataSeries format. For this application scenario, please ensure that pcap traces contain full-sized packets.    


How do I know if Chronicle is running fine?
===========================================

Chronicle periodically prints out per-pipeline statistics as far as the number of PDUs it parses. The number of good PDUs vs. bad PDUs is typically a good indicator of the quality of the data feed. For better monitoring of Chronicle's operations, we recommend installing [Graphite](http://graphite.wikidot.com/). With Graphite, one can view the following metrics, among many others, in almost real-time:

* Current, average, and maximum packet buffer pool utilization
* The number of different NFS operations parsed  
* Per-Process backlog

Please note that Chronicle reports these metrics every 10 seconds. For more information, please check the source code (`CHRONICLE_ROOT/src/chronicle/StatGatherer.*`).

To install Graphite on Debian/Ubuntu, follow the Graphite documentation or the following steps:

First, install the dependencies:

    apt­get install libpq-dev python-dev python-pip python-cairo python-psycopg2 python-django python-django-tagging python-pysqlite2 apache2 libapache2-mod-wsgi libapache2-mod-python sqlite3 memcached python-memcache
 
Second, install the Graphite components with Pip:

    pip install whisper 
    pip install carbon 
    pip install graphite-web
 
Third, configure carbon:

    cd /opt/graphite 
    cp conf/carbon.conf.example conf/carbon.conf
    cp conf/graphite.wsgi.example conf/graphite.wsgi
    cp CHRONICLE_ROOT/extra/graphite/storage-schemas.conf conf/storage-schemas.conf

 Fourth, configure Apache:

    cp CHRONICLE_ROOT/extra/graphite/example-graphite-vhost.conf /etc/apache2/sites-available/default

Fifth, uncomment `WSGISocketPrefix /var/run/apache2/wsgi` in `/etc/apache2/mods-available/wsgi.conf` and restart Apache:

    /etc/init.d/apache2 reload 

Sixth, initialize the internal database for Graphite administration: 
    
    cd /opt/graphite/webapp/graphite/
    cp CHRONICLE_ROOT/extra/graphite/local_settings.py local_settings.py
    python manage.py syncdb
    chown ­-R www-data:www-data /opt/graphite/storage/
    /etc/init.d/apache2 restart
 
Seventh, start Carbon:

    /opt/graphite/bin/carbon-cache.py start

Eighth, test the installation by feeding some data into Graphite: 

    python /opt/graphite/examples/example-client.py

Visit `http://localhost` and log in. You should see metrics under `system>loadavg_*` 

Please note that these instructions are based on older versions of Graphite and Apache. The recent version of Django is particularly not compatible with Graphite due to removal of `django.conf.urls.defaults`. You can manually identify and update the affected files:

    cd /opt/graphite/webapp
    grep -rn "django.conf.urls.defaults" *
  
How can I examine Chronicle traces?
===================================

The easiest method for displaying the contents of a trace file is via the `ds2txt` utility that comes with DataSeries. This utility dumps the contents in a tabular text format. The first part of the output is the XML description of all of the record types in the file. This will be followed by the contents of each “extent” of records in the file.
 
A more useful method of viewing the data is to examine a single record type. This can be done using the `select` option: 

    ds2txt --select “extentTypeName” “*” chronicle_*.ds

Basic statistics can be run over the records using DataSeries' `dsstatgroupby` utility. For example, the breakdown of NFS operation types can be constructed from the records in the `trace::rpc` extent. To do this, we want to run the statistics over that extent type, selecting only the records that have a matching RPC reply, and count the number of operations by type:

        dsstatgroupby trace::rpc basic 1 where 'reply_at!=0' group by operation from chronicle_*.ds
 
As another example, the following provides the distribution of I/O sizes for read and write NFS operations. The extent containing the information about read and write operations is `trace::rpc::nfs3::readwrite`. We do not need a `where` clause since the operation­-specific tables only have entries for complete operations. We are calculating the statistics over 
the field called `count` which is the number of bytes requested by the client. Here we also ask for the more detailed `qualtile` type of statistic as opposed to the `basic` that we used in the previous example.

    dsstatgroupby trace::rpc::nfs3::readwrite quantile count group by is_read from  chronicle_*.ds

FAQ
===
**Why does Chronicle not compile?**

* Please make sure the linker can find the prerequisite libraries (e.g., libDataSeries & libLintel)
* If you did not use the included version of netmap, make sure the header files (`netmap.h` and `netmap_user.h`) from the installed version are included in `CHRONICLE_ROOT/src/chronicle/NetmapInterface.h`. You may need to make small changes to these files to get them compiled. Please check the header files in `CHRONICLE_ROOT/extra/netmap/sys/net/netmap` for reference.

**Why does Chronicle crash?**

Chronicle can crash for a number of reasons: 

* If you are reading packet from NICs, you need to make sure that you are running Chronicle as root.
* If you are planning to use netmap, please make sure that the netmap kernel module is loaded.
* If you get an out-of-memory error, you need to modify `STAND_PKTS_IN_BUFFER_POOL` and `JUMBO_PKTS_IN_BUFFER_POOL` in ``CHRONICLE_ROOT/src/chronicle/ChronicleConfig.h`. These parameters determine the number of packet buffer pool entries for standard size frames (<=1500B) and jumbo frames (<= 9000B). All parameters that can be used to tune Chronicle are either defined in `ChronicleConfig.h` or are passed as command line arguments.
* If you see a message like `NetworkHeaderParser::parseTcpSegment: Dropping bad packet` when you run Chronicle, that indicates you are reading frames that are too large to be handled by Chronicle. The solution is to set the right MTU size for interfaces and/or to disable gso, tso, lro, and gro functions in the NIC. Please refer to `CHRONICLE_ROOT/extra/netmap/CHRONICLE/scripts/netmap-script.sh` for more details.



