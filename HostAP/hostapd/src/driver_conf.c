/* THIS FILE AUTOMATICALLY GENERATED, DO NOT EDIT! */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "hostapd.h"
#include "driver.h"
void hostap_driver_register(void);
void madwifi_driver_register(void);
void register_drivers(void) {
hostap_driver_register();
madwifi_driver_register();
}
