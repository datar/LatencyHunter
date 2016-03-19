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

int usage(){
  fprintf(stderr, "USAGE: <server_ip> <port> <word>\n");
  exit(1);
}

int set_socket_reuse(int sock){
  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return 0;
}

/*
void make_address(char const* host, unsigned short port, struct sockaddr_in* host_address){
  struct hostent *hPtr;

  bzero(host_address, sizeof(struct sockaddr_in));

  host_address->sin_family = AF_INET;
  host_address->sin_port = htons(port);

  if (host != NULL) {
    hPtr = (struct hostent *) gethostbyname(host);
    memcpy((char *)&host_address->sin_addr, hPtr->h_addr, hPtr->h_length);
  } else {
    host_address->sin_addr.s_addr=INADDR_ANY;
  }
}
*/

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
  int sock;
  
  
  unsigned int echolen;
  int received = 0;

  if (argc != 5) {
    usage();
  }
 

  /* Create the UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    Die("Failed to create socket");
  }
  set_socket_reuse(sock);
  int on = 1;
  setsockopt(sock, SOL_IP, IP_PKTINFO, &on, sizeof(on));
  do_ts_sockopt(sock);
  
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));       /* Clear struct */
  server_addr.sin_family = AF_INET;                  /* Internet/IP */
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);  /* IP address */
  server_addr.sin_port = htons(atoi(argv[2]));       /* server port */

  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));       
  client_addr.sin_family = AF_INET;                  
  client_addr.sin_addr.s_addr=inet_addr(argv[3]);
  client_addr.sin_port =  htons(atoi(argv[4]));;       
  if(bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0){
    Die("bind");
  }

  struct sockaddr_in recv_addr;
  memset(&recv_addr, 0, sizeof(recv_addr));       
  recv_addr.sin_family = AF_INET;                  
  recv_addr.sin_addr.s_addr=INADDR_ANY;
  recv_addr.sin_port = htons(0);       

  /* Send the word to the server */
  /*
  echolen = strlen(argv[3]);
  if (sendto(sock, argv[3], echolen, 0,
             (struct sockaddr *) &server_addr,
             sizeof(server_addr)) != echolen) {
    Die("Mismatch in number of sent bytes");
  }
  */






  char buffer[BUFFSIZE];
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
  

  char pay_load[64];
for(int i = 0; i < 10; i++){
  //msg.msg_name = &server_addr;
  //sendmsg(sock, &msg, 0);
  DEBUG("------------------------------");
  sendto(sock, pay_load, 64, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
  do {
    got = recvmsg(sock, &msg, MSG_ERRQUEUE);
  } while (got < 0 && errno == EAGAIN);
  if(got >= 0){
   handle_time(&msg);
  }else{
    printf("got < 0\n");
  }

  do {
    got = recvmsg(sock, &msg, 0);
  } while (got < 0);
  if(got >= 0){
   handle_time(&msg);
  }else{
    printf("got < 0\n");
  }
  
}
  close(sock);
  exit(0);
}