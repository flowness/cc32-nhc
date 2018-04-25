/*
 * Copyright (c) 2015-2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "stdio.h"
#include "string.h"

#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/slnetifwifi.h>

//#include <ti/display/Display.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC32XX.h>
#include <ti/drivers/SPI.h>

#include "Board.h"
#include "pthread.h"
#include "semaphore.h"


#define SPAWN_TASK_PRIORITY                   (9)
#define SPAWN_STACK_SIZE                      (4096)
#define TASK_STACK_SIZE                       (2048)
#define SLNET_IF_WIFI_PRIO                    (5)
#define SLNET_IF_WIFI_NAME                    "CC3220"

/*
#define SSID_NAME                             "Paradox NVR"                     // AP SSID
#define SECURITY_TYPE                         SL_WLAN_SEC_TYPE_WPA_WPA2         // Security type could be SL_WLAN_SEC_TYPE_WPA_WPA2 or SL_WLAN_SEC_TYPE_OPEN
#define SECURITY_KEY                          "ParadoxR2017"                    // Password of the secured AP
*/

#define SSID_NAME                             "paradox-rnd"                     // AP SSID
#define SECURITY_TYPE                         SL_WLAN_SEC_TYPE_WPA_WPA2         // Security type could be SL_WLAN_SEC_TYPE_WPA_WPA2 or SL_WLAN_SEC_TYPE_OPEN
#define SECURITY_KEY                          "P@r@d0xx"                    // Password of the secured AP

/*
#define SSID_NAME                             "Lessner"                     // AP SSID
#define SECURITY_TYPE                         SL_WLAN_SEC_TYPE_WPA_WPA2         // Security type could be SL_WLAN_SEC_TYPE_WPA_WPA2 or SL_WLAN_SEC_TYPE_OPEN
#define SECURITY_KEY                          "14071983"                    // Password of the secured AP
*/

sem_t     ipEventSyncObj;

#define MAX_NUM_RX_BYTES    1000   // Maximum RX bytes to receive in one go
#define MAX_NUM_TX_BYTES    1000   // Maximum TX bytes to send in one go

uint32_t wantedRxBytes;            // Number of bytes received so far
uint8_t rxBuf[MAX_NUM_RX_BYTES];   // Receive buffer
uint8_t txBuf[MAX_NUM_TX_BYTES];   // Transmit buffer

UART_Handle handle;

pthread_t httpThread = (pthread_t)NULL;
pthread_t spawn_thread = (pthread_t)NULL;
pthread_t console_Thread = (pthread_t)NULL;


//Display_Handle display;

extern void* httpTask(void* pvParameters);
extern void* consoleThread(void *arg0);
extern UART_Handle uart;
extern const char helpPrompt;
extern const char userPrompt;


void print(const char *String)
{
    //char errorBuff[256]={0};
    //sprintf(errorBuff,"Error! code = %d, desc = %s\r\n", code, errString);
    UART_write(uart, String,strlen(String));
    UART_write(uart, "\r\n",sizeof("\r\n"));

}

static void DisplayBanner()
{
    print("");
    print("\t\t *************************************************");
    print("\t\t            flowness Application       ");
    print("\t\t *************************************************");
}

/*!
    \brief          SimpleLinkNetAppEventHandler

    This handler gets called whenever a Netapp event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling IPv4 / IPv6 IP address acquisition.
    2. Handling IPv4 / IPv6 IP address Dropping.

    \param          pNetAppEvent     -   pointer to Netapp event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 5.7

*/
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(pNetAppEvent == NULL)
    {
        return;
    }

    switch(pNetAppEvent->Id)
    {
        case SL_NETAPP_EVENT_IPV4_ACQUIRED:
        case SL_NETAPP_EVENT_IPV6_ACQUIRED:
            /* Initialize SlNetSock layer with CC3x20 interface                      */
            SlNetIf_init(0);
            SlNetIf_add(SLNETIF_ID_1, SLNET_IF_WIFI_NAME, (const SlNetIf_Config_t *)&SlNetIfConfigWifi, SLNET_IF_WIFI_PRIO);

            SlNetSock_init(0);
            SlNetUtil_init(0);
            sem_post(&ipEventSyncObj);
            break;
        default:
            break;
   }
}
/*!
    \brief          SimpleLinkFatalErrorEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver. After this routine is called, the user's
    application must restart the device in order to recover.

    \param          slFatalErrorEvent    -   pointer to fatal error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
    /* Unused in this application */
}
/*!
    \brief          SimpleLinkNetAppRequestMemFreeEventHandler

    This handler gets called whenever the NWP is done handling with
    the buffer used in a NetApp request. This allows the use of
    dynamic memory with these requests.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkNetAppRequestEventHandler

    This handler gets called whenever a NetApp event is reported
    by the NWP / Host driver. User can write he's logic to handle
    the event here.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest, SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkHttpServerEventHandler

    This handler gets called whenever a HTTP event is reported
    by the NWP internal HTTP server.

    \param          pHttpEvent       -   pointer to http event data.

    \param          pHttpEvent       -   pointer to http response.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) chapter 9.

*/
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
                                      SlNetAppHttpServerResponse_t *pHttpResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkWlanEventHandler

    This handler gets called whenever a WLAN event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling connection / Disconnection.
    2. Handling Addition of station / removal.
    3. RX filter match handler.
    4. P2P connection establishment.

    \param          pWlanEvent       -   pointer to Wlan event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) sections 4.3.4, 4.4.5 and 4.5.5.

    \sa             cmdWlanConnectCallback, cmdEnableFilterCallback, cmdWlanDisconnectCallback,
                    cmdP2PModecallback.

*/
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    /* Unused in this application */
}
/*!
    \brief          SimpleLinkGeneralEventHandler

    This handler gets called whenever a general error is reported
    by the NWP / Host driver. Since these errors are not fatal,
    application can handle them.

    \param          pDevEvent    -   pointer to device error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkSockEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver.

    \param          SlSockEvent_t    -   pointer to socket event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 7.6.

*/
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    /* Unused in this application */
}

/*
 *  ======== printError ========
 */
void printError(char *errString, int code)
{
    char errorBuff[256]={0};
    sprintf(errorBuff,"Error! code = %d, desc = %s\r\n", code, errString);
    UART_write(uart, errorBuff,sizeof(errorBuff));
    while(1);
}

int16_t Connect(void)
{
    SlWlanSecParams_t   secParams = {0};
    int16_t ret = 0;
    secParams.Key = (signed char*)SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;
    ret = sl_WlanConnect((signed char*)SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    if (ret)
    {
        printError("Connection failed , error code : %d \r\n", ret);
    }

    return ret;
}

/*
// Callback function
static void readCallback(UART_Handle handle, void *rxBuf, size_t size)
{
    size_t i=0;
    // Make sure we received all expected bytes
    if (size == wantedRxBytes) {
        // Copy bytes from RX buffer to TX buffer
       for(; i < size; i++)
           txBuf[i] = ((uint8_t*)rxBuf)[i];

       // Echo the bytes received back to transmitter
      // UART_write(handle, txBuf, size);

       // Start another read, with size the same as it was during first call to
       // UART_read()
       UART_read(handle, rxBuf, wantedRxBytes);
    }
    else {
        // Handle error or call to UART_readCancel()
    }
}
*/

void mainThread(void *pvParameters)
{
    int32_t             status = 0;
    pthread_attr_t      pAttrs_spawn;
    pthread_attr_t      pAttrs;
    struct sched_param  priParam;
    int32_t             mode;
    int16_t             ret;
    UART_Params uartParams;


    SPI_init();
    /*Display_init();
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL) {
        // Failed to open display driver
        while(1);
    }*/

    UART_init();

    /*
     *  Initialize the UART parameters outside the loop. Let's keep
     *  most of the defaults (e.g. baudrate = 115200) and only change the
     *  following.
     */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uart = UART_open(Board_UART0, &uartParams);

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 1;
    status = pthread_attr_setschedparam(&pAttrs, &priParam);
    status |= pthread_attr_setstacksize(&pAttrs, TASK_STACK_SIZE);

    ret = pthread_create(&console_Thread, &pAttrs, consoleThread, NULL);
    if (ret != 0) {
        /* pthread_create() failed */
        while (1);
    }


    print("UART Display initilized...");

    ret = sem_init(&ipEventSyncObj,0,0);
    if(ret != 0)
    {
        printError("Semaphore init failed, error code : %d \r\n", ret);
        return;
    }
    /* Start the SimpleLink Host */
    pthread_attr_init(&pAttrs_spawn);
    priParam.sched_priority = SPAWN_TASK_PRIORITY;
    status = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
    status |= pthread_attr_setstacksize(&pAttrs_spawn, SPAWN_STACK_SIZE);

    status = pthread_create(&spawn_thread, &pAttrs_spawn, sl_Task, NULL);
    if(status)
    {
        printError("Task create failed, error code : %d \r\n", status);
    }
    print("sl_Task thread started...");

    /* initialize the device */
    mode = sl_Start(0, 0, 0);
    if (mode < 0)
    {
        printError("Sl_start failed, error code : %d \r\n", status);
    }

    if(mode != ROLE_STA)
    {
        mode = sl_WlanSetMode(ROLE_STA);
        //ASSERT_ON_ERROR(mode);
        if (mode < 0)
        {
            printError("sl_WlanSetMode failed, error code : %d \r\n", status);
        }
        mode = sl_Stop(200);
        //ASSERT_ON_ERROR(mode);
        if (mode < 0)
        {
            printError("sl_Stop failed, error code : %d \r\n", status);
        }
        mode = sl_Start(0, 0, 0);
        if (mode < 0 || (mode != ROLE_STA))
        {
            //ASSERT_ON_ERROR(mode);
            printError("sl_Start failed in STA role, error code : %d \r\n", status);

        }
        UART_write(uart, "Device started as STATION \n\r",sizeof("Device started as STATION \n\r"));
    }
    print("sl_Start...");

    DisplayBanner();
    print(&helpPrompt);
    print(&userPrompt);

    if(0==Connect())
    {
        char printString[256]={0};
        sprintf(printString, "Wifi Connected to %s",SSID_NAME);
        print(printString);


        status = pthread_create(&httpThread, &pAttrs, httpTask, NULL);
        if(status)
        {
            printError("Task create failed, error code : %d \r\n", status);
        }

    }

}

