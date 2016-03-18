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
  struct sockaddr_in echoserver;
  
  unsigned int echolen;
  int received = 0;

  if (argc != 4) {
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
  /* Construct the server sockaddr_in structure */
  memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
  echoserver.sin_family = AF_INET;                  /* Internet/IP */
  echoserver.sin_addr.s_addr = inet_addr(argv[1]);  /* IP address */
  echoserver.sin_port = htons(atoi(argv[2]));       /* server port */
  

  /* Send the word to the server */
  /*
  echolen = strlen(argv[3]);
  if (sendto(sock, argv[3], echolen, 0,
             (struct sockaddr *) &echoserver,
             sizeof(echoserver)) != echolen) {
    Die("Mismatch in number of sent bytes");
  }
  */



  struct sockaddr_in echoclient;
  memset(&echoclient, 0, sizeof(echoclient));       
  echoclient.sin_family = AF_INET;                  
  echoclient.sin_addr.s_addr=INADDR_ANY;
  //echoclient.sin_port = htons(atoi(argv[2]));       



  char buffer[BUFFSIZE];
  unsigned int clientlen;
  /* Receive the word back from the server */
  fprintf(stdout, "Received: ");
  clientlen = sizeof(echoclient);

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
for(int i = 0; i < 10; i++){
  msg.msg_name = &echoserver;
  sendmsg(sock, &msg, 0);
  msg.msg_name = &echoclient;
  got = recvmsg(sock, &msg, MSG_ERRQUEUE);
  if(got > 0){
   handle_time(&msg);
  }else{
    printf("got < 0\n");
  }

  got = recvmsg(sock, &msg, 0);
  handle_time(&msg);
   
   /*
   got = recvmsg(sock, &msg, 0);
   

    buffer[received] = '\0';        
    fprintf(stdout, "\n");
    */
}
  close(sock);
  exit(0);
}