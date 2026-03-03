/********************************** (C) COPYRIGHT *******************************
 * File Name          : hidkbd.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2018/12/10
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#ifndef HIDKBD_H
#define HIDKBD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

// Task Events
#define START_DEVICE_EVT          0x0001
#define START_REPORT_EVT          0x0002
#define START_PARAM_UPDATE_EVT    0x0004
#define START_PHY_UPDATE_EVT      0x0008
#define START_BATT_READ_EVT       0x0010
#define HID_SYS_LED_OFF_EVT       0x0100
#define HID_SYS_LED_BLINK_EVT     0x0200
#define HID_BLE_LED_OFF_EVT       0x0400
#define HID_BLE_LED_BLINK_EVT     0x0800
#define HID_USER_KEY_POLL_EVT     0x1000
#define HID_SLEEP_TIMEOUT_EVT     0x2000
#define HID_USB_POLL_EVT          0x4000
#define HID_USB_IDLE_EVT          0x8000

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*
 * Task Initialization for the BLE Application
 */
extern void HidEmu_Init(void);

/*
 * Task Event Processor for the BLE Application
 */
extern uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events);
extern uint8_t HidEmu_ResetIdleTimer(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
