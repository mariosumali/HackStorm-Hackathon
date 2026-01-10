

#ifndef _WIFI_BLE_H
#define _WIFI_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tal_wifi.h"
#include "tal_bluetooth.h"

/***********************************************************
************************macro define************************
***********************************************************/
extern uint8_t WIFI_NUM;
extern uint8_t BLE_NUM;

void WIFI_Scan();
void BLE_Scan();


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
