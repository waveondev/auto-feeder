#ifndef __TOPIC_LIST_H__
#define __TOPIC_LIST_H__



#include "device_config.h"


#define TOPIC_HEADER "things/"CONFIG_DEVICE_TYPE"/"CONFIG_DEVICE_PREFIX
#define SERVER_RX_TOPIC_REGISTRATION   TOPIC_HEADER "_%s/result/registration"
#define SERVER_RX_TOPIC_BOOT           TOPIC_HEADER "_%s/result/boot"
#define SERVER_RX_TOPIC_ACCESS         TOPIC_HEADER "_%s/result/access"
#define SERVER_RX_TOPIC_INTAKE         TOPIC_HEADER "_%s/result/intake"
#define SERVER_RX_TOPIC_DISPENSE       TOPIC_HEADER "_%s/result/dispense"
#define SERVER_RX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "_%s/result/diagnostics"
#define SERVER_RX_TOPIC_HEALTH         TOPIC_HEADER "_%s/result/health"

#define SERVER_TX_TOPIC_REGISTRATION   TOPIC_HEADER "_%s/registration"
#define SERVER_TX_TOPIC_BOOT           TOPIC_HEADER "_%s/boot"
#define SERVER_TX_TOPIC_ACCESS         TOPIC_HEADER "_%s/access"
#define SERVER_TX_TOPIC_INTAKE         TOPIC_HEADER "_%s/intake"
#define SERVER_TX_TOPIC_DISPENSE       TOPIC_HEADER "_%s/dispense"
#define SERVER_TX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "_%s/diagnostics"
#define SERVER_TX_TOPIC_HEALTH         TOPIC_HEADER "_%s/health"

//#define TRACKER_TOPIC_HEADER "things/"CONFIG_TRACKER_DEVICE_TYPE"/"CONFIG_TRACKER_PREFIX"_"

#define TRACKER_TOPIC_HEADER "things/tracker/TRACKER_"
#define TRACKER_TX_TOPIC_ACTIVITY      TRACKER_TOPIC_HEADER "%s/activity"
#define TRACKER_TX_TOPIC_DIAGNOSTICS   TRACKER_TOPIC_HEADER "%s/diagnostics"

#define TRACKER_TX_TOPIC_HEALTH        TRACKER_TOPIC_HEADER "%s/health"




#define AWS_RX_TOPIC_JOBS_NOTIFY          "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/jobs/notify-next"
#define AWS_RX_TOPIC_JOBS_NEXT_GET_ACCEPTED "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/jobs/$next/get/accepted"
#define AWS_RX_TOPIC_JOBS_GET_ACCEPTED    "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/shadow/get/accepted"
#define AWS_RX_TOPIC_JOBS_UPDATE_ACCEPTED "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/shadow/update/accepted"
#define AWS_RX_TOPIC_SHADOW_DELTA         "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/shadow/update/delta"



// 1. 대기 중인 OTA 조회 요청 (MAC 주소 1개 매칭)
#define AWS_TX_TOPIC_JOBS_GET           "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/jobs/$next/get"

// 2. OTA 상태 업데이트 보고 (MAC 주소 1개 + jobId 1개 매칭)
#define AWS_TX_TOPIC_JOBS_UPDATE        "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/jobs/%s/update"

// 3. Shadow 상태 보고 (MAC 주소 1개 매칭)
#define AWS_TX_TOPIC_SHADOW_UPDATE      "$aws/things/" CONFIG_DEVICE_PREFIX "_%s/shadow/update"


#endif
