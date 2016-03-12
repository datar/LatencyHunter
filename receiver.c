#include "headers.h"


#define TSCOL 6
#define PACKET_COUNT 1000000

int packet_count = PACKET_COUNT;
int test_duration = 10;
long result[PACKET_COUNT][TSCOL];
int current = 0;








static void sendpacket(int sock, struct sockaddr *addr, socklen_t addr_len)
{
	struct timeval now;
	int res;

	res = sendto(sock, sync, sizeof(sync), 0,
		addr, addr_len);
	gettimeofday(&now, 0);
	if (res < 0)
		DEBUG("IN_SENDPACKET::%s: %s\n", "send", strerror(errno));
	else
		DEBUG("IN_SENDPACKET::%ld.%06ld: sent %d bytes\n",
		       (long)now.tv_sec, (long)now.tv_usec,
		       res);
}

static void printpacket(struct msghdr *msg, int res,
			char *data,
			int sock, int recvmsg_flags,
			int siocgstamp, int siocgstampns)
{
	struct timespec *sender_ts;
	
	struct sockaddr_in *from_addr = (struct sockaddr_in *)msg->msg_name;
	struct cmsghdr *cmsg;
	struct timeval tv;
	struct timespec ts;
	struct timeval now;

	gettimeofday(&now, 0);

	DEBUG("%ld.%06ld: received %s data, %d bytes from %s, %zu bytes control messages\n",
	       (long)now.tv_sec, (long)now.tv_usec,
	       (recvmsg_flags & MSG_ERRQUEUE) ? "error" : "regular",
	       res,
	       inet_ntoa(from_addr->sin_addr),
	       msg->msg_controllen);
	
	sender_ts = data;
	result[current][0] = (long)sender_ts->tv_sec;
	result[current][1] = (long)sender_ts->tv_nsec;
	DEBUG("Sender_ts %ld.%09ld\t", (long)sender_ts->tv_sec, (long)sender_ts->tv_nsec);
	
	for (cmsg = CMSG_FIRSTHDR(msg);
	     cmsg;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		DEBUG("   cmsg len %zu: ", cmsg->cmsg_len);
		switch (cmsg->cmsg_level) {
		case SOL_SOCKET:
			DEBUG("SOL_SOCKET ");
			switch (cmsg->cmsg_type) {
			case SO_TIMESTAMP: {
				struct timeval *stamp =
					(struct timeval *)CMSG_DATA(cmsg);
				DEBUG("SO_TIMESTAMP %ld.%06ld",
				       (long)stamp->tv_sec,
				       (long)stamp->tv_usec);
				break;
			}
			case SO_TIMESTAMPNS: {
				struct timespec *stamp =
					(struct timespec *)CMSG_DATA(cmsg);
				DEBUG("SO_TIMESTAMPNS %ld.%09ld",
				       (long)stamp->tv_sec,
				       (long)stamp->tv_nsec);
				break;
			}
			case SO_TIMESTAMPING: {
				struct timespec *stamp =
					(struct timespec *)CMSG_DATA(cmsg);
				DEBUG("SO_TIMESTAMPING ");
				DEBUG("SW %ld.%09ld ",
				       (long)stamp->tv_sec,
				       (long)stamp->tv_nsec);
				stamp++;
				/* skip deprecated HW transformed */
				stamp++;
				result[current][2] = (long)stamp->tv_sec;
				result[current][3] = (long)stamp->tv_nsec;
				DEBUG("HW raw %ld.%09ld",
				       (long)stamp->tv_sec,
				       (long)stamp->tv_nsec);
				print_timespec(stamp);
				break;
			}
			default:
				DEBUG("type %d", cmsg->cmsg_type);
				break;
			}
			break;
		case IPPROTO_IP:
			DEBUG("IPPROTO_IP ");
			switch (cmsg->cmsg_type) {
			case IP_RECVERR: {
				struct sock_extended_err *err =
					(struct sock_extended_err *)CMSG_DATA(cmsg);
				DEBUG("IP_RECVERR ee_errno '%s' ee_origin %d => %s",
					strerror(err->ee_errno),
					err->ee_origin,
#ifdef SO_EE_ORIGIN_TIMESTAMPING
					err->ee_origin == SO_EE_ORIGIN_TIMESTAMPING ?
					"bounced packet" : "unexpected origin"
#else
					"probably SO_EE_ORIGIN_TIMESTAMPING"
#endif
					);
				if (res < sizeof(sync))
					DEBUG(" => truncated data?!");
				else if (!memcmp(sync, data + res - sizeof(sync),
							sizeof(sync)))
					DEBUG(" => GOT OUR DATA BACK (HURRAY!)");
				break;
			}
			case IP_PKTINFO: {
				struct in_pktinfo *pktinfo =
					(struct in_pktinfo *)CMSG_DATA(cmsg);
				DEBUG("IP_PKTINFO interface index %u",
					pktinfo->ipi_ifindex);
				break;
			}
			default:
				DEBUG("type %d", cmsg->cmsg_type);
				break;
			}
			break;
		default:
			DEBUG("level %d type %d",
				cmsg->cmsg_level,
				cmsg->cmsg_type);
			break;
		}
		DEBUG("\n");
	}

	if (siocgstamp) {
		if (ioctl(sock, SIOCGSTAMP, &tv))
			DEBUG("   %s: %s\n", "SIOCGSTAMP", strerror(errno));
		else
			DEBUG("SIOCGSTAMP %ld.%06ld\n",
			       (long)tv.tv_sec,
			       (long)tv.tv_usec);
	}
	if (siocgstampns) {
		if (ioctl(sock, SIOCGSTAMPNS, &ts))
			DEBUG("   %s: %s\n", "SIOCGSTAMPNS", strerror(errno));
		else
			DEBUG("SIOCGSTAMPNS %ld.%09ld\n",
			       (long)ts.tv_sec,
			       (long)ts.tv_nsec);
	}
}

static void recvpacket(int sock, int recvmsg_flags,
		       int siocgstamp, int siocgstampns)
{
	DEBUG("RecvPacket!!!\n");
	char data[256];
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_in from_addr;
	struct {
		struct cmsghdr cm;
		char control[512];
	} control;
	int res;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = data;
	entry.iov_len = sizeof(data);
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	res = recvmsg(sock, &msg, recvmsg_flags|MSG_DONTWAIT);
	if (res < 0) {
		DEBUG("%s %s: %s\n",
		       "recvmsg",
		       (recvmsg_flags & MSG_ERRQUEUE) ? "error" : "regular",
		       strerror(errno));
	} else {
		printpacket(&msg, res, data,
			    sock, recvmsg_flags,
			    siocgstamp, siocgstampns);
	}
}

int recerver(int argc, char **argv)
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
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;

	so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;
	

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
	DEBUG("SIOCSHWTSTAMP: tx_type %d requested, got %d; rx_filter %d requested, got %d\n",
	       hwconfig_requested.tx_type, hwconfig.tx_type,
	       hwconfig_requested.rx_filter, hwconfig.rx_filter);

	
	/* bind to PTP port */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(321); 
	if (bind(sock,
		 (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_in)) < 0)
		bail("bind");
	
	/* set multicast group for outgoing packets*/
	inet_aton("224.0.0.1", &iaddr); 
	addr.sin_addr = iaddr;
	imr.imr_multiaddr.s_addr = iaddr.s_addr;
	imr.imr_interface.s_addr =
		((struct sockaddr_in *)&device.ifr_addr)->sin_addr.s_addr;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0)
		bail("set multicast");

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		       &imr, sizeof(struct ip_mreq)) < 0)
		bail("join multicast group");

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
		       &ip_multicast_loop, sizeof(enabled)) < 0) {
		bail("loop multicast");
	}

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

	/* request IP_PKTINFO for debugging purposes */
	if (setsockopt(sock, SOL_IP, IP_PKTINFO,
		       &enabled, sizeof(enabled)) < 0)
		DEBUG("%s: %s\n", "setsockopt IP_PKTINFO", strerror(errno));

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


	/* send packets forever every five seconds */
	gettimeofday(&next, 0);
	next.tv_sec = (next.tv_sec + 1) ;
	next.tv_usec = 0;



	struct timespec app_ts;
	size_t timespec_size = sizeof(struct timespec);
	
	while (current<packet_count) {
		struct timeval now;
		struct timeval delta;
		long delta_us;
		int res;
		fd_set readfs, errorfs;

		FD_ZERO(&readfs);
		FD_ZERO(&errorfs);
		FD_SET(sock, &readfs);
		FD_SET(sock, &errorfs);
		DEBUG("%ld.%06ld: select %ldus\n",
		       (long)now.tv_sec, (long)now.tv_usec,
		       delta_us);
		res = select(sock + 1, &readfs, 0, &errorfs, &delta);
		gettimeofday(&now, 0);

		if (res > 0) {
			if (FD_ISSET(sock, &readfs)){
	
				clock_gettime(CLOCK_REALTIME, &app_ts);
				result[current][4] = (long)app_ts.tv_sec;
				result[current][5] = (long)app_ts.tv_nsec;
				recvpacket(sock, 0, siocgstamp, siocgstampns);
				current = current + 1;
			}
			if (FD_ISSET(sock, &errorfs))
				DEBUG("has error\n");
			
			/*recvpacket(sock, MSG_ERRQUEUE, siocgstamp, siocgstampns);*/
		}
		
	}
	FILE* outfile;
	outfile = fopen("result.txt", "w");
	for(current = 0; current < packet_count; current++){
		fprintf(outfile, "%ld,%9ld,%ld,%9ld,%ld,%9ld\n", result[current][0],result[current][1],result[current][2],result[current][3],result[current][4],result[current][5]);
	}
	close(outfile);
	return 0;
}