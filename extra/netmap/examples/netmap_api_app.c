// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <net/if.h>

#define NETMAP_WITH_LIBS
#define DEBUG_NETMAP_USER

#include "netmap.h"
#include "netmap_user.h"

void print_packet(u_char *user, const struct nm_pkthdr *hdr, const u_char *d)
{
	printf("read pkt user:%p %p %p\n", user, hdr, d);
}

int main(int argc, char **argv)
{
	unsigned cnt = 10;
	struct nm_desc *desc;
	
	if (argc != 2) {
		fprintf(stderr, "usage: ./netmap_api_app interface\n");
		exit(1);
	}

	if ((desc = nm_open(argv[1], NULL, 0, NULL)) == NULL) {
		fprintf(stderr, "nm_open failed\n");
		exit(1);
	}

	while (cnt) {
		sleep(10);
		printf("cnt:%u\n", cnt);
		cnt -= nm_dispatch(desc, 2, print_packet, NULL);
	}

	if (nm_close(desc))
		fprintf(stderr, "nm_close failed\n");

	return 0;
}
