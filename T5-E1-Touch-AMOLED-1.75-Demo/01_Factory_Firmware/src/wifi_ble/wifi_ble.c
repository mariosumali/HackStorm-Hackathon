#include "wifi_ble.h"
#include "tal_api.h"

uint8_t WIFI_NUM = 0;
uint8_t BLE_NUM = 0;

void WIFI_Scan()
{
    AP_IF_S *ap_info;
    uint32_t ap_info_nums;

    tal_wifi_all_ap_scan(&ap_info, &ap_info_nums);
    WIFI_NUM = (uint8_t)ap_info_nums;
    PR_DEBUG("Scanf to %d wifi signals", WIFI_NUM);
}

/**
 * @brief bluebooth event callback function
 *
 * @param[in] p_event: bluebooth event
 * @return none
 */
static void __ble_central_event_callback(TAL_BLE_EVT_PARAMS_T *p_event)
{
    PR_DEBUG("----------ble_central event callback-------");
    PR_DEBUG("ble_central event is : %d", p_event->type);
    switch (p_event->type) {
    case TAL_BLE_EVT_ADV_REPORT: {
        int i = 0;

        /*printf peer addr and addr type*/
        PR_DEBUG_RAW("Scanf device peer addr: ");
        for (i = 0; i < 6; i++) {
            PR_DEBUG_RAW("  %d", p_event->ble_event.adv_report.peer_addr.addr[i]);
        }
        PR_DEBUG_RAW(" \r\n");

        if (TAL_BLE_ADDR_TYPE_RANDOM == p_event->ble_event.adv_report.peer_addr.type) {
            PR_DEBUG("Peer addr type is random address");
        } else {
            PR_DEBUG("Peer addr type is public address");
        }

        /*printf ADV type*/
        switch (p_event->ble_event.adv_report.adv_type) {
        case TAL_BLE_ADV_DATA: {
            PR_DEBUG("ADV data only!");
            break;
        }

        case TAL_BLE_RSP_DATA: {
            PR_DEBUG("Scan Response Data only!");
            break;
        }

        case TAL_BLE_ADV_RSP_DATA: {
            PR_DEBUG("ADV data and Scan Response Data!");
            break;
        }
        default:
            break;
        }

        /*printf ADV rssi*/
        PR_DEBUG("Received Signal Strength Indicator : %d", p_event->ble_event.adv_report.rssi);

        /*printf ADV data*/
        PR_DEBUG("Advertise packet data length : %d", p_event->ble_event.adv_report.data_len);
        PR_DEBUG_RAW("Advertise packet data: ");
        for (i = 0; i < p_event->ble_event.adv_report.data_len; i++) {
            PR_DEBUG_RAW("  0x%02X", p_event->ble_event.adv_report.p_data[i]);
        }
        PR_DEBUG_RAW(" \r\n");

        break;
    }
    default:
        break;
    }

    tal_ble_scan_stop();
}

void BLE_Scan(){
    TAL_BLE_SCAN_PARAMS_T scan_cfg;
    memset(&scan_cfg, 0, sizeof(TAL_BLE_SCAN_PARAMS_T));

    /*ble_central init*/
    PR_NOTICE("ble central init start");
    tal_ble_bt_init(TAL_BLE_ROLE_CENTRAL, __ble_central_event_callback);

    /*start scan*/
    scan_cfg.type = TAL_BLE_SCAN_TYPE_ACTIVE;
    scan_cfg.scan_interval = 0x400;
    scan_cfg.scan_window = 0x400;
    scan_cfg.timeout = 0xFFFF;
    scan_cfg.filter_dup = 0;

    tal_ble_scan_start(&scan_cfg);

    PR_NOTICE("ble central init success");
}