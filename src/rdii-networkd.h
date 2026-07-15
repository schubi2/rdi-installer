// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MAX_GATEWAYS 20

// Structure to hold the parsed ip= and ifcfg= configuration
typedef struct {
  const char *client_ip;
  const char *peer_ip;
  const char *gateways[MAX_GATEWAYS];
  int gateways_count;
  const char *destinations[MAX_GATEWAYS];
  int netmask;
  const char *hostname;
  const char *interface;
  const char *autoconf;
  int use_dns; // 0 = unset, 1 = false, 2 = true
  char *dns1;
  char *dns2;
  char *ntp;
  char *mtu;
  char *macaddr;
  char *domains;
  int  vlan1;
  int  vlan2;
  int  vlan3;
} ip_t;

extern int append_route_settings(const char *gateway, const char *destination, ip_t *cfg);
extern int return_syntax_error(int line, const char *value, const int ret);
extern int get_vlan_id(const char *vlan_name, int *ret);
extern int write_network_config(const char *output_dir, int line_num, ip_t *cfg);
