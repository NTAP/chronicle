#!/bin/bash

function usage {
	echo "usage: $0 [start | stop]"
}

if [ $# -ne 1 ]; then
	usage
	exit 1
fi

function bring_up_interfaces {
	for i in `ifconfig -a | grep eth | awk '{print $1}'`;
	do
		echo "bringing up $i"
		sudo /sbin/ifconfig $i mtu 9000 up promisc
		sudo /sbin/ethtool -K $i tso off
		sudo /sbin/ethtool -K $i gso off
		sudo /sbin/ethtool -K $i gro off
		sudo /sbin/ethtool -K $i lro off
	done
}

function bring_down_interfaces {
	for i in `ifconfig -a | grep eth | awk '{print $1}'`;
	do
		echo "bringing down $i"
		sudo /sbin/ifconfig $i down
	done
}

function start_netmap {
	bring_down_interfaces
	sudo /sbin/rmmod igb
	sudo /sbin/rmmod e1000e
	sudo /sbin/rmmod ixgbe
	sudo /sbin/insmod ./netmap_lin.ko 
	sudo /sbin/insmod ./igb.ko
	sudo /sbin/insmod ./e1000e.ko
	sudo /sbin/insmod ./ixgbe.ko
	bring_up_interfaces
}

function stop_netmap {
	bring_down_interfaces
	sudo /sbin/rmmod igb
	sudo /sbin/rmmod e1000e
	sudo /sbin/rmmod ixgbe
	sudo /sbin/rmmod netmap_lin
	sudo modprobe igb
	sudo modprobe e1000e
	sudo modprobe ixgbe
	bring_up_interfaces
}

case "$1" in
	start)
		echo "setting up netmap"
		start_netmap
		;;
	stop)
		echo "bringing down netmap"
		stop_netmap
		;;
	*)
		usage
		exit 1
		;;
esac

exit 0

