#ifndef __TOPIC_LIST_H__
#define __TOPIC_LIST_H__



#include "device_config.h"


#define TOPIC_HEADER "things/"CONFIG_DEVICE_TYPE"/"CONFIG_DEVICE_PREFIX"_"
#define SERVER_RX_TOPIC_REGISTRATION   TOPIC_HEADER "%s/result/registration"
#define SERVER_RX_TOPIC_BOOT           TOPIC_HEADER "%s/result/boot"
#define SERVER_RX_TOPIC_ACCESS         TOPIC_HEADER "%s/result/access"
#define SERVER_RX_TOPIC_DRINK          TOPIC_HEADER "%s/result/drink"
#define SERVER_RX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "%s/result/diagnostics"
#define SERVER_RX_TOPIC_HEALTH         TOPIC_HEADER "%s/result/health"

#define SERVER_TX_TOPIC_REGISTRATION   TOPIC_HEADER "%s/registration"
#define SERVER_TX_TOPIC_BOOT           TOPIC_HEADER "%s/boot"
#define SERVER_TX_TOPIC_ACCESS         TOPIC_HEADER "%s/access"
#define SERVER_TX_TOPIC_DRINK          TOPIC_HEADER "%s/drink"
#define SERVER_TX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "%s/diagnostics"
#define SERVER_TX_TOPIC_HEALTH         TOPIC_HEADER "%s/health"


#define TRACKER_TOPIC_HEADER "things/tracker/TRACKER_"
#define TRACKER_RX_TOPIC_ACTIVITY      TRACKER_TOPIC_HEADER "%s/activity"
#define TRACKER_RX_TOPIC_DIAGNOSTICS   TRACKER_TOPIC_HEADER "%s/diagnostics"

#define TRACKER_RX_TOPIC_HEALTH        TRACKER_TOPIC_HEADER "%s/health"





#endif
