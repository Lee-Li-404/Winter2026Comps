#include "worm_bsd.h"

#define NUM_FILTERS 2
char *hostname_filters[NUM_FILTERS] = {"localhost", "simh"};

int main(argc, argv) 
	int argc; 
	char *argv[];
{
	int i, docker_ip_num, hosts_num;
	struct host_info *docker_ip;
	struct host_info *hosts;

	docker_ip_num = parse_host(DOCKER_IP, &docker_ip, hostname_filters, NUM_FILTERS);
	hosts_num = parse_host(HOSTS_FILE, &hosts, hostname_filters, NUM_FILTERS);

	if (hosts_num < 0) return 1;

	for (i = 0; i < hosts_num; i++) {
			 
	}	

	return 0;
}
