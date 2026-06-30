#!/bin/bash

set -e

# Sets up a network proxy.
# The supported proxy URL format is protocol://[user[:password]@]host[:port].
#
# Kernel cmdline arguments
#
# proxy=protocol://[user[:password]@]host[:port]
#
# Example:
# proxy=http://192.168.122.1:3128

RDII_CONFIG_FILE="/run/rdi-installer/rdii-config"

cmdline=$(cat /proc/cmdline)

# Initialize variables
declare -A proxies

if [[ -f "$RDII_CONFIG_FILE" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" == proxy=* ]]; then
            proxy_url="${line#proxy=}"

            if [[ "$proxy_url" == *://* ]]; then
                protocol="${proxy_url%%://*}"
                protocol_upper="${protocol^^}"
                proxies["${protocol_upper}_PROXY"]="$proxy_url"
            fi
        fi
    done < "$RDII_CONFIG_FILE"
fi

for arg in $cmdline; do
    if [[ "$arg" == proxy=* ]]; then
        proxy_url="${arg#proxy=}"

        if [[ "$proxy_url" == *://* ]]; then
            protocol="${proxy_url%%://*}"
            protocol_upper="${protocol^^}"

            # Store in array. If HTTP_PROXY already exists from the
            # config file, this seamlessly overwrites it.
            proxies["${protocol_upper}_PROXY"]="$proxy_url"
        fi
    fi
done

SYSCONFIG_PROXY="/etc/sysconfig/proxy"
if [[ ${#proxies[@]} -gt 0 ]]; then
    if [ -f "$SYSCONFIG_PROXY" ]; then
	for key in "${!proxies[@]}"; do
	    if grep -q "^${key}=" "$SYSCONFIG_PROXY"; then
		sed -i -e "s|^${key}=.*|${key}=\"${proxies[$key]}\"|g" "$SYSCONFIG_PROXY"
	    else
		echo "${key}=\"${proxies[$key]}\"" >> "$SYSCONFIG_PROXY"
	    fi
	done
	sed -i -e 's|PROXY_ENABLED=.*|PROXY_ENABLED="yes"|g' "$SYSCONFIG_PROXY"
    else
	echo 'PROXY_ENABLED="yes"' > "$SYSCONFIG_PROXY"
	for key in "${!proxies[@]}"; do
	    echo "${key}=\"${proxies[$key]}\"" >> "$SYSCONFIG_PROXY"
	done
    fi
fi
