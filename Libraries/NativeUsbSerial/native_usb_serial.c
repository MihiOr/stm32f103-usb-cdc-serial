/*
 * NativeUsbSerial: STM32F103 USB CDC integration for STM32CubeF1 1.8.7.
 * Board adaptations and print API live here, outside CubeMX-owned files.
 * HAL endpoint handling and USB glue/core portions are derived from
 * STMicroelectronics code, Copyright (c) 2015-2026 STMicroelectronics.
 * All rights reserved. See LICENSE.txt and Package_license.html for ST component licensing.
 * This mixed ST-derived implementation is excluded from the root BSD grant.
 * See docs/LICENSING.md for the file-specific licensing scope.
 */
#include "native_usb_serial.h"
#include "stm32f1xx_hal.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_ioreq.h"
#include <string.h>
#include <stdio.h>

#if !defined(STM32F103xB)
#error "This module is validated for STM32F103xB (Blue Pill), not USB OTG MCUs."
#endif
#define APP_RX_DATA_SIZE 1024
#define APP_TX_DATA_SIZE 1024
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;


/* --- USB board glue --- */
/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_cdc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

PCD_HandleTypeDef hpcd_USB_FS;
void Error_Handler(void);

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status);

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state);
#else
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

/* MSP Init */

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
  if(pcdHandle->Instance==USB)
  {

    /* Peripheral clock enable */
    __HAL_RCC_USB_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  
    /* Power up the transceiver while holding USB reset. RM0008 requires
       tSTARTUP before FRES is released; allow margin for Blue Pill clones. */
    USB->CNTR = USB_CNTR_FRES;
    HAL_Delay(1);
    USB->ISTR = 0;
  
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{
  if(pcdHandle->Instance==USB)
  {

    /* Peripheral clock disable */
    __HAL_RCC_USB_CLK_DISABLE();

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);

  }
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  /* SETUP starts a fresh control transfer, including after an unsupported
     descriptor request. Restore both directions on compatible USB cells
     which retain the preceding transfer's STALL/status bits. */
  PCD_SET_EP_TX_STATUS(hpcd->Instance, 0U, USB_EP_TX_NAK);
  PCD_SET_EP_RX_CNT(hpcd->Instance, 0U, USB_MAX_EP0_SIZE);
  PCD_SET_EP_RX_STATUS(hpcd->Instance, 0U, USB_EP_RX_VALID);
  hpcd->IN_ep[0].is_stall = 0U;
  hpcd->OUT_ep[0].is_stall = 0U;
  USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t *)hpcd->Setup);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#else
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#else
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  if (epnum == 0U)
  {
    /* Do not leave the acknowledged status ZLP available to a new SETUP. */
    PCD_SET_EP_TX_STATUS(hpcd->Instance, 0U, USB_EP_TX_NAK);
  }
  USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  if ( hpcd->Init.speed != PCD_SPEED_FULL)
  {
    Error_Handler();
  }
    /* Set Speed. */
  USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);

  /* Reset Device. */
  USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  /* Inform USB library that core enters in suspend Mode. */
  USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
  /* Enter in STOP mode. */
  
  if (hpcd->Init.low_power_enable)
  {
    /* Set SLEEPDEEP bit and SleepOnExit of Cortex System Control Register. */
    SCB->SCR |= (uint32_t)((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
  }
  
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{

  USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#else
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#else
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData);
}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
#else
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  /* Init USB Ip. */
  /* Link the driver to the stack. */
  hpcd_USB_FS.pData = pdev;
  pdev->pData = &hpcd_USB_FS;

  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler( );
  }

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
  /* Register USB PCD CallBacks */
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_SOF_CB_ID, PCD_SOFCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_SETUPSTAGE_CB_ID, PCD_SetupStageCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_RESET_CB_ID, PCD_ResetCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_SUSPEND_CB_ID, PCD_SuspendCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_RESUME_CB_ID, PCD_ResumeCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_CONNECT_CB_ID, PCD_ConnectCallback);
  HAL_PCD_RegisterCallback(&hpcd_USB_FS, HAL_PCD_DISCONNECT_CB_ID, PCD_DisconnectCallback);

  HAL_PCD_RegisterDataOutStageCallback(&hpcd_USB_FS, PCD_DataOutStageCallback);
  HAL_PCD_RegisterDataInStageCallback(&hpcd_USB_FS, PCD_DataInStageCallback);
  HAL_PCD_RegisterIsoOutIncpltCallback(&hpcd_USB_FS, PCD_ISOOUTIncompleteCallback);
  HAL_PCD_RegisterIsoInIncpltCallback(&hpcd_USB_FS, PCD_ISOINIncompleteCallback);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
  
  /* Reserve all eight hardware BTABLE entries (8 x 8 bytes). Some F103
     compatible parts use the full table even when only EP0..EP2 are active. */
  HAL_PCDEx_PMAConfig((PCD_HandleTypeDef*)pdev->pData , 0x00 , PCD_SNG_BUF, 0x40);
  HAL_PCDEx_PMAConfig((PCD_HandleTypeDef*)pdev->pData , 0x80 , PCD_SNG_BUF, 0x80);

  HAL_PCDEx_PMAConfig((PCD_HandleTypeDef*)pdev->pData , 0x81 , PCD_SNG_BUF, 0xC0);
  HAL_PCDEx_PMAConfig((PCD_HandleTypeDef*)pdev->pData , CDC_OUT_EP , PCD_SNG_BUF, 0x100);
  HAL_PCDEx_PMAConfig((PCD_HandleTypeDef*)pdev->pData , 0x82 , PCD_SNG_BUF, 0x140);
  
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_DeInit(pdev->pData);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  /* A pending reset IRQ must not preempt HAL_PCD_Start while its handle is
     locked: the reset callback opens EP0 and sets the address using that lock. */
  HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  hal_status = HAL_PCD_Start(pdev->pData);
  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_Stop(pdev->pData);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Close(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Flush(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_SetStall(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef*) pdev->pData;

  if((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
  }
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_SetAddress(pdev->pData, dev_addr);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  USBD_StatusTypeDef usb_status = USBD_OK;

  hal_status = HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);

  usb_status =  USBD_Get_USB_Status(hal_status);

  return usb_status;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef*) pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

void *USBD_static_malloc(uint32_t size)
{
  static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef)/4)+1];/* On 32-bit boundary */
  return mem;
}

void USBD_static_free(void *p)
{

}

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
static void PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state)
#else
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state)
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
{
  
  if (state == 1)
  {
    /* Configure Low connection state. */

  }
  else
  {
    /* Configure High connection state. */

  }
  
}

USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status)
{
  USBD_StatusTypeDef usb_status = USBD_OK;

  switch (hal_status)
  {
    case HAL_OK :
      usb_status = USBD_OK;
    break;
    case HAL_ERROR :
      usb_status = USBD_FAIL;
    break;
    case HAL_BUSY :
      usb_status = USBD_BUSY;
    break;
    case HAL_TIMEOUT :
      usb_status = USBD_FAIL;
    break;
    default :
      usb_status = USBD_FAIL;
    break;
  }
  return usb_status;
}

/* --- Initialization --- */
/* Includes ------------------------------------------------------------------*/


#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"




/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* USB Device Core handle declaration. */
USBD_HandleTypeDef hUsbDeviceFS;

/*
 * -- Insert your variables declaration here --
 */

/*
 * -- Insert your external function declaration here --
 */

void nativeUsbInit(void)
{
  
  // Force the host to re-enumerate the USB device
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Drive PA12 (USB_DP) LOW to simulate a disconnection
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  HAL_Delay(20); // Keep it pulled low briefly
  
  // De-initialize the pin so the USB peripheral can control it normally
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);

  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    Error_Handler();
  }

}

void MX_USB_DEVICE_Init(void) { nativeUsbInit(); }

/* --- CDC application interface --- */
/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* Default 115200 baud, one stop bit, no parity, eight data bits. This is a
   virtual port: line coding does not change the USB transfer rate. */
static uint8_t lineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
#define RX_QUEUE_SIZE 2048U
static uint8_t rxQueue[RX_QUEUE_SIZE];
static volatile uint32_t rxHead;
static volatile uint32_t rxTail;
static volatile uint8_t rxPaused;

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

/* Private functions ---------------------------------------------------------*/

static int8_t CDC_Init_FS(void)
{
  
  rxHead = rxTail = 0;
  rxPaused = 0;
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
  
}

static int8_t CDC_DeInit_FS(void)
{
  
  rxHead = rxTail = 0;
  rxPaused = 0;
  return (USBD_OK);
  
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  
    case CDC_SET_LINE_CODING:
      if (length != sizeof(lineCoding)) return USBD_FAIL;
      memcpy(lineCoding, pbuf, sizeof(lineCoding));
    break;

    case CDC_GET_LINE_CODING:
      if (length != sizeof(lineCoding)) return USBD_FAIL;
      memcpy(pbuf, lineCoding, sizeof(lineCoding));
    break;

    case CDC_SET_CONTROL_LINE_STATE:

    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  
  if (*Len > CDC_DATA_FS_OUT_PACKET_SIZE ||
      *Len > RX_QUEUE_SIZE - (rxHead - rxTail))
  {
    rxPaused = 1;
    return USBD_FAIL;
  }
  /* Copy before rearming; the USB packet buffer is reused by the next OUT. */
  for (uint32_t i = 0; i < *Len; ++i)
  {
    rxQueue[rxHead % RX_QUEUE_SIZE] = Buf[i];
    ++rxHead;
  }
  if (RX_QUEUE_SIZE - (rxHead - rxTail) >= CDC_DATA_FS_OUT_PACKET_SIZE)
  {
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  }
  else
  {
    rxPaused = 1;
  }
  return (USBD_OK);
  
}

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || hcdc == NULL ||
      Len > APP_TX_DATA_SIZE || (Buf == NULL && Len != 0U))
  {
    __set_PRIMASK(primask);
    return USBD_FAIL;
  }
  if (hcdc->TxState != 0){
    __set_PRIMASK(primask);
    return USBD_BUSY;
  }
  /* Own the buffer until the asynchronous IN transfer (including ZLP) ends. */
  if (Len != 0U) memcpy(UserTxBufferFS, Buf, Len);
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  __set_PRIMASK(primask);
  
  return result;
}

void CDC_Process_FS(void)
{
  /* Protect the check/copy/rearm sequence against USB reset or disconnect. */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (rxPaused && hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
      RX_QUEUE_SIZE - (rxHead - rxTail) >= CDC_DATA_FS_OUT_PACKET_SIZE)
  {
    rxPaused = 0;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  }
  __set_PRIMASK(primask);
}
uint16_t CDC_Read_FS(uint8_t *buffer, uint16_t capacity)
{
  if (buffer == NULL) return 0;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t length = (uint16_t)MIN((uint32_t)capacity, rxHead - rxTail);
  for (uint16_t i = 0; i < length; ++i)
    buffer[i] = rxQueue[rxTail++ % RX_QUEUE_SIZE];
  __set_PRIMASK(primask);
  return length;
}

/* --- Endpoint interrupt compatibility --- */
#if (USE_USB_DOUBLE_BUFFER == 1U)
static uint16_t nativeDoubleReceive(PCD_HandleTypeDef *, PCD_EPTypeDef *, uint16_t);
static HAL_StatusTypeDef nativeDoubleTransmit(PCD_HandleTypeDef *, PCD_EPTypeDef *, uint16_t);
#endif
static HAL_StatusTypeDef nativeEndpointIrq(PCD_HandleTypeDef *hpcd)
{
  PCD_EPTypeDef *ep;
  uint16_t count;
  uint16_t wIstr;
  uint16_t wEPVal;
  uint16_t TxPctSize;
  uint8_t epindex;

#if (USE_USB_DOUBLE_BUFFER != 1U)
  count = 0U;
#endif /* USE_USB_DOUBLE_BUFFER */

  /* stay in loop while pending interrupts */
  while ((hpcd->Instance->ISTR & USB_ISTR_CTR) != 0U)
  {
    wIstr = hpcd->Instance->ISTR;

    /* extract highest priority endpoint number */
    epindex = (uint8_t)(wIstr & USB_ISTR_EP_ID);

    if (epindex == 0U)
    {
      /* Decode and service control endpoint interrupt */

      /* Use endpoint flags: on this F103-compatible board ISTR.DIR can
         remain OUT when only CTR_TX is pending, trapping the original HAL
         in this loop. Retain RX/SETUP priority if both flags are pending. */
      if ((PCD_GET_ENDPOINT(hpcd->Instance, PCD_ENDP0) & USB_EP_CTR_RX) == 0U)
      {
        /* Only IN completion remains pending. */
        PCD_CLEAR_TX_EP_CTR(hpcd->Instance, PCD_ENDP0);
        ep = &hpcd->IN_ep[0];

        ep->xfer_count = PCD_GET_EP_TX_CNT(hpcd->Instance, ep->num);
        ep->xfer_buff += ep->xfer_count;

        /* TX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
        hpcd->DataInStageCallback(hpcd, 0U);
#else
        HAL_PCD_DataInStageCallback(hpcd, 0U);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

        if ((hpcd->USB_Address > 0U) && (ep->xfer_len == 0U))
        {
          hpcd->Instance->DADDR = ((uint16_t)hpcd->USB_Address | USB_DADDR_EF);
          hpcd->USB_Address = 0U;
        }
      }
      else
      {
        /* RX completion: SETUP or OUT; TX may also be pending. */
        ep = &hpcd->OUT_ep[0];
        wEPVal = PCD_GET_ENDPOINT(hpcd->Instance, PCD_ENDP0);

        if ((wEPVal & USB_EP_SETUP) != 0U)
        {
          /* Clear a previous control STALL/status packet while SETUP is
             still frozen by CTR_RX. Once CTR_RX is cleared this USB cell
             can answer IN immediately, before the stack prepares a reply. */
          PCD_SET_EP_TX_STATUS(hpcd->Instance, PCD_ENDP0, USB_EP_TX_NAK);
          PCD_SET_EP_RX_STATUS(hpcd->Instance, PCD_ENDP0, USB_EP_RX_NAK);
          /* Get SETUP Packet */
          ep->xfer_count = PCD_GET_EP_RX_CNT(hpcd->Instance, ep->num);

          USB_ReadPMA(hpcd->Instance, (uint8_t *)hpcd->Setup,
                      ep->pmaadress, (uint16_t)ep->xfer_count);

          /* SETUP bit kept frozen while CTR_RX = 1 */
          PCD_CLEAR_RX_EP_CTR(hpcd->Instance, PCD_ENDP0);

          /* Process SETUP Packet*/
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
          hpcd->SetupStageCallback(hpcd);
#else
          HAL_PCD_SetupStageCallback(hpcd);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
        }
        else if ((wEPVal & USB_EP_CTR_RX) != 0U)
        {
          PCD_CLEAR_RX_EP_CTR(hpcd->Instance, PCD_ENDP0);

          /* Get Control Data OUT Packet */
          ep->xfer_count = PCD_GET_EP_RX_CNT(hpcd->Instance, ep->num);

          if ((ep->xfer_count != 0U) && (ep->xfer_buff != 0U))
          {
            USB_ReadPMA(hpcd->Instance, ep->xfer_buff,
                        ep->pmaadress, (uint16_t)ep->xfer_count);

            ep->xfer_buff += ep->xfer_count;

            /* Process Control Data OUT Packet */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
            hpcd->DataOutStageCallback(hpcd, 0U);
#else
            HAL_PCD_DataOutStageCallback(hpcd, 0U);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
          }

          wEPVal = (uint16_t)PCD_GET_ENDPOINT(hpcd->Instance, PCD_ENDP0);

          if (((wEPVal & USB_EP_SETUP) == 0U) && ((wEPVal & USB_EP_RX_STRX) != USB_EP_RX_VALID))
          {
            PCD_SET_EP_RX_CNT(hpcd->Instance, PCD_ENDP0, ep->maxpacket);
            PCD_SET_EP_RX_STATUS(hpcd->Instance, PCD_ENDP0, USB_EP_RX_VALID);
          }
        }
      }
    }
    else
    {
      /* Decode and service non control endpoints interrupt */
      /* process related endpoint register */
      wEPVal = PCD_GET_ENDPOINT(hpcd->Instance, epindex);

      if ((wEPVal & USB_EP_CTR_RX) != 0U)
      {
        /* clear int flag */
        PCD_CLEAR_RX_EP_CTR(hpcd->Instance, epindex);
        ep = &hpcd->OUT_ep[epindex];

        /* OUT Single Buffering */
        if (ep->doublebuffer == 0U)
        {
          count = (uint16_t)PCD_GET_EP_RX_CNT(hpcd->Instance, ep->num);

          /* This board can report COUNT_RX=0x3ff for consecutive OUT packets.
             Never copy beyond the allocated PMA packet or application buffer.
             Drop the malformed packet and leave reception usable. This guard
             prevents memory corruption; it does not recover the lost data. */
          if (count > ep->maxpacket)
          {
            PCD_SET_EP_RX_CNT(hpcd->Instance, ep->num, ep->maxpacket);
            PCD_SET_EP_RX_STATUS(hpcd->Instance, ep->num, USB_EP_RX_VALID);
            continue;
          }

          if (count != 0U)
          {
            USB_ReadPMA(hpcd->Instance, ep->xfer_buff, ep->pmaadress, count);
          }
        }
#if (USE_USB_DOUBLE_BUFFER == 1U)
        else
        {
          /* manage double buffer bulk out */
          if (ep->type == EP_TYPE_BULK)
          {
            count = nativeDoubleReceive(hpcd, ep, wEPVal);
          }
          else /* manage double buffer iso out */
          {
            /* free EP OUT Buffer */
            PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 0U);

            if ((PCD_GET_ENDPOINT(hpcd->Instance, ep->num) & USB_EP_DTOG_RX) != 0U)
            {
              /* read from endpoint BUF0Addr buffer */
              count = (uint16_t)PCD_GET_EP_DBUF0_CNT(hpcd->Instance, ep->num);

              if (count != 0U)
              {
                USB_ReadPMA(hpcd->Instance, ep->xfer_buff, ep->pmaaddr0, count);
              }
            }
            else
            {
              /* read from endpoint BUF1Addr buffer */
              count = (uint16_t)PCD_GET_EP_DBUF1_CNT(hpcd->Instance, ep->num);

              if (count != 0U)
              {
                USB_ReadPMA(hpcd->Instance, ep->xfer_buff, ep->pmaaddr1, count);
              }
            }
          }
        }
#endif /* (USE_USB_DOUBLE_BUFFER == 1U) */

        /* multi-packet on the NON control OUT endpoint */
        ep->xfer_count += count;
        ep->xfer_buff += count;

        if ((ep->xfer_len == 0U) || (count < ep->maxpacket))
        {
          /* RX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
          hpcd->DataOutStageCallback(hpcd, ep->num);
#else
          HAL_PCD_DataOutStageCallback(hpcd, ep->num);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
        }
        else
        {
          (void)USB_EPStartXfer(hpcd->Instance, ep);
        }
      }

      if ((wEPVal & USB_EP_CTR_TX) != 0U)
      {
        ep = &hpcd->IN_ep[epindex];

        /* clear int flag */
        PCD_CLEAR_TX_EP_CTR(hpcd->Instance, epindex);

        if (ep->type == EP_TYPE_ISOC)
        {
          ep->xfer_len = 0U;

#if (USE_USB_DOUBLE_BUFFER == 1U)
          if (ep->doublebuffer != 0U)
          {
            if ((wEPVal & USB_EP_DTOG_TX) != 0U)
            {
              PCD_SET_EP_DBUF0_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);
            }
            else
            {
              PCD_SET_EP_DBUF1_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);
            }
          }
#endif /* (USE_USB_DOUBLE_BUFFER == 1U) */

          /* TX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
          hpcd->DataInStageCallback(hpcd, ep->num);
#else
          HAL_PCD_DataInStageCallback(hpcd, ep->num);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
        }
        else
        {
          /* Manage Single Buffer Transaction */
          if ((wEPVal & USB_EP_KIND) == 0U)
          {
            /* multi-packet on the NON control IN endpoint */
            TxPctSize = (uint16_t)PCD_GET_EP_TX_CNT(hpcd->Instance, ep->num);

            if (ep->xfer_len > TxPctSize)
            {
              ep->xfer_len -= TxPctSize;
            }
            else
            {
              ep->xfer_len = 0U;
            }

            /* Zero Length Packet? */
            if (ep->xfer_len == 0U)
            {
              /* TX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
              hpcd->DataInStageCallback(hpcd, ep->num);
#else
              HAL_PCD_DataInStageCallback(hpcd, ep->num);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */
            }
            else
            {
              /* Transfer is not yet Done */
              ep->xfer_buff += TxPctSize;
              ep->xfer_count += TxPctSize;
              (void)USB_EPStartXfer(hpcd->Instance, ep);
            }
          }
#if (USE_USB_DOUBLE_BUFFER == 1U)
          /* Double Buffer bulk IN (bulk transfer Len > Ep_Mps) */
          else
          {
            (void)nativeDoubleTransmit(hpcd, ep, wEPVal);
          }
#endif /* (USE_USB_DOUBLE_BUFFER == 1U) */
        }
      }
    }
  }

  return HAL_OK;
}

#if (USE_USB_DOUBLE_BUFFER == 1U)

static uint16_t nativeDoubleReceive(PCD_HandleTypeDef *hpcd,
                                      PCD_EPTypeDef *ep, uint16_t wEPVal)
{
  uint16_t count;

  /* Manage Buffer0 OUT */
  if ((wEPVal & USB_EP_DTOG_RX) != 0U)
  {
    /* Get count of received Data on buffer0 */
    count = (uint16_t)PCD_GET_EP_DBUF0_CNT(hpcd->Instance, ep->num);

    if (ep->xfer_len >= count)
    {
      ep->xfer_len -= count;
    }
    else
    {
      ep->xfer_len = 0U;
    }

    if (ep->xfer_len == 0U)
    {
      /* set NAK to OUT endpoint since double buffer is enabled */
      PCD_SET_EP_RX_STATUS(hpcd->Instance, ep->num, USB_EP_RX_NAK);
    }

    /* Check if Buffer1 is in blocked state which requires to toggle */
    if ((wEPVal & USB_EP_DTOG_TX) != 0U)
    {
      PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 0U);
    }

    if (count != 0U)
    {
      USB_ReadPMA(hpcd->Instance, ep->xfer_buff, ep->pmaaddr0, count);
    }
  }
  /* Manage Buffer 1 DTOG_RX=0 */
  else
  {
    /* Get count of received data */
    count = (uint16_t)PCD_GET_EP_DBUF1_CNT(hpcd->Instance, ep->num);

    if (ep->xfer_len >= count)
    {
      ep->xfer_len -= count;
    }
    else
    {
      ep->xfer_len = 0U;
    }

    if (ep->xfer_len == 0U)
    {
      /* set NAK on the current endpoint */
      PCD_SET_EP_RX_STATUS(hpcd->Instance, ep->num, USB_EP_RX_NAK);
    }

    /*Need to FreeUser Buffer*/
    if ((wEPVal & USB_EP_DTOG_TX) == 0U)
    {
      PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 0U);
    }

    if (count != 0U)
    {
      USB_ReadPMA(hpcd->Instance, ep->xfer_buff, ep->pmaaddr1, count);
    }
  }

  return count;
}

static HAL_StatusTypeDef nativeDoubleTransmit(PCD_HandleTypeDef *hpcd,
                                                PCD_EPTypeDef *ep, uint16_t wEPVal)
{
  uint32_t len;
  uint16_t TxPctSize;

  /* Data Buffer0 ACK received */
  if ((wEPVal & USB_EP_DTOG_TX) != 0U)
  {
    /* multi-packet on the NON control IN endpoint */
    TxPctSize = (uint16_t)PCD_GET_EP_DBUF0_CNT(hpcd->Instance, ep->num);

    if (ep->xfer_len > TxPctSize)
    {
      ep->xfer_len -= TxPctSize;
    }
    else
    {
      ep->xfer_len = 0U;
    }

    /* Transfer is completed */
    if (ep->xfer_len == 0U)
    {
      PCD_SET_EP_DBUF0_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);
      PCD_SET_EP_DBUF1_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);

      /* TX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
      hpcd->DataInStageCallback(hpcd, ep->num);
#else
      HAL_PCD_DataInStageCallback(hpcd, ep->num);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

      if ((wEPVal & USB_EP_DTOG_RX) != 0U)
      {
        PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 1U);
      }
    }
    else /* Transfer is not yet Done */
    {
      /* need to Free USB Buff */
      if ((wEPVal & USB_EP_DTOG_RX) != 0U)
      {
        PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 1U);
      }

      /* Still there is data to Fill in the next Buffer */
      if (ep->xfer_fill_db == 1U)
      {
        ep->xfer_buff += TxPctSize;
        ep->xfer_count += TxPctSize;

        /* Calculate the len of the new buffer to fill */
        if (ep->xfer_len_db >= ep->maxpacket)
        {
          len = ep->maxpacket;
          ep->xfer_len_db -= len;
        }
        else if (ep->xfer_len_db == 0U)
        {
          len = TxPctSize;
          ep->xfer_fill_db = 0U;
        }
        else
        {
          ep->xfer_fill_db = 0U;
          len = ep->xfer_len_db;
          ep->xfer_len_db = 0U;
        }

        /* Write remaining Data to Buffer */
        /* Set the Double buffer counter for pma buffer1 */
        PCD_SET_EP_DBUF0_CNT(hpcd->Instance, ep->num, ep->is_in, len);

        /* Copy user buffer to USB PMA */
        USB_WritePMA(hpcd->Instance, ep->xfer_buff,  ep->pmaaddr0, (uint16_t)len);
      }
    }
  }
  else /* Data Buffer1 ACK received */
  {
    /* multi-packet on the NON control IN endpoint */
    TxPctSize = (uint16_t)PCD_GET_EP_DBUF1_CNT(hpcd->Instance, ep->num);

    if (ep->xfer_len >= TxPctSize)
    {
      ep->xfer_len -= TxPctSize;
    }
    else
    {
      ep->xfer_len = 0U;
    }

    /* Transfer is completed */
    if (ep->xfer_len == 0U)
    {
      PCD_SET_EP_DBUF0_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);
      PCD_SET_EP_DBUF1_CNT(hpcd->Instance, ep->num, ep->is_in, 0U);

      /* TX COMPLETE */
#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
      hpcd->DataInStageCallback(hpcd, ep->num);
#else
      HAL_PCD_DataInStageCallback(hpcd, ep->num);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

      /* need to Free USB Buff */
      if ((wEPVal & USB_EP_DTOG_RX) == 0U)
      {
        PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 1U);
      }
    }
    else /* Transfer is not yet Done */
    {
      /* need to Free USB Buff */
      if ((wEPVal & USB_EP_DTOG_RX) == 0U)
      {
        PCD_FREE_USER_BUFFER(hpcd->Instance, ep->num, 1U);
      }

      /* Still there is data to Fill in the next Buffer */
      if (ep->xfer_fill_db == 1U)
      {
        ep->xfer_buff += TxPctSize;
        ep->xfer_count += TxPctSize;

        /* Calculate the len of the new buffer to fill */
        if (ep->xfer_len_db >= ep->maxpacket)
        {
          len = ep->maxpacket;
          ep->xfer_len_db -= len;
        }
        else if (ep->xfer_len_db == 0U)
        {
          len = TxPctSize;
          ep->xfer_fill_db = 0U;
        }
        else
        {
          len = ep->xfer_len_db;
          ep->xfer_len_db = 0U;
          ep->xfer_fill_db = 0;
        }

        /* Set the Double buffer counter for pmabuffer1 */
        PCD_SET_EP_DBUF1_CNT(hpcd->Instance, ep->num, ep->is_in, len);

        /* Copy the user buffer to USB PMA */
        USB_WritePMA(hpcd->Instance, ep->xfer_buff,  ep->pmaaddr1, (uint16_t)len);
      }
    }
  }

  /*enable endpoint IN*/
  PCD_SET_EP_TX_STATUS(hpcd->Instance, ep->num, USB_EP_TX_VALID);

  return HAL_OK;
}
#endif /* (USE_USB_DOUBLE_BUFFER == 1U) */

/* GNU ld selects this F103 handler without modifying the vendor HAL. */
void __wrap_HAL_PCD_IRQHandler(PCD_HandleTypeDef *hpcd)
{
  uint32_t wIstr = USB_ReadInterrupts(hpcd->Instance);
  uint16_t store_ep[8];
  uint8_t i;

  if ((wIstr & USB_ISTR_CTR) == USB_ISTR_CTR)
  {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    (void)nativeEndpointIrq(hpcd);

    return;
  }

  if ((wIstr & USB_ISTR_RESET) == USB_ISTR_RESET)
  {
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_RESET);

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
    hpcd->ResetCallback(hpcd);
#else
    HAL_PCD_ResetCallback(hpcd);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

    (void)HAL_PCD_SetAddress(hpcd, 0U);

    return;
  }

  if ((wIstr & USB_ISTR_PMAOVR) == USB_ISTR_PMAOVR)
  {
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_PMAOVR);

    return;
  }

  if ((wIstr & USB_ISTR_ERR) == USB_ISTR_ERR)
  {
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_ERR);

    return;
  }

  if ((wIstr & USB_ISTR_WKUP) == USB_ISTR_WKUP)
  {
    hpcd->Instance->CNTR &= (uint16_t) ~(USB_CNTR_LP_MODE);
    hpcd->Instance->CNTR &= (uint16_t) ~(USB_CNTR_FSUSP);

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
    hpcd->ResumeCallback(hpcd);
#else
    HAL_PCD_ResumeCallback(hpcd);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_WKUP);

    return;
  }

  if ((wIstr & USB_ISTR_SUSP) == USB_ISTR_SUSP)
  {
    /* WA: To Clear Wakeup flag if raised with suspend signal */

    /* Store Endpoint registers */
    for (i = 0U; i < 8U; i++)
    {
      store_ep[i] = PCD_GET_ENDPOINT(hpcd->Instance, i);
    }

    /* FORCE RESET */
    hpcd->Instance->CNTR |= (uint16_t)(USB_CNTR_FRES);

    /* CLEAR RESET */
    hpcd->Instance->CNTR &= (uint16_t)(~USB_CNTR_FRES);

    /* wait for reset flag in ISTR */
    while ((hpcd->Instance->ISTR & USB_ISTR_RESET) == 0U)
    {
    }

    /* Clear Reset Flag */
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_RESET);

    /* Restore Registre */
    for (i = 0U; i < 8U; i++)
    {
      PCD_SET_ENDPOINT(hpcd->Instance, i, store_ep[i]);
    }

    /* Force low-power mode in the macrocell */
    hpcd->Instance->CNTR |= (uint16_t)USB_CNTR_FSUSP;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_SUSP);

    hpcd->Instance->CNTR |= (uint16_t)USB_CNTR_LP_MODE;

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
    hpcd->SuspendCallback(hpcd);
#else
    HAL_PCD_SuspendCallback(hpcd);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

    return;
  }

  if ((wIstr & USB_ISTR_SOF) == USB_ISTR_SOF)
  {
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_SOF);

#if (USE_HAL_PCD_REGISTER_CALLBACKS == 1U)
    hpcd->SOFCallback(hpcd);
#else
    HAL_PCD_SOFCallback(hpcd);
#endif /* USE_HAL_PCD_REGISTER_CALLBACKS */

    return;
  }

  if ((wIstr & USB_ISTR_ESOF) == USB_ISTR_ESOF)
  {
    /* clear ESOF flag in ISTR */
    __HAL_PCD_CLEAR_FLAG(hpcd, USB_ISTR_ESOF);

    return;
  }
}

/* --- EP0 IN completion compatibility --- */
USBD_StatusTypeDef __wrap_USBD_LL_DataInStage(USBD_HandleTypeDef *pdev,
                                       uint8_t epnum, uint8_t *pdata)
{
  USBD_EndpointTypeDef *pep;

  if (epnum == 0U)
  {
    pep = &pdev->ep_in[0];

    if (pdev->ep0_state == USBD_EP0_DATA_IN)
    {
      if (pep->rem_length > pep->maxpacket)
      {
        pep->rem_length -= pep->maxpacket;

        USBD_CtlContinueSendData(pdev, pdata, (uint16_t)pep->rem_length);

        /* Prepare endpoint for premature end of transfer */
        USBD_LL_PrepareReceive(pdev, 0U, NULL, 0U);
      }
      else
      {
        /* last packet is MPS multiple, so send ZLP packet */
        if ((pep->total_length % pep->maxpacket == 0U) &&
            (pep->total_length >= pep->maxpacket) &&
            (pep->total_length < pdev->ep0_data_len))
        {
          USBD_CtlContinueSendData(pdev, NULL, 0U);
          pdev->ep0_data_len = 0U;

          /* Prepare endpoint for premature end of transfer */
          USBD_LL_PrepareReceive(pdev, 0U, NULL, 0U);
        }
        else
        {
          if ((pdev->pClass->EP0_TxSent != NULL) &&
              (pdev->dev_state == USBD_STATE_CONFIGURED))
          {
            pdev->pClass->EP0_TxSent(pdev);
          }
          /* Keep EP0 IN NAKed after success. This board can answer the next
             control request with a stale STALL before software handles SETUP.
             Invalid requests still use USBD_CtlError to signal STALL. */
          USBD_CtlReceiveStatus(pdev);
        }
      }
    }
    /* STATUS_IN/IDLE also leave the completed IN endpoint NAKed. The
       board's DataIn callback sets NAK before this function is called. */

    if (pdev->dev_test_mode == 1U)
    {
      USBD_RunTestMode(pdev);
      pdev->dev_test_mode = 0U;
    }
  }
  else if ((pdev->pClass->DataIn != NULL) &&
           (pdev->dev_state == USBD_STATE_CONFIGURED))
  {
    pdev->pClass->DataIn(pdev, epnum);
  }
  else
  {
    /* should never be in this condition */
    return USBD_FAIL;
  }

  return USBD_OK;
}

/* --- Nonblocking serial API --- */


#define txCapacity 1024U
static uint8_t txQueue[txCapacity];
static uint32_t txHead = 0, txTail = 0;
static uint32_t droppedBytes = 0;
static uint8_t txLineOpen = 0;

void serialClearTxBuffer(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    droppedBytes += txHead - txTail;
    txHead = txTail;
    /* Keep the active USB transfer untouched; resetting TxState while an IN
       packet is pending would allow its buffer to be overwritten. */
    if (txLineOpen) {
        txQueue[txHead++ % txCapacity] = '\r';
        txQueue[txHead++ % txCapacity] = '\n';
    }
    __set_PRIMASK(primask);
}

static void enqueue(const char *text, uint8_t newline)
{
    if (!text) return;
    const size_t length = strlen(text);
    const size_t total = length + (newline ? 2U : 0U);
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    // Preserve complete messages and never wait for an unopened USB monitor.
    if (total > txCapacity - (txHead - txTail)) {
        droppedBytes += (uint32_t)total;
    } else {
        for (size_t i = 0; i < length; ++i) txQueue[txHead++ % txCapacity] = text[i];
        if (newline) {
            txQueue[txHead++ % txCapacity] = '\r';
            txQueue[txHead++ % txCapacity] = '\n';
        }
    }
    __set_PRIMASK(primask);
}

void print(const char *text)
{
    enqueue(text, 0);
}

void println(const char *text)
{
    enqueue(text, 1);
}

uint32_t serialWrite(const uint8_t *data, uint32_t length)
{
    if (!data) return 0;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t freeBytes = txCapacity - (txHead - txTail);
    if (length > freeBytes) length = freeBytes;
    if (length > 64U) length = 64U;
    for (uint32_t i = 0; i < length; ++i)
        txQueue[txHead++ % txCapacity] = data[i];
    __set_PRIMASK(primask);
    return length;
}

int serialTryPrintln(const char *text)
{
    if (!text) return 0;
    const size_t length = strlen(text);
    if (length > txCapacity - 2U) return 0;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const int fits = length + 2U <= txCapacity - (txHead - txTail);
    if (fits) {
        for (size_t i = 0; i < length; ++i) txQueue[txHead++ % txCapacity] = text[i];
        txQueue[txHead++ % txCapacity] = '\r';
        txQueue[txHead++ % txCapacity] = '\n';
    }
    __set_PRIMASK(primask);
    return fits;
}

int serialTryWriteFrame(const uint8_t *data, uint32_t length)
{
    if (!data || length == 0U || length > txCapacity) return 0;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    USBD_CDC_HandleTypeDef *hcdc =
        (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (txHead != txTail || hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED ||
        hcdc == NULL || hcdc->TxState != 0U) {
        __set_PRIMASK(primask);
        return 0;
    }
    for (uint32_t i = 0; i < length; ++i)
        txQueue[txHead++ % txCapacity] = data[i];
    __set_PRIMASK(primask);
    return 1;
}

void serialProcess(void)
{
    CDC_Process_FS();
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t offset = txTail % txCapacity;
    uint32_t length = txHead - txTail;
    if (length > txCapacity - offset) length = txCapacity - offset;
    if (length > CDC_DATA_FS_MAX_PACKET_SIZE) length = CDC_DATA_FS_MAX_PACKET_SIZE;
    if (length && CDC_Transmit_FS(&txQueue[offset], (uint16_t)length) == USBD_OK) {
        txLineOpen = txQueue[offset + length - 1U] != '\n';
        txTail += length;
    }
    __set_PRIMASK(primask);
}

uint32_t serialDroppedBytes(void)
{
    return droppedBytes;
}
void printInt(int value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    print(buffer);
}
void printFloat(float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.3f", value);
    print(buffer);
}
void printDouble(double value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.6f", value);
    print(buffer);
}
void printlnInt(int value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    println(buffer);
}
void printlnFloat(float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.3f", value);
    println(buffer);
}
void printlnDouble(double value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.6f", value);
    println(buffer);
}
void printBin(uint32_t value)
{
    char buffer[42];
    int index = 0;

    for (int i = 31; i >= 0; i--)
    {
        buffer[index++] = (value & (1UL << i)) ? '1' : '0';

        if (i % 4 == 0 && i != 0)
        {
            buffer[index++] = ' ';
        }
    }

    buffer[index++] = '\r';
    buffer[index++] = '\n';
    buffer[index] = '\0';

    print(buffer);
}

char *serialRead(void)
{
    static char buffer[64];
    static uint8_t index = 0;

    uint8_t c;

    while (CDC_Read_FS(&c, 1) == 1)
    {
        if (c == '\r') continue;
        if (c == '\n')
        {
            buffer[index] = '\0';
            index = 0;
            return buffer;
        }
        if (index < sizeof(buffer) - 1) buffer[index++] = c;
    }
    return NULL;
}
