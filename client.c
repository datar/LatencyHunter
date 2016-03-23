#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
//#include <time.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
//#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/time.h>

#include <netdb.h>
#include <getopt.h>

#include <asm/types.h>
#include <linux/errqueue.h>

#include "DEBUG.h"

/* These are defined in socket.h, but older versions might not have all 3 */
#ifndef SO_TIMESTAMP
  #define SO_TIMESTAMP            29
#endif
#ifndef SO_TIMESTAMPNS
  #define SO_TIMESTAMPNS          35
#endif
#ifndef SO_TIMESTAMPING
  #define SO_TIMESTAMPING         37
#endif

#define BUFFSIZE 255
#define TIME_FMT "%" PRIu64 ".%.9" PRIu64 " "

void Die(char *mess) { perror(mess); exit(1); }


struct configuration {
  int protocol;
  char const* host_ip;    
  unsigned short host_port;    
  char const* server_ip;
  unsigned short server_port;
  unsigned int msg_size;
  unsigned int   max_packets; /* Stop after this many (0=forever) */
};


static void parse_options( int argc, char** argv, struct configuration* cfg )
{
  int option_index = 0;
  int opt;
  static struct option long_options[] = {
    { "protocol", required_argument, 0, 't' },
    { "host", required_argument, 0, 'i' },
    { "port", required_argument, 0, 'p' },
    { "server_ip", required_argument, 0, 's'}
    { "server_port", required_argument, 0, 'r'}
    { "msg_szie", required_argument, 0, 'm'}
    { "max", required_argument, 0, 'n' },
    { "help", no_argument, 0, 'h' },
    { 0, no_argument, 0, 0 }
  };
  char const* optstring = "tipsrmnh";

  /* Defaults */
  bzero(cfg, sizeof(struct configuration));
  cfg->protocol = IPPROTO_UDP;
  cfg->host_port = 11111;
  cfg->server_port = 11111;
  cfg->max_packets = 10;

  opt = getopt_long(argc, argv, optstring, long_options, &option_index);
  while( opt != -1 ) {
    switch( opt ) {
      case 't':
        cfg->protocol = get_protocol(optarg);
        break;
      case 'i':
        cfg->host_ip = optarg;
        break;
      case 'p':
        cfg->host_port = atoi(optarg);
        break;
      case 's':
        cfg->server_ip = optarg;
        break;
      case 'r':
        cfg->server_port = atoi(optarg);
        break;
      case 'm':
        cfg->msg_size = atoi(optarg);
        break;
      case 'n':
        cfg->max_packets = atoi(optarg);
        break;
      case 'h':
      default:
        usage();
        break;
    }
    opt = getopt_long(argc, argv, optstring, long_options, &option_index);
  }
}


int usage(){
  fprintf(stderr, "USAGE: client --host <host_ip> --port <host_port> --server_ip <server_ip> --server_port <server_port> --msg_size <size> --\n");
  exit(1);
}

int set_socket_reuse(int sock){
  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return 0;
}


void make_address(char const* host, unsigned short port, struct sockaddr_in* host_addr){
  memset(&host_addr, 0, sizeof(sockaddr_in));
  host_addr->sin_family = AF_INET;
  host_addr->sin_port = htons(port);
  if (host != NULL) {
    struct hostent *hPtr = (struct hostent *) gethostbyname(host);
    memcpy((char *)&host_addr->sin_addr, hPtr->h_addr, hPtr->h_length);
  } else {
    host_addr->sin_addr.s_addr=INADDR_ANY;
  }
}


void print_time(char *s, struct timespec *ts){
   printf("%s timestamp " TIME_FMT "\n", s, 
          (uint64_t)ts->tv_sec, (uint64_t)ts->tv_nsec);
}

void handle_time(struct msghdr* msg){
  struct timespec* udp_tx_stamp;
  struct cmsghdr* cmsg;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level != SOL_SOCKET)
      continue;
    switch(cmsg->cmsg_type) {
    case SO_TIMESTAMPING:
      udp_tx_stamp = (struct timespec*) CMSG_DATA(cmsg);
      print_time("System", &(udp_tx_stamp[0]));
      print_time("Transformed", &(udp_tx_stamp[1]));
      print_time("Raw", &(udp_tx_stamp[2]));
      break;
    default:
      printf("nothing ts\n");
      break;
    }
  }
}


static void do_ts_sockopt(int sock)
{
  int enable = 1;
  int ok = 0;

  printf("Selecting hardware timestamping mode.\n");
  enable = SOF_TIMESTAMPING_RX_HARDWARE|SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_SYS_HARDWARE |
    SOF_TIMESTAMPING_RAW_HARDWARE;
  ok = setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &enable, sizeof(int));
  if (ok < 0) {
    printf("Timestamp socket option failed.  %d (%d - %s)\n",
            ok, errno, strerror(errno));
    exit(ok);
  }
}

int main(int argc, char *argv[]) {
  struct configuration config;
  parse_options(argc, argv, &config);
  int sock;
  int received = 0;
  /* Create the UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    Die("Failed to create socket");
  }
  set_socket_reuse(sock);
  int on = 1;
  setsockopt(sock, SOL_IP, IP_PKTINFO, &on, sizeof(on));
  do_ts_sockopt(sock);
  
  struct sockaddr_in server_addr;
  make_address(config.server_ip, server_port, &server_addr)

  /*
  memset(&server_addr, 0, sizeof(server_addr));      
  server_addr.sin_family = AF_INET;                  
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);  
  server_addr.sin_port = htons(atoi(argv[2]));       
*/

  struct sockaddr_in client_addr;
  make_address(config.host_ip, host_port, &client_addr)

  if(bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0){
    Die("bind");
  }

  struct sockaddr_in recv_addr;
  make_address(0, 0, &recv_addr)       


  char buffer[config.msg_size];
  unsigned int clientlen;
  clientlen = sizeof(client_addr);

  struct msghdr msg;
  struct iovec iov;
  char control[1024];
  int got;

  iov.iov_base = buffer;
  iov.iov_len = BUFFSIZE;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_control = control;
  msg.msg_controllen = 1024;
  msg.msg_name = &recv_addr;

  char pay_load[config.msg_size];

for(int i = 0; i < config.max_packets; i++){
  sendto(sock, pay_load, 64, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
  do {
    got = recvmsg(sock, &msg, MSG_ERRQUEUE);
  } while (got < 0 && errno == EAGAIN);
  if(got >= 0){
   handle_time(&msg);
  }else{
    DEBUG("Unable to get TX_TIMESTAMPING\n");
  }

  do {
    got = recvmsg(sock, &msg, 0);
  } while (got < 0 && errno == EAGAIN);
  if(got >= 0){
   handle_time(&msg);
  }else{
    DEBUG("Unable to get RESPONSE MSG\n");
  }
  
}
  close(sock);
  exit(0);
}