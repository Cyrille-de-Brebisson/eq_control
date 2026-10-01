#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_NUS";

static uint8_t ble_addr_type;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t nus_tx_val_handle;

/* 128-bit UUIDs for Nordic UART Service (Little-Endian format for NimBLE) */
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_nus_svc_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
// RX Char (Write): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_nus_rx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
// TX Char (Notify): 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_nus_tx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static int nus_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_cb(struct ble_gap_event *event, void *arg);   // FIX: forward declaration

/* GATT Service Table */
static const struct ble_gatt_svc_def gatt_svcs[] = 
{
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX Characteristic (Receives data from client)
                .uuid = &gatt_nus_rx_uuid.u,
                .access_cb = nus_gatt_handler,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // TX Characteristic (Sends notifications to client)
                .uuid = &gatt_nus_tx_uuid.u,
                .access_cb = nus_gatt_handler,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &nus_tx_val_handle,
            },
            { 0 } // No more characteristics
        },
    },
    { 0 } // No more services
};

static StreamBufferHandle_t xStreamBuffer = NULL;

/* GATT Callback: Handles RX writes */
static int nus_gatt_handler(uint16_t conn_h, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) 
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    char rx_buf[512];
    if (len>=sizeof(rx_buf)) len= sizeof(rx_buf);
    os_mbuf_copydata(ctxt->om, 0, len, rx_buf);
    //rx_buf[len]=0; Serial.flush("BTin "); Serial.flush(rx_buf); Serial.flush("\n");
    xStreamBufferSend(xStreamBuffer, rx_buf, len, 0);
    return 0;
}

/* Helper function to transmit data over BLE TX */
void ble_nus_send_data(const char *data, uint16_t len) 
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    // FIX: a notification carries at most (ATT MTU - 3) bytes; split longer strings
    uint16_t mtu = ble_att_mtu(conn_handle);
    uint16_t chunk = (mtu > 3) ? (mtu - 3) : 20;

    uint16_t off = 0;
    while (off < len)
    {
        uint16_t n = len - off;
        if (n > chunk) n = chunk;
        int rc = BLE_HS_ENOMEM;
        for (int tries = 0; tries < 20; tries++)   // retry if the mbuf pool is momentarily empty
        {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data + off, n);
            if (om == NULL) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
            rc = ble_gatts_notify_custom(conn_handle, nus_tx_val_handle, om);   // consumes om
            if (rc != BLE_HS_ENOMEM) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (rc != 0)
        {
            ESP_LOGE(TAG, "notify failed rc=%d", rc);   // FIX: report the result
            return;
        }
        off += n;
    }
    //Serial.flush("BTout "); Serial.flush(data); Serial.flush("\n");
}

/* GAP Advertising Control */
static void start_advertising(void) 
{
    //Serial.flush("start_advertising\n");
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"EQControl";
    fields.name_len = strlen("EQControl");
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields rc=%d", rc); return; }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    // FIX: pass gap_event_cb, otherwise connect/disconnect/subscribe events are never delivered
    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (rc != 0) ESP_LOGE(TAG, "adv_start rc=%d", rc);
}

/* GAP Event Handler */
static int gap_event_cb(struct ble_gap_event *event, void *arg) 
{
    switch (event->type) 
    {
        case BLE_GAP_EVENT_CONNECT:
            //Serial.flush("BLE_GAP_EVENT_CONNECT\n");
            if (event->connect.status == 0) 
            {
                conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Client Connected");
            } 
            else
            {
                conn_handle = BLE_HS_CONN_HANDLE_NONE;
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            //Serial.flush("BLE_GAP_EVENT_DISCONNECT\n");
            ESP_LOGI(TAG, "Client Disconnected");
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe attr=%d notify=%d", event->subscribe.attr_handle, event->subscribe.cur_notify);
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU=%d", event->mtu.value);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;
    }
    return 0;
}

static void on_sync(void) 
{
    int rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (rc != 0) { ESP_LOGE(TAG, "id_infer_auto rc=%d", rc); return; }
    start_advertising();
}

static void host_task(void *param) 
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void BTTask(void*);

class CBLESerial : public CSerial { public:
    static void begin()
    {
        // 1. Initialize NVS (Required for Wi-Fi / Bluetooth)
        //Serial.flush("BT begin\n");
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
        { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init(); }
        ESP_ERROR_CHECK(ret);

        if (xStreamBuffer == NULL) xStreamBuffer = xStreamBufferCreate(512, 1);

        // 2. Initialize NimBLE stack
        ESP_ERROR_CHECK(nimble_port_init());

        // 3. Configure NimBLE callbacks and service table
        ble_hs_cfg.sync_cb = on_sync;
        ble_svc_gap_device_name_set("ESP32C3-NUS");
        ble_svc_gap_init();
        ble_svc_gatt_init();
        
        ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_svcs));
        ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_svcs));

        // 4. Start host task thread
        nimble_port_freertos_init(host_task);

        xTaskCreate(BTTask, "Serial", 2048, NULL, 2, NULL);
        return;
    }
    int16_t read() override
    { 
        char c;
        int len = xStreamBufferReceive(xStreamBuffer, &c, 1, 20 / portTICK_PERIOD_MS);
        if (len!=1) return -1; return c;
    }
    int read(uint8_t *d, int size) override
    { 
        return xStreamBufferReceive(xStreamBuffer, (char*)d, size, 1000 / portTICK_PERIOD_MS);
    }
    void flush(char const *s) override { 
        //Serial.flush("flush "); Serial.flush(s); Serial.flush("\n");
        ble_nus_send_data(s, strlen(s)); }
} BLESerial;

static void BTTask(void*)
{
    TSerialContext serialContext;
    while (true)
    {
        uint8_t d[64]; int l= BLESerial.read(d, sizeof(d)); // blocking...
        if (l>0) processSerial((char*)d, l, serialContext, BLESerial);
    }
}