// Read-only fast startup stage. No RF, Wi-Fi, PWM, setting writes or bootloader changes.
#include <Arduino.h>
#include <Preferences.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <rom/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <cstring>
#include "er6g_esc_startup.h"
#include "surpass_protocol.h"
extern "C" {
extern const unsigned char r1_er6g_startup_identity[] __attribute__((used)) =
 "\xBE\xEF\xCA\xFE" "UNIFIED_ESP32_2400_RX\0R1-ER6G-STARTUP-r8\0RadioMaster ER6-G\0";
}
namespace {
constexpr auto Port=UART_NUM_2;constexpr auto Pin=GPIO_NUM_2;
Er6gStartup::Record result;
void received(uint8_t b){++result.rxCount;result.rxTail=(result.rxTail<<8)|b;}
void release(){
 // Input mode already detaches TX in IDF4.4. The ROM matrix helper would
 // re-enable the pad output and prevent reception of the ESC's reply.
 gpio_set_direction(Pin,GPIO_MODE_INPUT);gpio_set_level(Pin,1);
 gpio_set_pull_mode(Pin,GPIO_PULLUP_ONLY);
}
bool initStageUart(){
 release();uart_config_t c={};c.baud_rate=800;c.data_bits=UART_DATA_8_BITS;
 c.parity=UART_PARITY_DISABLE;c.stop_bits=UART_STOP_BITS_1;c.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;c.source_clk=UART_SCLK_APB;
 return uart_param_config(Port,&c)==ESP_OK&&
  uart_set_pin(Port,-1,Pin,-1,-1)==ESP_OK&&uart_driver_install(Port,512,0,0,nullptr,0)==ESP_OK&&
  uart_set_rx_full_threshold(Port,1)==ESP_OK&&uart_set_rx_timeout(Port,2)==ESP_OK;
}
bool readStageByte(uint8_t &b,uint32_t deadline){
 while(int32_t(deadline-millis())>0){if(uart_read_bytes(Port,&b,1,0)==1){received(b);return true;}delay(1);}return false;
}
bool pair(uint8_t first,uint8_t &second,uint32_t timeoutMs){
 const uint32_t deadline=millis()+timeoutMs;uint8_t b=0;bool head=false;
 while(readStageByte(b,deadline)){if(head){second=b;result.readProgress=5;return true;}head=b==first;if(head)result.readProgress=4;}return false;
}
bool waitForQuiet(){
 const uint32_t began=millis();uint32_t quiet=began;int previous=-1;uint8_t discarded;
 while(uint32_t(millis()-began)<500){
  const int level=gpio_get_level(Pin);
  if(level==0||level!=previous)quiet=millis();previous=level;
  while(uart_read_bytes(Port,&discarded,1,0)==1){received(discarded);quiet=millis();}
  if(level==1&&uint32_t(millis()-quiet)>=40)return true;
  delay(1);
 }
 return false;
}
bool send(const uint8_t *data,unsigned length){
 // Only the measured startup response or parameter0 read may leave this stage.
 if(!((length==2&&data[0]==0x1c&&data[1]==0xff)||
      (length==3&&data[0]==0x1c&&data[1]==0&&data[2]==0)))return false;
 gpio_set_level(Pin,1);gpio_set_direction(Pin,GPIO_MODE_INPUT_OUTPUT_OD);
 if(uart_set_pin(Port,Pin,Pin,-1,-1)!=ESP_OK){release();return false;}
 gpio_set_direction(Pin,GPIO_MODE_INPUT_OUTPUT_OD);gpio_set_pull_mode(Pin,GPIO_PULLUP_ONLY);
 gpio_matrix_out(Pin,U2TXD_OUT_IDX,false,false);
 const uint32_t txBegan=micros();
 const int written=uart_write_bytes(Port,data,length);
 while(written==int(length)&&uint32_t(micros()-txBegan)<length*12500+1000)delay(1);
 const bool sent=written==int(length)&&
                 uart_wait_tx_done(Port,pdMS_TO_TICKS(100))==ESP_OK;

 release();if(!sent)return false;
 const uint32_t deadline=millis()+150;uint8_t b;
 for(unsigned i=0;i<length;++i)if(!readStageByte(b,deadline)||b!=data[i])return false;
 return true;
}
bool peerIdentity(const esp_partition_t *p){
 if(!p||p->type!=ESP_PARTITION_TYPE_APP)return false;
 esp_image_metadata_t metadata={};esp_partition_pos_t location={p->address,p->size};
 if(esp_image_verify(ESP_IMAGE_VERIFY,&location,&metadata)!=ESP_OK||metadata.image_len<750000)return false;
 // Retain overlap so a signature straddling a flash read is still found.
 uint8_t buffer[1088];size_t kept=0;bool target=false,version=false;
 const char *a="\xBE\xEF\xCA\xFE" "UNIFIED_ESP32_2400_RX",*b="4.1.0-er6g-esc-mode-r14";
 for(size_t offset=0;offset<p->size;){
  const size_t count=std::min(size_t(1024),size_t(p->size-offset));
  if(esp_partition_read(p,offset,buffer+kept,count)!=ESP_OK)return false;
  const size_t n=kept+count;
  for(size_t i=0;i<n;++i){
   if(i+strlen(a)<=n&&!memcmp(buffer+i,a,strlen(a)))target=true;
   if(i+strlen(b)<=n&&!memcmp(buffer+i,b,strlen(b)))version=true;
  }
  if(target&&version){uint8_t digest[32];return esp_partition_get_sha256(p,digest)==ESP_OK;}
  kept=std::min(size_t(64),n);memmove(buffer,buffer+n-kept,kept);offset+=count;
 }
 return false;
}
}
void setup(){
 const uint32_t start=micros();
 // Consume a one-shot request BEFORE touching the wire. An accidental boot of
 // this bank must never put the ESC into programming mode.
 Preferences intent;bool requested=false;
 if(intent.begin(Er6gStartup::Namespace,false)){
  if(intent.getUInt(Er6gStartup::TicketKey,0)==Er6gStartup::ProgramTicket){
   intent.remove(Er6gStartup::TicketKey);
   requested=intent.getUInt(Er6gStartup::TicketKey,0)==0&&
      intent.putUInt(Er6gStartup::LockKey,Er6gStartup::ProgramTicket)==sizeof(uint32_t);
  }
  intent.end();
 }
 const bool ready=requested&&initStageUart();result.readyUs=micros();
 uint8_t value=255;
 if(!requested)result.result=Er6gStartup::DriveBypass;
 else if(!ready)result.result=Er6gStartup::UartFailed;
 else {
  const uint8_t hello[]={0x1c,0xff};
  // Avoid the final startup byte: require 40ms HIGH/RX quiet before the first reply.
  result.readProgress=1;
  bool accepted=waitForQuiet()&&send(hello,2);uint32_t lastHello=millis();
  if(accepted){
   result.readProgress=2;
   // If we arrived before the ESC, answer its later announcement once as well.
   const uint32_t deadline=millis()+750;uint8_t previous=0,b=0;bool startup=false;
   while(readStageByte(b,deadline)){if(previous==0x1b&&b==0xff){startup=true;break;}previous=b;}
   if(startup){delay(40);accepted=send(hello,2);lastHello=millis();}
  }
  if(accepted){
   while(uint32_t(millis()-lastHello)<1250)delay(1);
   uint8_t read[3];Surpass::request(false,0,0,read);
   const bool sent=send(read,3);if(sent)result.readProgress=3;
   if(sent&&pair(0x1b,value,500)&&Surpass::valid(0,value)){
    result.result=Er6gStartup::ReadVerified;result.value=value;
   }else result.result=Er6gStartup::ReadFailed;
  }else result.result=Er6gStartup::HandshakeFailed;
 }
 release();result.value=value;result.elapsedUs=micros()-start;
 const esp_partition_t *other=esp_ota_get_next_update_partition(esp_ota_get_running_partition());
 if(peerIdentity(other)){
  result.returnAddress=other->address;result.seal();Preferences store;
  const bool saved=store.begin(Er6gStartup::Namespace,false)&&
      store.putBytes(Er6gStartup::Key,&result,sizeof(result))==sizeof(result);
  store.end();
  (void)saved; // Always preserve return-to-full-firmware recovery if NVS storage fails.
  if(esp_ota_set_boot_partition(other)==ESP_OK){delay(20);esp_restart();}
 }
 // A mismatched peer must never be booted. No unsafe partition writes or raw flashing.
}
void loop(){delay(1000);}
