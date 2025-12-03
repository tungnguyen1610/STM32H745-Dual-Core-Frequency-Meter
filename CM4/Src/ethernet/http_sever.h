#ifndef HTTP_SEVER_H
#define HTTP_SEVER_H
#include "lwip/api.h"
void http_sever_init();
void lwip_ready_callback(void *arg);
#endif