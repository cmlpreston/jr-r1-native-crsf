// Included inside main.cpp's namespace. Critical producers never format, allocate
// or wait on USB. Only this low-priority consumer calls the diagnostic sink.
enum class DiagnosticKind : uint8_t { Safety, EscTx, ErsTx, ManagementTx, EscBlocked };
struct DiagnosticEvent {
    DiagnosticKind kind=DiagnosticKind::Safety;
    uint32_t at=0,a=0,b=0,c=0,d=0,e=0,f=0;
};
BoundedEventQueue<DiagnosticEvent,32> diagnosticEvents;
portMUX_TYPE diagnosticMux=portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t diagnosticTaskHandle=nullptr;
// Serialize complete machine-readable USB replies/screenshots with this worker.
// This mutex is used only by UI/diagnostic tasks, never critical producers.
class DiagnosticConsoleGuard {
public:
    DiagnosticConsoleGuard(){xSemaphoreTakeRecursive(mutex(),portMAX_DELAY);}
    ~DiagnosticConsoleGuard(){xSemaphoreGiveRecursive(mutex());}
    static SemaphoreHandle_t mutex(){
        static StaticSemaphore_t storage;
        static SemaphoreHandle_t handle=xSemaphoreCreateRecursiveMutexStatic(&storage);
        return handle;
    }
};
void recordDiagnostic(DiagnosticKind kind,uint32_t a=0,uint32_t b=0,uint32_t c=0,
                      uint32_t d=0,uint32_t e=0,uint32_t f=0) {
    DiagnosticEvent event;event.kind=kind;event.at=millis();
    event.a=a;event.b=b;event.c=c;event.d=d;event.e=e;event.f=f;
    portENTER_CRITICAL(&diagnosticMux);
    diagnosticEvents.push(event);
    portEXIT_CRITICAL(&diagnosticMux);
}
void diagnosticTask(void *) {
    uint32_t reportedDrops=0;
    for(;;){
        DiagnosticEvent event;uint32_t dropped;
        portENTER_CRITICAL(&diagnosticMux);
        const bool available=diagnosticEvents.pop(event);dropped=diagnosticEvents.dropped();
        portEXIT_CRITICAL(&diagnosticMux);
        if(!available&&dropped==reportedDrops){vTaskDelay(pdMS_TO_TICKS(10));continue;}
        DiagnosticConsoleGuard console;
        if(dropped!=reportedDrops){
            Serial.printf("DIAGNOSTICS: dropped=%lu (producer queue never waits)\n",(unsigned long)dropped);
            reportedDrops=dropped;
        }
        if(!available)continue;
        switch(event.kind){
        case DiagnosticKind::Safety:
            Serial.printf("SAFETY t=%lums %s -> %s arm_request=%s raw=%u filtered=%u centre=%u delta=%+d\n",
                (unsigned long)event.at,safetyStatusName(OutputSafetyStatus(event.a)),
                safetyStatusName(OutputSafetyStatus(event.b)),event.c?"yes":"no",
                unsigned(event.d),unsigned(event.e),unsigned(event.f),int(event.e)-int(event.f));break;
        case DiagnosticKind::EscTx:
            Serial.printf("ESC RF TX: probe=%u op=%u seq=%u\n",unsigned(event.a),unsigned(event.b),unsigned(event.c));break;
        case DiagnosticKind::ErsTx:
            Serial.printf("ERS TX: type=%u id=%u command=%u bytes=%u\n",unsigned(event.a),unsigned(event.b),unsigned(event.c),unsigned(event.d));break;
        case DiagnosticKind::ManagementTx:
            Serial.printf("ELRS MGMT: sent %s to %s (%u-byte phrase)\n",
                event.a==unsigned(ElrsManagementAction::ReadUid)?"UID request":"bindphrase",
                event.b==unsigned(ElrsManagementTarget::Transmitter)?"TX module":"receiver",unsigned(event.c));break;
        case DiagnosticKind::EscBlocked:
            Serial.printf("ESC RF BLOCKED: stale=%u safety=%s\n",unsigned(event.a),safetyStatusName(OutputSafetyStatus(event.b)));break;
        }
    }
}
