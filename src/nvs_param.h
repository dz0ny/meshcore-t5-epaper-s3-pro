#pragma once

#include <Arduino.h>

typedef enum
{
    NVS_ID_BACKLIGHT = 0,
    NVS_ID_EPD_VCOM,
    NVS_ID_REFRESH_MODE,
    NVS_ID_MAX,
} NVSDataID;

typedef enum
{
    NVS_I8,
    NVS_U8,
    NVS_I16,
    NVS_U16,
    NVS_I32,
    NVS_U32,
    NVS_I64,
    NVS_U64,
    NVS_FLOAT,
    NVS_DOUBLE,
    NVS_STR,
    NVS_BLOB,
    NVS_INVALID
} NVSType;

union nvs_data
{
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float ff;
    double dd;
    bool bb;
    const char *str;
};

typedef struct
{
    NVSDataID id;
    NVSType type;
    const char *key;
    union nvs_data data;
} nvs_param;

void nsv_param_init(void);
// set
void nvs_param_set_i8(NVSDataID id, int8_t data);
void nvs_param_set_u8(NVSDataID id, uint8_t data);
void nvs_param_set_i16(NVSDataID id, int16_t data);
void nvs_param_set_u16(NVSDataID id, uint16_t data);
void nvs_param_set_i32(NVSDataID id, int32_t data);
void nvs_param_set_u32(NVSDataID id, uint32_t data);
void nvs_param_set_i64(NVSDataID id, int64_t data);
void nvs_param_set_u64(NVSDataID id, uint64_t data);
void nvs_param_set_ff(NVSDataID id, float data);
void nvs_param_set_dd(NVSDataID id, double data);
void nvs_param_set_bb(NVSDataID id, bool data);
void nvs_param_set_str(NVSDataID id, const char *data);
// get
int8_t nvs_param_get_i8(NVSDataID id);
uint8_t nvs_param_get_u8(NVSDataID id);
int16_t nvs_param_get_i16(NVSDataID id);
uint16_t nvs_param_get_u16(NVSDataID id);
int32_t nvs_param_get_i32(NVSDataID id);
uint32_t nvs_param_get_u32(NVSDataID id);
int64_t nvs_param_get_i64(NVSDataID id);
uint64_t nvs_param_get_u64(NVSDataID id);
float nvs_param_get_ff(NVSDataID id);
double nvs_param_get_dd(NVSDataID id);
bool nvs_param_get_bb(NVSDataID id);
const char *nvs_param_get_str(NVSDataID id);