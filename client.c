#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define BUFFSIZE 255
void Die(char *mess) { perror(mess); exit(1); }

int usage(){
  fprintf(stderr, "USAGE: %s <server_ip> <port> <word>\n", argv[0]);
  exit(1);
}

int set_socket_reuse(int sock){
  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return 0;
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

  /* Construct the server sockaddr_in structure */
  memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
  echoserver.sin_family = AF_INET;                  /* Internet/IP */
  echoserver.sin_addr.s_addr = inet_addr(argv[1]);  /* IP address */
  echoserver.sin_port = htons(atoi(argv[2]));       /* server port */
  

  /* Send the word to the server */
  echolen = strlen(argv[3]);
  if (sendto(sock, argv[3], echolen, 0,
             (struct sockaddr *) &echoserver,
             sizeof(echoserver)) != echolen) {
    Die("Mismatch in number of sent bytes");
  }
  

  /*
  len = recvfrom(fd, msgbuf, UDP_MAX_PAYLOAD_LEN, 0, (struct sockaddr *)dest, (socklen_t*)&destlen);
將其改為recvmsg並且設定一些sockopt去取得interface的相關資訊

setsockopt(fd, SOL_IP, IP_PKTINFO, &on, sizeof(on));
iov.iov_base = (void*)msgbuf;
iov.iov_len = (size_t) UDP_MAX_PAYLOAD_LEN;
msg.msg_name = (struct sockaddr*)dest;
msg.msg_namelen = (socklen_t)destlen;
msg.msg_iov = &iov;
msg.msg_iovlen = 1;
msg.msg_control = cbuf;
msg.msg_controllen = sizeof(cbuf);
*/

  struct sockaddr_in echoclient;
  char buffer[BUFFSIZE];
  unsigned int clientlen;
  /* Receive the word back from the server */
  fprintf(stdout, "Received: ");
  clientlen = sizeof(echoclient);
  if ((received = recvfrom(sock, buffer, BUFFSIZE, 0,
                           (struct sockaddr *) &echoclient,
                           &clientlen)) != echolen) {
    Die("Mismatch in number of received bytes");
  }
  /* Check that client and server are using same socket */
  if (echoserver.sin_addr.s_addr != echoclient.sin_addr.s_addr) {
    Die("Received a packet from an unexpected server");
  }
  buffer[received] = '\0';        /* Assure null terminated string */
  fprintf(stdout, buffer);
  fprintf(stdout, "\n");
  close(sock);
  exit(0);
}