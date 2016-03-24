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

