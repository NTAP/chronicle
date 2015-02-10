#include <cstdio>
#include <cstdlib>
#include <list>
#include <unistd.h>
#include "TopologyMap.h"

int main()
{	
	std::list<int> l;
	std::list<int>::iterator it;
	TopologyMap *map = new TopologyMap();
	
	if (map->init() == -1) 
		exit(EXIT_FAILURE);
	
	map->print();

	//TODO: replace everything below with Google Mock tests!
	for(int i = 0; i < sysconf(_SC_NPROCESSORS_CONF); i++) {
		map->sameCore(i, l);
		printf("same core as %u: ", i);
		for (it = l.begin(); it != l.end(); it++) 
			printf("%d ", *it);
		printf("\n");
		l.clear();
	}
	
	l.clear();
	map->sameSocket(0, l);
	printf("same socket as 0: ");
	for (it = l.begin(); it != l.end(); it++) 
		printf("%d ", *it);
	printf("\n");
	
	l.clear();
	map->sameSocket(sysconf(_SC_NPROCESSORS_CONF) - 1, l);
	printf("same socket as %ld: ", sysconf(_SC_NPROCESSORS_CONF)-1);
	for (it = l.begin(); it != l.end(); it++) 
		printf("%d ", *it);
	printf("\n");

	l.clear();
	map->getPriorityList(0, l);
	printf("priority list for 0: ");
	for (it = l.begin(); it != l.end(); it++) 
		printf("%d ", *it);
	printf("\n");

	l.clear();
	map->getPriorityList((sysconf(_SC_NPROCESSORS_CONF) - 1)/2, l);
	printf("priority list for %ld: ", (sysconf(_SC_NPROCESSORS_CONF) - 1)/2);
	for (it = l.begin(); it != l.end(); it++) 
		printf("%d ", *it);
	printf("\n");	

	return 0;
}
