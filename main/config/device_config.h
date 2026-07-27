#ifndef __DEVICE_CONFIG_H__
#define __DEVICE_CONFIG_H__
// 다비아스의 모든 설정을 선언.
// ==========================================
// [1] 디바이스 기본 정보 설정
// ==========================================
#define CONFIG_DEVICE_PREFIX        "W100"               // 시리얼 넘버 접두사
#define CONFIG_DEVICE_TYPE          "w100"               // 디바이스 타입
#define CONFIG_DEVICE_THING_CODE    "WATER"              // Thing 이름에 들어갈 코드
#define CONFIG_HW_REV               "r1.0"               // 하드웨어 리비전
#define CONFIG_FW_VERSION           "v1.0.0"             // 펌웨어 버전

// ==========================================
// [2] MQTT 토픽(Topic) 포맷 설정
// ==========================================
// 공통 패턴: things/<device_type>/<thing_code>_<MAC>/<event_type>[cite: 11]

#define TOPIC_FORMAT_REGISTRATION   "things/%s/%s_%02X%02X%02X%02X%02X%02X/registration"
#define TOPIC_FORMAT_BOOT           "things/%s/%s_%02X%02X%02X%02X%02X%02X/boot"
#define TOPIC_FORMAT_ACCESS         "things/%s/%s_%02X%02X%02X%02X%02X%02X/access"
#define TOPIC_FORMAT_DRINK          "things/%s/%s_%02X%02X%02X%02X%02X%02X/drink"
#define TOPIC_FORMAT_DIAGNOSTICS    "things/%s/%s_%02X%02X%02X%02X%02X%02X/diagnostics"
#define TOPIC_FORMAT_HEALTH         "things/%s/%s_%02X%02X%02X%02X%02X%02X/health"

#endif /* __DEVICE_CONFIG_H__ */