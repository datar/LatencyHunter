/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   sock_common.h
 * Author: Max
 *
 * Created on February 24, 2016, 4:57 PM
 */

#ifndef SOCK_COMMON_H
#define SOCK_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif




#ifdef __cplusplus
}
#endif

struct configuration {
  int            protocol;  /* protocol: udp or tcp */
  char const*    host;      /* listen address */
  char const*    interface;     /* e.g. eth6  - calls the ts enable ioctl */
  unsigned short port;      /* listen port */
  unsigned int   max_packets; /* Stop after this many (0=forever) */
};

int print_timespec(struct timespec *ts);
static void usage(const char *error);
static void bail(const char *error);
static void parse_options( int argc, char** argv, struct configuration* cfg );

#endif /* SOCK_COMMON_H */

