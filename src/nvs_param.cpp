
#include <Preferences.h>
#include "nvs_param.h"

Preferences prefs;

/* clang-format off */
nvs_param nvs_default[NVS_ID_MAX] = {
    {NVS_ID_BACKLIGHT,      NVS_U8,     "blacklight",   .data = {.u8 = 0}},
    {NVS_ID_BRIGHTNESS,     NVS_U8,     "brightness",   .data = {.u8 = 1}},
    {NVS_ID_EPD_VCOM,       NVS_U16,    "eps_vcom",     .data = {.u16 = 1000}},
    {NVS_ID_REFRESH_MODE,   NVS_U8,     "refresh_mode", .data = {.u8 = 0} },
    {NVS_ID_SLEEP_TIMEOUT,  NVS_U8,     "sleep_idx",    .data = {.u8 = 3} },
    {NVS_ID_UI_THEME,       NVS_U8,     "ui_theme",     .data = {.u8 = 0} },
    {NVS_ID_BLE_ENABLED,    NVS_U8,     "ble_on",       .data = {.u8 = 0} },
    {NVS_ID_STATUSBAR_MEMORY, NVS_U8,   "mem_bar",      .data = {.u8 = 0} },
    {NVS_ID_HIT_AREA_DEBUG, NVS_U8,     "hit_debug",    .data = {.u8 = 0} },
};
/* clang-format on */

void nsv_param_init(void)
{
    prefs.begin("system"); // use "system" namespace

    bool tpInit = prefs.isKey("nvsInit"); // Test for the existence of the "already initialized" key.
    // bool tpInit = false;

    /* clang-format off */
    if(tpInit == false)
    {
        Serial.println("nvsInit");
        for(int i = 0; i < NVS_ID_MAX; i++)
        {
            switch (nvs_default[i].type)
            {
            case NVS_I8:     prefs.putChar(nvs_default[i].key, nvs_default[i].data.i8);     break;
            case NVS_U8:     prefs.putUChar(nvs_default[i].key, nvs_default[i].data.u8);    break;
            case NVS_I16:    prefs.putShort(nvs_default[i].key, nvs_default[i].data.i16);   break;
            case NVS_U16:    prefs.putUShort(nvs_default[i].key, nvs_default[i].data.u16);  break;
            case NVS_I32:    prefs.putInt(nvs_default[i].key, nvs_default[i].data.i32);     break;
            case NVS_U32:    prefs.putUInt(nvs_default[i].key, nvs_default[i].data.u32);    break;
            case NVS_I64:    prefs.putLong64(nvs_default[i].key, nvs_default[i].data.i64);  break;
            case NVS_U64:    prefs.putULong64(nvs_default[i].key, nvs_default[i].data.u64); break;
            case NVS_FLOAT:  prefs.putFloat(nvs_default[i].key, nvs_default[i].data.ff);    break;
            case NVS_DOUBLE: prefs.putDouble(nvs_default[i].key, nvs_default[i].data.dd);   break;
            case NVS_BLOB:   prefs.putBool(nvs_default[i].key, nvs_default[i].data.bb);     break;
            case NVS_STR:    prefs.putString(nvs_default[i].key, nvs_default[i].data.str);  break;
            default:
                break;
            }
        }

        prefs.putBool("nvsInit", true); // Create the "already initialized" key and store a value.

        int currentBrightness = prefs.getUChar("blacklight"); //
        int tChannel = prefs.getUShort("eps_vcom");        //  The LHS variables were defined

        Serial.printf("blacklight = %d\n", currentBrightness);
        Serial.printf("eps_vcom = %d\n", tChannel);
    }

    prefs.end();
    /* clang-format on */
}

int nvs_param_id_is_find(NVSDataID id)
{
    for (int i = 0; i < NVS_ID_MAX; i++)
    {
        if (nvs_default[i].id == id)
            return i;
    }
    return -1;
}

/* clang-format off */
void nvs_param_set(NVSDataID id, NVSType type, union nvs_data data)
{
    prefs.begin("system");
    // Serial.printf("[SET] ID=%d, Type=%d, key=%s, data=%d\n", id, type, nvs_default[id].key, data.u8);

    int i = nvs_param_id_is_find(id);
    if(i == -1) return;

    switch (nvs_default[i].type)
    {
    case NVS_I8:     prefs.putChar(nvs_default[i].key, data.i8);     break;
    case NVS_U8:     prefs.putUChar(nvs_default[i].key, data.u8);    break;
    case NVS_I16:    prefs.putShort(nvs_default[i].key, data.i16);   break;
    case NVS_U16:    prefs.putUShort(nvs_default[i].key, data.u16);  break;
    case NVS_I32:    prefs.putInt(nvs_default[i].key, data.i32);     break;
    case NVS_U32:    prefs.putUInt(nvs_default[i].key, data.u32);    break;
    case NVS_I64:    prefs.putLong64(nvs_default[i].key, data.i64);  break;
    case NVS_U64:    prefs.putULong64(nvs_default[i].key, data.u64); break;
    case NVS_FLOAT:  prefs.putFloat(nvs_default[i].key, data.ff);    break;
    case NVS_DOUBLE: prefs.putDouble(nvs_default[i].key, data.dd);   break;
    case NVS_BLOB:   prefs.putBool(nvs_default[i].key, data.bb);     break;
    case NVS_STR:    prefs.putString(nvs_default[i].key, data.str);  break;
    default:
        break;
    }
    prefs.end();
}

union nvs_data nvs_param_get(NVSDataID id, NVSType type)
{
    union nvs_data t = {0};
    prefs.begin("system");

    int i = nvs_param_id_is_find(id);
    if(i == -1) return t;

    switch (nvs_default[i].type)
    {
    case NVS_I8:     t.i8 = prefs.getChar(nvs_default[i].key);     break;
    case NVS_U8:     t.u8 = prefs.getUChar(nvs_default[i].key);    break;
    case NVS_I16:    t.i16 = prefs.getShort(nvs_default[i].key);   break;
    case NVS_U16:    t.u16 = prefs.getUShort(nvs_default[i].key);  break;
    case NVS_I32:    t.i32 = prefs.getInt(nvs_default[i].key);     break;
    case NVS_U32:    t.u32 = prefs.getUInt(nvs_default[i].key);    break;
    case NVS_I64:    t.i64 = prefs.getLong64(nvs_default[i].key);  break;
    case NVS_U64:    t.u64 = prefs.getULong64(nvs_default[i].key); break;
    case NVS_FLOAT:  t.ff = prefs.getFloat(nvs_default[i].key);    break;
    case NVS_DOUBLE: t.dd = prefs.getDouble(nvs_default[i].key);   break;
    case NVS_BLOB:   t.bb = prefs.getBool(nvs_default[i].key);     break;
    case NVS_STR:    t.str = (prefs.getString(nvs_default[i].key)).c_str();  break;
    default:
        break;
    }
    // Serial.printf("[GET] ID=%d, Type=%d, data=%d\n", id, type, t.u8);
    prefs.end();
    return t;
}
/* clang-format on */

#define NVS_PARAM_SET_XX(xx, yy)                    \
    void nvs_param_set_##xx(NVSDataID id, yy data)  \
    {                                               \
        union nvs_data t;                           \
        t.xx = data;                                \
        nvs_param_set(id, nvs_default[id].type, t); \
    }

#define NVS_PARAM_GET_XX(xx, yy)                                    \
    yy nvs_param_get_##xx(NVSDataID id)                             \
    {                                                               \
        union nvs_data t = nvs_param_get(id, nvs_default[id].type); \
        return t.xx;                                                \
    }

// set
NVS_PARAM_SET_XX(i8, int8_t)
NVS_PARAM_SET_XX(u8, uint8_t)
NVS_PARAM_SET_XX(i16, int16_t)
NVS_PARAM_SET_XX(u16, uint16_t)
NVS_PARAM_SET_XX(i32, int32_t)
NVS_PARAM_SET_XX(u32, uint32_t)
NVS_PARAM_SET_XX(i64, int64_t)
NVS_PARAM_SET_XX(u64, uint64_t)
NVS_PARAM_SET_XX(ff, float)
NVS_PARAM_SET_XX(dd, double)
NVS_PARAM_SET_XX(bb, bool)
NVS_PARAM_SET_XX(str, const char *)
// get
NVS_PARAM_GET_XX(i8, int8_t)
NVS_PARAM_GET_XX(u8, uint8_t)
NVS_PARAM_GET_XX(i16, int16_t)
NVS_PARAM_GET_XX(u16, uint16_t)
NVS_PARAM_GET_XX(i32, int32_t)
NVS_PARAM_GET_XX(u32, uint32_t)
NVS_PARAM_GET_XX(i64, int64_t)
NVS_PARAM_GET_XX(u64, uint64_t)
NVS_PARAM_GET_XX(ff, float)
NVS_PARAM_GET_XX(dd, double)
NVS_PARAM_GET_XX(bb, bool)
NVS_PARAM_GET_XX(str, const char *)
