#include "headers.h"

static void sendpacket(int sock, struct sockaddr *addr, socklen_t addr_len)
{
	struct timeval now;
	int res;

	struct timespec app_ts;
	size_t timespec_size = sizeof(struct timespec);
	//clock_gettime(CLOCK_MONOTONIC_RAW, &app_ts);
	clock_gettime(CLOCK_REALTIME, &app_ts);
	//res = sendto(sock, sync, sizeof(sync), 0, addr, addr_len);
	res = sendto(sock, &app_ts, timespec_size, 0, addr, addr_len);
	gettimeofday(&now, 0);
	if (res < 0)
		DEBUG("IN_SENDPACKET::%s: %s\n", "send", strerror(errno));
	else
		DEBUG("IN_SENDPACKET::%ld.%06ld: sent %d bytes\n", (long)now.tv_sec, (long)now.tv_usec, res);
}


int set_socket_reuse(int sock){
	int on = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	return 0;
}

static void make_address(char const* host, unsigned short port, struct sockaddr_in* host_address){
  struct hostent *hPtr;

  bzero(host_address, sizeof(struct sockaddr_in));

  host_address->sin_family = AF_INET;
  host_address->sin_port = htons(port);

  if (host != NULL) {
    hPtr = (struct hostent *) gethostbyname(host);
    TEST( hPtr != NULL );

    memcpy((char *)&host_address->sin_addr, hPtr->h_addr, hPtr->h_length);
  } else {
    host_address->sin_addr.s_addr=INADDR_ANY;
  }
}

int setup_socket(int sock, struct configuration cfg){
	int so_timestamping_flags = 0;
	int so_timestamp = 0;
	int so_timestampns = 0;
	int siocgstamp = 0;
	int siocgstampns = 0;
	int ip_multicast_loop = 0;
	int i;
	int enabled = 1;
	
	struct ifreq device;
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig, hwconfig_requested;
	struct sockaddr_in addr;
	struct ip_mreq imr;
	struct in_addr iaddr;
	int val;
	socklen_t len;
	struct timeval next;

	so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
		bail("socket");
	

	memset(&device, 0, sizeof(device));
	strncpy(device.ifr_name, cfg->interface, sizeof(device.ifr_name));
	if (ioctl(sock, SIOCGIFADDR, &device) < 0)
		bail("getting interface IP address");

	memset(&hwtstamp, 0, sizeof(hwtstamp));
	strncpy(hwtstamp.ifr_name, cfg->interface, sizeof(hwtstamp.ifr_name));
	hwtstamp.ifr_data = (void *)&hwconfig;
	memset(&hwconfig, 0, sizeof(hwconfig));
	

	hwconfig.tx_type =
		(so_timestamping_flags & SOF_TIMESTAMPING_TX_HARDWARE) ?
		HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
	hwconfig.rx_filter =
		(so_timestamping_flags & SOF_TIMESTAMPING_RX_HARDWARE) ?
		//HWTSTAMP_FILTER_PTP_V1_L4_SYNC : HWTSTAMP_FILTER_NONE;
		HWTSTAMP_FILTER_ALL : HWTSTAMP_FILTER_NONE;
	hwconfig_requested = hwconfig;

	
	if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) < 0) {
		if ((errno == EINVAL || errno == ENOTSUP) &&
		    hwconfig_requested.tx_type == HWTSTAMP_TX_OFF &&
		    hwconfig_requested.rx_filter == HWTSTAMP_FILTER_NONE)
			DEBUG("SIOCSHWTSTAMP: disabling hardware time stamping not possible\n");
		else
			bail("SIOCSHWTSTAMP");
	}
	DEBUG("SIOCSHWTSTAMP: tx_type %d requested, got %d; rx_filter %d requested, got %d\n", hwconfig_requested.tx_type, hwconfig.tx_type, hwconfig_requested.rx_filter, hwconfig.rx_filter);

	
	// bind to PTP port 
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(321); //PTP event port 
	if (bind(sock,
		 (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_in)) < 0)
		bail("bind");
	
	//* set multicast group for outgoing packets
	inet_aton("224.0.0.1", &iaddr); //* alternate PTP domain 1 
	addr.sin_addr = iaddr;
	imr.imr_multiaddr.s_addr = iaddr.s_addr;
	imr.imr_interface.s_addr =
		((struct sockaddr_in *)&device.ifr_addr)->sin_addr.s_addr;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0)
		bail("set multicast");

	//* join multicast group, loop our own packet 
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		       &imr, sizeof(struct ip_mreq)) < 0)
		bail("join multicast group");

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
		       &ip_multicast_loop, sizeof(enabled)) < 0) {
		bail("loop multicast");
	}

	// set socket options for time stamping 
	if (so_timestamp &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP,
			   &enabled, sizeof(enabled)) < 0)
		bail("setsockopt SO_TIMESTAMP");

	if (so_timestampns &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS,
			   &enabled, sizeof(enabled)) < 0)
		bail("setsockopt SO_TIMESTAMPNS");

	if (so_timestamping_flags &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING,
			   &so_timestamping_flags,
			   sizeof(so_timestamping_flags)) < 0)
		bail("setsockopt SO_TIMESTAMPING");

	// request IP_PKTINFO for debugging purposes 
	if (setsockopt(sock, SOL_IP, IP_PKTINFO,
		       &enabled, sizeof(enabled)) < 0)
		DEBUG("%s: %s\n", "setsockopt IP_PKTINFO", strerror(errno));

	// verify socket options 
	len = sizeof(val);
	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &val, &len) < 0)
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMP", strerror(errno));
	else
		DEBUG("SO_TIMESTAMP %d\n", val);

	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS, &val, &len) < 0)
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMPNS",
		       strerror(errno));
	else
		DEBUG("SO_TIMESTAMPNS %d\n", val);

	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &val, &len) < 0) {
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMPING",
		       strerror(errno));
	} else {
		DEBUG("SO_TIMESTAMPING %d\n", val);
		if (val != so_timestamping_flags)
			DEBUG("   not the expected value %d\n",
			       so_timestamping_flags);
	}

	return 0;
}



int sender(int argc, char **argv)
{
	struct configuration cfg;
	parse_options(argc, argv, &cfg);

	int sock;




	// send packets forever every five seconds 
	//gettimeofday(&next, 0);
	//next.tv_sec = (next.tv_sec + 1) ;
	//next.tv_usec = 0;
	
	while(1){
		sendpacket(sock, (struct sockaddr *)&addr, sizeof(addr));
	}
/*
	while (1) {
		struct timeval now;
		struct timeval delta;
		long delta_us;
		int res;
		fd_set readfs, errorfs;

		gettimeofday(&now, 0);
		delta_us = (long)(next.tv_sec - now.tv_sec) * 1000000 + (long)(next.tv_usec - now.tv_usec);

		if(delta_us < 0){
			// write one packet 
			sendpacket(sock, (struct sockaddr *)&addr, sizeof(addr));
			next.tv_sec = next.tv_sec + ((long)(1000+next.tv_usec))/1000000;
			next.tv_usec = ((long)(1000+next.tv_usec))%1000000;
			continue;
		}
	}
*/	
	return 0;
}



/*
int sender(int argc, char **argv)
{
	int so_timestamping_flags = 0;
	int so_timestamp = 0;
	int so_timestampns = 0;
	int siocgstamp = 0;
	int siocgstampns = 0;
	int ip_multicast_loop = 0;
	char *interface;
	int i;
	int enabled = 1;
	int sock;
	struct ifreq device;
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig, hwconfig_requested;
	struct sockaddr_in addr;
	struct ip_mreq imr;
	struct in_addr iaddr;
	int val;
	socklen_t len;
	struct timeval next;

	interface = "eth0";

	so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
	//so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
		bail("socket");
	int on = 1;
	setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

	memset(&device, 0, sizeof(device));
	strncpy(device.ifr_name, interface, sizeof(device.ifr_name));
	if (ioctl(sock, SIOCGIFADDR, &device) < 0)
		bail("getting interface IP address");

	memset(&hwtstamp, 0, sizeof(hwtstamp));
	strncpy(hwtstamp.ifr_name, interface, sizeof(hwtstamp.ifr_name));
	hwtstamp.ifr_data = (void *)&hwconfig;
	memset(&hwconfig, 0, sizeof(hwconfig));
	

	hwconfig.tx_type =
		(so_timestamping_flags & SOF_TIMESTAMPING_TX_HARDWARE) ?
		HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
	hwconfig.rx_filter =
		(so_timestamping_flags & SOF_TIMESTAMPING_RX_HARDWARE) ?
		//HWTSTAMP_FILTER_PTP_V1_L4_SYNC : HWTSTAMP_FILTER_NONE;
		HWTSTAMP_FILTER_ALL : HWTSTAMP_FILTER_NONE;
	hwconfig_requested = hwconfig;

	
	if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) < 0) {
		if ((errno == EINVAL || errno == ENOTSUP) &&
		    hwconfig_requested.tx_type == HWTSTAMP_TX_OFF &&
		    hwconfig_requested.rx_filter == HWTSTAMP_FILTER_NONE)
			DEBUG("SIOCSHWTSTAMP: disabling hardware time stamping not possible\n");
		else
			bail("SIOCSHWTSTAMP");
	}
	DEBUG("SIOCSHWTSTAMP: tx_type %d requested, got %d; rx_filter %d requested, got %d\n", hwconfig_requested.tx_type, hwconfig.tx_type, hwconfig_requested.rx_filter, hwconfig.rx_filter);

	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(321); //PTP event port 
	if (bind(sock,
		 (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_in)) < 0)
		bail("bind");
	
	//* set multicast group for outgoing packets
	inet_aton("11.11.11.2", &iaddr); 
	addr.sin_addr = iaddr;
        
	imr.imr_multiaddr.s_addr = iaddr.s_addr;
	imr.imr_interface.s_addr =
		((struct sockaddr_in *)&device.ifr_addr)->sin_addr.s_addr;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0)
		bail("set multicast");

	//* join multicast group, loop our own packet 
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		       &imr, sizeof(struct ip_mreq)) < 0)
		bail("join multicast group");

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
		       &ip_multicast_loop, sizeof(enabled)) < 0) {
		bail("loop multicast");
	}

	// set socket options for time stamping 
	if (so_timestamp &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP,
			   &enabled, sizeof(enabled)) < 0)
		bail("setsockopt SO_TIMESTAMP");

	if (so_timestampns &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS,
			   &enabled, sizeof(enabled)) < 0)
		bail("setsockopt SO_TIMESTAMPNS");

	if (so_timestamping_flags &&
		setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING,
			   &so_timestamping_flags,
			   sizeof(so_timestamping_flags)) < 0)
		bail("setsockopt SO_TIMESTAMPING");

	// request IP_PKTINFO for debugging purposes 
	if (setsockopt(sock, SOL_IP, IP_PKTINFO,
		       &enabled, sizeof(enabled)) < 0)
		DEBUG("%s: %s\n", "setsockopt IP_PKTINFO", strerror(errno));

	// verify socket options 
	len = sizeof(val);
	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &val, &len) < 0)
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMP", strerror(errno));
	else
		DEBUG("SO_TIMESTAMP %d\n", val);

	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS, &val, &len) < 0)
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMPNS",
		       strerror(errno));
	else
		DEBUG("SO_TIMESTAMPNS %d\n", val);

	if (getsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &val, &len) < 0) {
		DEBUG("%s: %s\n", "getsockopt SO_TIMESTAMPING",
		       strerror(errno));
	} else {
		DEBUG("SO_TIMESTAMPING %d\n", val);
		if (val != so_timestamping_flags)
			DEBUG("   not the expected value %d\n",
			       so_timestamping_flags);
	}


	// send packets forever every five seconds 
	gettimeofday(&next, 0);
	//next.tv_sec = (next.tv_sec + 1) ;
	//next.tv_usec = 0;
	while (1) {
		struct timeval now;
		struct timeval delta;
		long delta_us;
		int res;
		fd_set readfs, errorfs;

		gettimeofday(&now, 0);
		delta_us = (long)(next.tv_sec - now.tv_sec) * 1000000 + (long)(next.tv_usec - now.tv_usec);

		if(delta_us < 0){
			// write one packet 
			sendpacket(sock,
				   (struct sockaddr *)&addr,
				   sizeof(addr));
			next.tv_sec = next.tv_sec + ((long)(1000+next.tv_usec))/1000000;
			next.tv_usec = ((long)(1000+next.tv_usec))%1000000;
			continue;
		}
	}
	
	return 0;
}
*/