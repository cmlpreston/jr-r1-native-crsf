#pragma once

#ifndef JR_R1_GIT_BRANCH
#define JR_R1_GIT_BRANCH "unknown"
#endif

#ifndef JR_R1_GIT_COMMIT
#define JR_R1_GIT_COMMIT "unknown"
#endif

#ifndef JR_R1_GIT_STATE
#define JR_R1_GIT_STATE "unknown"
#endif

#ifndef JR_R1_BUILD_ENV
#define JR_R1_BUILD_ENV "unknown"
#endif

constexpr const char *JR_R1_FIRMWARE_NAME = "JR R-1 NATIVE CRSF";
constexpr const char *JR_R1_FIRMWARE_BRANCH = JR_R1_GIT_BRANCH;
constexpr const char *JR_R1_FIRMWARE_COMMIT = JR_R1_GIT_COMMIT;
constexpr const char *JR_R1_FIRMWARE_GIT_STATE = JR_R1_GIT_STATE;
constexpr const char *JR_R1_FIRMWARE_BUILD_ENV = JR_R1_BUILD_ENV;
constexpr const char *JR_R1_FIRMWARE_BUILD_DATE = __DATE__;
constexpr const char *JR_R1_FIRMWARE_BUILD_TIME = __TIME__;
