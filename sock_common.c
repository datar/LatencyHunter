/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include "headers.h"

#include "sock_common.h"

int print_timespec(struct timespec *ts){
	time_t nowtime = ts->tv_sec;
	struct tm *nowtm = localtime(&nowtime);
	char tmbuf[64];
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
	DEBUG(tmbuf);
	return 0;
}

static void usage(const char *error)
{
	if (error)
		DEBUG("invalid option: %s\n", error);
	DEBUG("timestamping interface option*\n\n"
	       "Options:\n"
	       "  IP_MULTICAST_LOOP - looping outgoing multicasts\n"
	       "  SO_TIMESTAMP - normal software time stamping, ms resolution\n"
	       "  SO_TIMESTAMPNS - more accurate software time stamping\n"
	       "  SOF_TIMESTAMPING_TX_HARDWARE - hardware time stamping of outgoing packets\n"
	       "  SOF_TIMESTAMPING_TX_SOFTWARE - software fallback for outgoing packets\n"
	       "  SOF_TIMESTAMPING_RX_HARDWARE - hardware time stamping of incoming packets\n"
	       "  SOF_TIMESTAMPING_RX_SOFTWARE - software fallback for incoming packets\n"
	       "  SOF_TIMESTAMPING_SOFTWARE - request reporting of software time stamps\n"
	       "  SOF_TIMESTAMPING_RAW_HARDWARE - request reporting of raw HW time stamps\n"
	       "  SIOCGSTAMP - check last socket time stamp\n"
	       "  SIOCGSTAMPNS - more accurate socket time stamp\n");
	exit(1);
}

static void bail(const char *error)
{
	DEBUG("%s: %s\n", error, strerror(errno));
	exit(1);
}

static void parse_options( int argc, char** argv, struct configuration* cfg )
{
  int option_index = 0;
  int opt;
  static struct option long_options[] = {
    { "proto", required_argument, 0, 't' },
    { "host", required_argument, 0, 'l' },
    { "interface", required_argument, 0, 'i' },
    { "port", required_argument, 0, 'p' },
    { "max", required_argument, 0, 'n' },
    { "help", no_argument, 0, 'h' },
    { 0, no_argument, 0, 0 }
  };
  char const* optstring = "tlipnh";

  /* Defaults */
  bzero(cfg, sizeof(struct configuration));
  cfg->protocol = IPPROTO_UDP;
  cfg->port = 9000;

  opt = getopt_long(argc, argv, optstring, long_options, &option_index);
  while( opt != -1 ) {
    switch( opt ) {
      case 't':
        cfg->protocol = get_protocol(optarg);
        break;
      case 'l':
        cfg->host = optarg;
        break;
      case 'i':
        cfg->interface = optarg;
        break;
      case 'p':
        cfg->port = atoi(optarg);
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