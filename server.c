#include "headers.h"


void Die(char *mess) { perror(mess); exit(1); }


struct configuration {
  int protocol;
  char const* host_ip;
  unsigned int host_port;
  char const* server_ip;
  unsigned int server_port;
  unsigned int msg_size;
  unsigned int max_packets;
};


int usage(){
  fprintf(stderr, "USAGE: client --host <host_ip> --port <host_port> --max_packets <number>\n");
  exit(1);
}

static void parse_options( int argc, char** argv, struct configuration* cfg ){
  int option_index = 0;
  int opt;
  static struct option long_options[] = {
      { "protocol", required_argument, 0, 't' },
      { "host", required_argument, 0, 'i' },
      { "port", required_argument, 0, 'p' },
      { "server_ip", required_argument, 0, 's'},
      { "server_port", required_argument, 0, 'r'},
      { "msg_size", required_argument, 0, 'm'},
      { "max", required_argument, 0, 'n' },
      { "help", no_argument, 0, 'h' },
      { 0, no_argument, 0, 0 }
  };
  char const* optstring = "tipsrmnh";

  /* Defaults */
  memset(cfg, 0, sizeof(struct configuration));
  cfg->protocol = IPPROTO_UDP;
  cfg->host_ip = "192.168.1.2";
  cfg->host_port = 11111;
  cfg->server_ip = "192.168.1.1";
  cfg->server_port = 11111;
  cfg->msg_size = 64;
  cfg->max_packets = 10;

  opt = getopt_long(argc, argv, optstring, long_options, &option_index);
  while( opt != -1 ) {
    switch( opt ) {
    case 't':
      //cfg->protocol = get_protocol(optarg);
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

int set_socket_reuse(int sock){
  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return 0;
}


void make_address(char const* host, unsigned int port, struct sockaddr_in* addr){
  memset(addr, 0, sizeof(struct sockaddr_in));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  if (host != NULL) {
    struct hostent *hPtr = (struct hostent *) gethostbyname(host);
    memcpy((char *)&addr->sin_addr, hPtr->h_addr_list[0], hPtr->h_length);
  } else {
    addr->sin_addr.s_addr=INADDR_ANY;
  }
}

#define TIME_FMT "%" PRIu64 ".%.9" PRIu64 " "
void print_time(char *s, struct timespec *ts){
  printf("" TIME_FMT "\n", (uint64_t)ts->tv_sec, (uint64_t)ts->tv_nsec);
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


void keep_time(struct msghdr* msg, struct timespec* ts){
  struct timespec* udp_tx_stamp;
  struct cmsghdr* cmsg;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level != SOL_SOCKET)
      continue;
    switch(cmsg->cmsg_type) {
    case SO_TIMESTAMPING:
      udp_tx_stamp = (struct timespec*) CMSG_DATA(cmsg);
      memcpy(ts, ((struct timespec*) CMSG_DATA(cmsg))+1, sizeof(struct timespec));
      break;
    default:
      DEBUG("NO_TS\n");
      break;
    }
  }
};


static void do_ts_sockopt(int sock){
  DEBUG("Selecting hardware timestamping mode.\n");
  int enable = SOF_TIMESTAMPING_RX_HARDWARE
      |SOF_TIMESTAMPING_TX_HARDWARE
      |SOF_TIMESTAMPING_SYS_HARDWARE
      |SOF_TIMESTAMPING_RAW_HARDWARE;
  int ok = setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &enable, sizeof(int));
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

  /* Create the UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    Die("Failed to create socket");
  }
  set_socket_reuse(sock);
  int on = 1;
  setsockopt(sock, SOL_IP, IP_PKTINFO, &on, sizeof(on));
  struct sockaddr_in host_addr;
  make_address(config.host_ip, config.host_port, &host_addr);
  if(bind(sock, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0){
    Die("bind");
  }
  do_ts_sockopt(sock);
  
  


  struct sockaddr_in recv_addr;
  make_address(0, 0, &recv_addr);

  char buffer[config.msg_size];
  unsigned int clientlen;
  clientlen = sizeof(host_addr);

  struct msghdr msg;
  struct iovec iov;
  char control[1024];
  int got;

  iov.iov_base = buffer;
  iov.iov_len = config.msg_size;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_control = control;
  msg.msg_controllen = 1024;
  msg.msg_name = &recv_addr;

  size_t result_len = sizeof(struct timespec)*4*config.max_packets;
  struct timespec (*result)[4] = (struct timespec ((*)[4]))malloc(result_len);
  memset(result[0], 0, result_len);

  int i;
  for(i = 0; i < config.max_packets; i++){
    do {
       got = recvmsg(sock, &msg, 0);
    } while (got < 0 && errno == EAGAIN);
    if(got >= 0){
      clock_gettime(CLOCK_REALTIME, result[i]+1);
      keep_time(&msg, result[i]);
    }else{
      DEBUG("Unable to RECEIVE MSG\n");
    }

    clock_gettime(CLOCK_REALTIME, result[i]+2);
    msg.msg_controllen = 0;
    iov.iov_len = got;
    if(0 > sendmsg(sock, &msg, 0)){
      DEBUG("SEND_ERR");
    }
    msg.msg_controllen = 1024;
    do {
      got = recvmsg(sock, &msg, MSG_ERRQUEUE);
    } while (got < 0 && errno == EAGAIN);
    if(got >= 0){
      keep_time(&msg, result[i]+3);
    }else{
      DEBUG("Unable to get TX_TIMESTAMPING\n");
    }

    

  }
  FILE* outfile;
  outfile = fopen("result.txt", "w");
  for(i = 0; i < config.max_packets; i++){
    int j;
    for (j = 0; j < 4; j++){
      if (j < 3){
        fprintf(outfile, "%ld,%9ld,", result[i][j].tv_sec, result[i][j].tv_nsec);
      }else{
        fprintf(outfile, "%ld,%9ld\n", result[i][j].tv_sec, result[i][j].tv_nsec);
      }
    }
    
  }
  close(outfile);

  close(sock);
  exit(0);
}
