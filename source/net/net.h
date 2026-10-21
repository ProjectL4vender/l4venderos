#ifndef SYSTEMC_NET_H
#define SYSTEMC_NET_H
#include "../kernel/types.h"
void net_init(void);
/* Re-detect the built-in E1000 device and synchronously poll for a DHCP lease. */
int net_install_auto(void);
void net_poll(void);
int net_has_driver(void);
int net_is_online(void);
void net_get_mac(char *out);
void net_get_ipv4(char *out);
void net_get_gateway(char *out);
void net_get_dns(char *out);
int net_http_get(const char *host, const char *path, char *out, u32 capacity);
#endif
