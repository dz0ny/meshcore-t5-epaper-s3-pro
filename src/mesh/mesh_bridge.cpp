#include "mesh_bridge.h"

namespace mesh::bridge {

QueueHandle_t contact_queue = NULL;
QueueHandle_t message_queue = NULL;
QueueHandle_t telemetry_queue = NULL;
QueueHandle_t trace_queue = NULL;
SemaphoreHandle_t status_mutex = NULL;
MeshStatus status = {};
volatile bool discovery_changed = false;

void init() {
    contact_queue = xQueueCreate(128, sizeof(ContactUpdate));
    message_queue = xQueueCreate(32, sizeof(MessageIn));
    telemetry_queue = xQueueCreate(16, sizeof(TelemetryResponse));
    trace_queue = xQueueCreate(16, sizeof(TraceResponse));
    status_mutex = xSemaphoreCreateMutex();
}

void push_contact(const ContactUpdate& c) {
    xQueueSend(contact_queue, &c, 0); // don't block
}

void push_message(const MessageIn& m) {
    xQueueSend(message_queue, &m, 0);
}

void push_telemetry(const TelemetryResponse& t) {
    xQueueSend(telemetry_queue, &t, 0);
}

void push_trace(const TraceResponse& t) {
    xQueueSend(trace_queue, &t, 0);
}

void update_status(const MeshStatus& s) {
    if (xSemaphoreTake(status_mutex, pdMS_TO_TICKS(10))) {
        status = s;
        xSemaphoreGive(status_mutex);
    }
}

bool pop_contact(ContactUpdate& c) {
    return xQueueReceive(contact_queue, &c, 0) == pdTRUE;
}

bool pop_message(MessageIn& m) {
    return xQueueReceive(message_queue, &m, 0) == pdTRUE;
}

bool pop_telemetry(TelemetryResponse& t) {
    return xQueueReceive(telemetry_queue, &t, 0) == pdTRUE;
}

bool pop_trace(TraceResponse& t) {
    return xQueueReceive(trace_queue, &t, 0) == pdTRUE;
}

MeshStatus get_status() {
    MeshStatus s = {};
    if (xSemaphoreTake(status_mutex, pdMS_TO_TICKS(10))) {
        s = status;
        xSemaphoreGive(status_mutex);
    }
    return s;
}

} // namespace mesh::bridge
