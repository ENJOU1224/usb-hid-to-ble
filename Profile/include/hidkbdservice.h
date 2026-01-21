/*********************************************************************
 * File Name          : hidkbdservice.h
 * Description        : HID Service Header (Keyboard + Mouse)
 *********************************************************************/

#ifndef HIDKBDSERVICE_H
#define HIDKBDSERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * CONSTANTS
 */

// Number of HID reports defined in the service
// Keyboard In, Mouse In, LED Out, Boot In, Boot Out, Feature
#define HID_NUM_REPORTS        6

// HID Report IDs for the service
// 注意：复合设备必须使用非0的 ID
#define HID_RPT_ID_KEY_IN      1                      // Keyboard input report ID
#define HID_RPT_ID_MOUSE_IN    2                      // Mouse input report ID
#define HID_RPT_ID_LED_OUT     1                      // LED output report ID
#define HID_RPT_ID_FEATURE     0                      // Feature report ID (暂未用到)

// HID feature flags
#define HID_FEATURE_FLAGS      HID_FLAGS_REMOTE_WAKE

/*********************************************************************
 * API FUNCTIONS
 */
extern bStatus_t Hid_AddService(void);
extern uint8_t Hid_SetParameter(uint8_t id, uint8_t type, uint16_t uuid, uint8_t len, void *pValue);
extern uint8_t Hid_GetParameter(uint8_t id, uint8_t type, uint16_t uuid, uint16_t *pLen, void *pValue);

// 辅助函数：获取属性句柄 (供内部使用)
uint16_t Hid_GetAttrHandle(uint8_t id, uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* HIDKBDSERVICE_H */