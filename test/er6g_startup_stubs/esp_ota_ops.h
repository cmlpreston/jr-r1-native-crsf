#pragma once
#include "esp_partition.h"
struct esp_image_metadata_t{uint32_t image_len;};
struct esp_partition_pos_t{uint32_t offset,size;};
constexpr int ESP_IMAGE_VERIFY=0;
inline int esp_image_verify(int mode,const esp_partition_pos_t*p,esp_image_metadata_t*m){assert(mode==0&&p->offset==peerPart.address&&p->size==peerPart.size);m->image_len=tooSmall?200000:1200000;return 0;}
inline const esp_partition_t*esp_ota_get_running_partition(){return &runningPart;}
inline const esp_partition_t*esp_ota_get_next_update_partition(const esp_partition_t*p){assert(p==&runningPart);return returnRunning?&runningPart:&peerPart;}
inline int esp_ota_set_boot_partition(const esp_partition_t*p){assert(p==&peerPart||p==&runningPart);++selects;return failSelect?-1:0;}
