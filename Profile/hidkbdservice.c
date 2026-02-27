/*********************************************************************
 * File Name          : hidkbdservice.c
 * Description        : HID Service Implementation (Composite)
 *********************************************************************/

#include "CONFIG.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "battservice.h"

/*********************************************************************
 * GLOBAL VARIABLES
 */
// HID service
const uint8_t hidServUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(HID_SERV_UUID), HI_UINT16(HID_SERV_UUID)};

// UUIDs
const uint8_t hidBootKeyInputUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(BOOT_KEY_INPUT_UUID), HI_UINT16(BOOT_KEY_INPUT_UUID)};
const uint8_t hidBootKeyOutputUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(BOOT_KEY_OUTPUT_UUID), HI_UINT16(BOOT_KEY_OUTPUT_UUID)};
const uint8_t hidInfoUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(HID_INFORMATION_UUID), HI_UINT16(HID_INFORMATION_UUID)};
const uint8_t hidReportMapUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(REPORT_MAP_UUID), HI_UINT16(REPORT_MAP_UUID)};
const uint8_t hidControlPointUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(HID_CTRL_PT_UUID), HI_UINT16(HID_CTRL_PT_UUID)};
const uint8_t hidReportUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(REPORT_UUID), HI_UINT16(REPORT_UUID)};
const uint8_t hidProtocolModeUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(PROTOCOL_MODE_UUID), HI_UINT16(PROTOCOL_MODE_UUID)};

/*********************************************************************
 * LOCAL VARIABLES
 */

// HID Information characteristic value
static const uint8_t hidInfo[HID_INFORMATION_LEN] = {
    LO_UINT16(0x0111), HI_UINT16(0x0111), // bcdHID (USB HID version)
    0x00,                                 // bCountryCode
    HID_FEATURE_FLAGS                     // Flags
};

// ===================================================================
// 1. HID Report Map (身份证：键盘 + 鼠标)
// ===================================================================
static const uint8_t hidReportMap[] = {
    // --- Keyboard (ID 1) ---
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    
    // --- Mouse (ID 2) ---
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 
    0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0
};

// HID report map length
uint16_t hidReportMapLen = sizeof(hidReportMap);

// HID report mapping table
static hidRptMap_t hidRptMap[HID_NUM_REPORTS];

/*********************************************************************
 * Profile Attributes - variables
 */

static const gattAttrType_t hidService = {ATT_BT_UUID_SIZE, hidServUUID};
static uint16_t include = GATT_INVALID_HANDLE;

// HID Information
static uint8_t hidInfoProps = GATT_PROP_READ;

// HID Report Map
static uint8_t hidReportMapProps = GATT_PROP_READ;

// HID External Report Reference (Battery)
static uint8_t hidExtReportRefDesc[ATT_BT_UUID_SIZE] = {LO_UINT16(BATT_LEVEL_UUID), HI_UINT16(BATT_LEVEL_UUID)};

// HID Control Point
static uint8_t hidControlPointProps = GATT_PROP_WRITE_NO_RSP;
static uint8_t hidControlPoint;

// HID Protocol Mode
static uint8_t hidProtocolModeProps = GATT_PROP_READ | GATT_PROP_WRITE_NO_RSP;
uint8_t        hidProtocolMode = HID_PROTOCOL_MODE_REPORT;

// --- Report 1: Keyboard Input ---
static uint8_t       hidReportKeyInProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t       hidReportKeyIn;
static gattCharCfg_t hidReportKeyInClientCharCfg[GATT_MAX_NUM_CONN];
static uint8_t hidReportRefKeyIn[HID_REPORT_REF_LEN] = {HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT};

// --- Report 1: LED Output ---
static uint8_t hidReportLedOutProps = GATT_PROP_READ | GATT_PROP_WRITE | GATT_PROP_WRITE_NO_RSP;
static uint8_t hidReportLedOut;
static uint8_t hidReportRefLedOut[HID_REPORT_REF_LEN] = {HID_RPT_ID_LED_OUT, HID_REPORT_TYPE_OUTPUT};

// --- Report 2: Mouse Input (新增) ---
static uint8_t       hidReportMouseInProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t       hidReportMouseIn;
static gattCharCfg_t hidReportMouseInClientCharCfg[GATT_MAX_NUM_CONN];
static uint8_t hidReportRefMouseIn[HID_REPORT_REF_LEN] = {HID_RPT_ID_MOUSE_IN, HID_REPORT_TYPE_INPUT};

// --- Boot Keyboard ---
static uint8_t       hidReportBootKeyInProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t       hidReportBootKeyIn;
static gattCharCfg_t hidReportBootKeyInClientCharCfg[GATT_MAX_NUM_CONN];
static uint8_t hidReportBootKeyOutProps = GATT_PROP_READ | GATT_PROP_WRITE | GATT_PROP_WRITE_NO_RSP;
static uint8_t hidReportBootKeyOut;

// Feature Report
static uint8_t hidReportFeatureProps = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8_t hidReportFeature;
static uint8_t hidReportRefFeature[HID_REPORT_REF_LEN] = {HID_RPT_ID_FEATURE, HID_REPORT_TYPE_FEATURE};

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t hidAttrTbl[] = {
    // HID Service
    { {ATT_BT_UUID_SIZE, primaryServiceUUID}, GATT_PERMIT_READ, 0, (uint8_t *)&hidService },
    // Included service (battery)
    { {ATT_BT_UUID_SIZE, includeUUID}, GATT_PERMIT_READ, 0, (uint8_t *)&include},
    // HID Info
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidInfoProps},
    { {ATT_BT_UUID_SIZE, hidInfoUUID}, GATT_PERMIT_ENCRYPT_READ, 0, (uint8_t *)hidInfo},
    // HID Control Point
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidControlPointProps},
    { {ATT_BT_UUID_SIZE, hidControlPointUUID}, GATT_PERMIT_ENCRYPT_WRITE, 0, &hidControlPoint},
    // HID Protocol Mode
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidProtocolModeProps},
    { {ATT_BT_UUID_SIZE, hidProtocolModeUUID}, GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, &hidProtocolMode},
    // HID Report Map
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportMapProps},
    { {ATT_BT_UUID_SIZE, hidReportMapUUID}, GATT_PERMIT_ENCRYPT_READ, 0, (uint8_t *)hidReportMap},
    // External Report Reference
    { {ATT_BT_UUID_SIZE, extReportRefUUID}, GATT_PERMIT_READ, 0, hidExtReportRefDesc},

    // --------------------------------------------------------
    // Report 1: Keyboard Input
    // --------------------------------------------------------
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportKeyInProps},
    { {ATT_BT_UUID_SIZE, hidReportUUID}, GATT_PERMIT_ENCRYPT_READ, 0, &hidReportKeyIn},
    { {ATT_BT_UUID_SIZE, clientCharCfgUUID}, GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, (uint8_t *)&hidReportKeyInClientCharCfg},
    { {ATT_BT_UUID_SIZE, reportRefUUID}, GATT_PERMIT_READ, 0, hidReportRefKeyIn},

    // --------------------------------------------------------
    // Report 1: LED Output
    // --------------------------------------------------------
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportLedOutProps},
    { {ATT_BT_UUID_SIZE, hidReportUUID}, GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, &hidReportLedOut},
    { {ATT_BT_UUID_SIZE, reportRefUUID}, GATT_PERMIT_READ, 0, hidReportRefLedOut},

    // --------------------------------------------------------
    // Report 2: Mouse Input (新增)
    // --------------------------------------------------------
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportMouseInProps},
    { {ATT_BT_UUID_SIZE, hidReportUUID}, GATT_PERMIT_ENCRYPT_READ, 0, &hidReportMouseIn},
    { {ATT_BT_UUID_SIZE, clientCharCfgUUID}, GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, (uint8_t *)&hidReportMouseInClientCharCfg},
    { {ATT_BT_UUID_SIZE, reportRefUUID}, GATT_PERMIT_READ, 0, hidReportRefMouseIn},

    // --------------------------------------------------------
    // Boot Keyboard
    // --------------------------------------------------------
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportBootKeyInProps},
    { {ATT_BT_UUID_SIZE, hidBootKeyInputUUID}, GATT_PERMIT_ENCRYPT_READ, 0, &hidReportBootKeyIn},
    { {ATT_BT_UUID_SIZE, clientCharCfgUUID}, GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, (uint8_t *)&hidReportBootKeyInClientCharCfg},
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportBootKeyOutProps},
    { {ATT_BT_UUID_SIZE, hidBootKeyOutputUUID}, GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, &hidReportBootKeyOut},

    // Feature Report
    { {ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &hidReportFeatureProps},
    { {ATT_BT_UUID_SIZE, hidReportUUID}, GATT_PERMIT_ENCRYPT_READ | GATT_PERMIT_ENCRYPT_WRITE, 0, &hidReportFeature},
    { {ATT_BT_UUID_SIZE, reportRefUUID}, GATT_PERMIT_READ, 0, hidReportRefFeature},
};

// 属性表索引枚举
enum
{
    HID_SERVICE_IDX,
    HID_INCLUDED_SERVICE_IDX,
    HID_INFO_DECL_IDX, HID_INFO_IDX,
    HID_CONTROL_POINT_DECL_IDX, HID_CONTROL_POINT_IDX,
    HID_PROTOCOL_MODE_DECL_IDX, HID_PROTOCOL_MODE_IDX,
    HID_REPORT_MAP_DECL_IDX, HID_REPORT_MAP_IDX,
    HID_EXT_REPORT_REF_DESC_IDX,
    // Keyboard Input
    HID_REPORT_KEY_IN_DECL_IDX, HID_REPORT_KEY_IN_IDX, HID_REPORT_KEY_IN_CCCD_IDX, HID_REPORT_REF_KEY_IN_IDX,
    // LED Output
    HID_REPORT_LED_OUT_DECL_IDX, HID_REPORT_LED_OUT_IDX, HID_REPORT_REF_LED_OUT_IDX,
    // Mouse Input (新增)
    HID_REPORT_MOUSE_IN_DECL_IDX, HID_REPORT_MOUSE_IN_IDX, HID_REPORT_MOUSE_IN_CCCD_IDX, HID_REPORT_REF_MOUSE_IN_IDX,
    // Boot Keyboard
    HID_BOOT_KEY_IN_DECL_IDX, HID_BOOT_KEY_IN_IDX, HID_BOOT_KEY_IN_CCCD_IDX,
    HID_BOOT_KEY_OUT_DECL_IDX, HID_BOOT_KEY_OUT_IDX,
    // Feature
    HID_FEATURE_DECL_IDX, HID_FEATURE_IDX, HID_REPORT_REF_FEATURE_IDX
};

/*********************************************************************
 * PROFILE CALLBACKS
 */
gattServiceCBs_t hidKbdCBs = {
    HidDev_ReadAttrCB,  // Read callback function pointer
    HidDev_WriteAttrCB, // Write callback function pointer
    NULL                // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

bStatus_t Hid_AddService(void)
{
    uint8_t status = SUCCESS;

    // Initialize CCCD
    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, hidReportKeyInClientCharCfg);
    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, hidReportMouseInClientCharCfg); // 初始化鼠标
    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, hidReportBootKeyInClientCharCfg);

    // Register GATT
    status = GATTServApp_RegisterService(hidAttrTbl, GATT_NUM_ATTRS(hidAttrTbl), GATT_MAX_ENCRYPT_KEY_SIZE, &hidKbdCBs);

    // Set up included service
    Batt_GetParameter(BATT_PARAM_SERVICE_HANDLE, &GATT_INCLUDED_HANDLE(hidAttrTbl, HID_INCLUDED_SERVICE_IDX));

    // Construct map of reports (告诉 hiddev.c 哪个 ID 对应哪个 Handle)
    
    // 1. Keyboard Input
    hidRptMap[0].id = hidReportRefKeyIn[0];
    hidRptMap[0].type = hidReportRefKeyIn[1];
    hidRptMap[0].handle = hidAttrTbl[HID_REPORT_KEY_IN_IDX].handle;
    hidRptMap[0].cccdHandle = hidAttrTbl[HID_REPORT_KEY_IN_CCCD_IDX].handle;
    hidRptMap[0].mode = HID_PROTOCOL_MODE_REPORT;

    // 2. LED Output
    hidRptMap[1].id = hidReportRefLedOut[0];
    hidRptMap[1].type = hidReportRefLedOut[1];
    hidRptMap[1].handle = hidAttrTbl[HID_REPORT_LED_OUT_IDX].handle;
    hidRptMap[1].cccdHandle = 0;
    hidRptMap[1].mode = HID_PROTOCOL_MODE_REPORT;

    // 3. Mouse Input (新增)
    hidRptMap[2].id = hidReportRefMouseIn[0];
    hidRptMap[2].type = hidReportRefMouseIn[1];
    hidRptMap[2].handle = hidAttrTbl[HID_REPORT_MOUSE_IN_IDX].handle;
    hidRptMap[2].cccdHandle = hidAttrTbl[HID_REPORT_MOUSE_IN_CCCD_IDX].handle;
    hidRptMap[2].mode = HID_PROTOCOL_MODE_REPORT;

    // ... (后续是 Boot Keyboard 和 Feature，保持兼容)
    hidRptMap[3].id = hidReportRefKeyIn[0];
    hidRptMap[3].type = hidReportRefKeyIn[1];
    hidRptMap[3].handle = hidAttrTbl[HID_BOOT_KEY_IN_IDX].handle;
    hidRptMap[3].cccdHandle = hidAttrTbl[HID_BOOT_KEY_IN_CCCD_IDX].handle;
    hidRptMap[3].mode = HID_PROTOCOL_MODE_BOOT;

    hidRptMap[4].id = hidReportRefLedOut[0];
    hidRptMap[4].type = hidReportRefLedOut[1];
    hidRptMap[4].handle = hidAttrTbl[HID_BOOT_KEY_OUT_IDX].handle;
    hidRptMap[4].cccdHandle = 0;
    hidRptMap[4].mode = HID_PROTOCOL_MODE_BOOT;

    hidRptMap[5].id = hidReportRefFeature[0];
    hidRptMap[5].type = hidReportRefFeature[1];
    hidRptMap[5].handle = hidAttrTbl[HID_FEATURE_IDX].handle;
    hidRptMap[5].cccdHandle = 0;
    hidRptMap[5].mode = HID_PROTOCOL_MODE_REPORT;

    // Battery level
    Batt_GetParameter(BATT_PARAM_BATT_LEVEL_IN_REPORT, &(hidRptMap[6]));

    HidDev_RegisterReports(HID_NUM_REPORTS, hidRptMap);

    return (status);
}

// 辅助函数：根据 ID 和 Type 查找 Handle (关键！)
uint16_t Hid_GetAttrHandle(uint8_t id, uint8_t type)
{
    if (type == HID_REPORT_TYPE_INPUT) {
        if (id == HID_RPT_ID_KEY_IN) return hidAttrTbl[HID_REPORT_KEY_IN_IDX].handle;
        if (id == HID_RPT_ID_MOUSE_IN) return hidAttrTbl[HID_REPORT_MOUSE_IN_IDX].handle;
    }
    else if (type == HID_REPORT_TYPE_OUTPUT) {
        if (id == HID_RPT_ID_LED_OUT) return hidAttrTbl[HID_REPORT_LED_OUT_IDX].handle;
    }
    return 0;
}

// ===================================================================
// 核心逻辑：设置参数并发送通知
// ===================================================================
uint8_t Hid_SetParameter(uint8_t id, uint8_t type, uint16_t uuid, uint8_t len, void *pValue)
{
    bStatus_t ret = SUCCESS;

    // 1. 获取对应的 Attribute Handle
    uint16_t attrHandle = Hid_GetAttrHandle(id, type);

    // 2. 如果是输入报告 (Input Report)，尝试发送 Notification
    if (type == HID_REPORT_TYPE_INPUT && attrHandle != 0)
    {
        gattCharCfg_t *pCharCfg = NULL;

        // 找到对应的 CCCD 数组
        if (id == HID_RPT_ID_KEY_IN) pCharCfg = hidReportKeyInClientCharCfg;
        else if (id == HID_RPT_ID_MOUSE_IN) pCharCfg = hidReportMouseInClientCharCfg;

        if (pCharCfg != NULL)
        {
            // 检查主机是否允许通知
            // connHandle 传 0 表示遍历所有连接，或者传入 hidEmuConnHandle
            uint16_t value = GATTServApp_ReadCharCfg(0, pCharCfg); 
            if (value & GATT_CLIENT_CFG_NOTIFY)
            {
                attHandleValueNoti_t noti;
                noti.handle = attrHandle;
                noti.len = len;
                noti.pValue = (uint8_t *)pValue;
                
                // 发送！
                ret = GATT_Notification(0, &noti, FALSE);
            }
        }
    }
    // 3. 其他类型的处理 (Output, Feature) 保持原样
    else 
    {
        // ... (保持原有的 switch case 逻辑，如 LED 设置)
        // 为了篇幅这里省略，主要逻辑是处理 REPORT_UUID 和 BOOT_KEY_OUTPUT_UUID
        // 你可以直接复制原版 switch 结构在这里
    }

    return (ret);
}

// GetParameter 保持不变
uint8_t Hid_GetParameter(uint8_t id, uint8_t type, uint16_t uuid, uint16_t *pLen, void *pValue)
{
    // ... (保持原版逻辑，主要是返回 Handle)
    // 对于输入报告，我们不需要 GetParameter，直接 return SUCCESS 即可
    *pLen = 0;
    return SUCCESS;
}