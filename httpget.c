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

/*
 *  ======== httpget.c ========
 *  HTTP Client GET example application
 */

/* BSD support */
#include "stdio.h"
#include "string.h"
#include <ti/drivers/UART.h>
#include <ti/net/http/httpclient.h>

#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/slnetifwifi.h>

#include "semaphore.h"

#define APPLICATION_NAME      "HTTP GET"

#define HOSTNAME "https://httpbin.org"
#define REQUEST_URI "/get"
/*
#define HOSTNAME              "http://www.google.com"
#define REQUEST_URI           "/"
*/
#define USER_AGENT            "HTTPClient (ARM; TI-RTOS)"
#define HTTP_MIN_RECV         (256)

//extern Display_Handle display;
extern sem_t    ipEventSyncObj;
extern void printError(char *errString, int code);
extern void print(const char *String);
extern UART_Handle uart;

//*****************************************************************************
//
//! \brief  Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************

/*
 *  ======== httpTask ========
 *  Makes a HTTP GET request
 */
void* httpTask(void* pvParameters)
{
    bool moreDataFlag = false;
    char data[HTTP_MIN_RECV];
    int16_t ret = 0;
    int16_t len = 0;

    /* Print Application name */
    HTTPClient_extSecParams httpClientSecParams;

    httpClientSecParams.rootCa = "dst-root-ca-x3.der"; //"dummy-ca-cert.der";
    httpClientSecParams.clientCert = NULL;
    httpClientSecParams.privateKey = NULL;


    sem_wait(&ipEventSyncObj);
    //UART_write( "Sending a HTTP GET request to '%s'\n",HOSTNAME);

    HTTPClient_Handle httpClientHandle;
    int16_t statusCode;
    httpClientHandle = HTTPClient_create(&statusCode,0);
    if (statusCode < 0)
    {
        printError("httpTask: creation of http client handle failed", statusCode);
    }

    ret = HTTPClient_setHeader(httpClientHandle, HTTPClient_HFIELD_REQ_USER_AGENT,USER_AGENT,strlen(USER_AGENT),HTTPClient_HFIELD_PERSISTENT);
    if (ret < 0) {
        printError("httpTask: setting request header failed", ret);
    }

    ret = HTTPClient_connect(httpClientHandle,HOSTNAME,&httpClientSecParams,0);
    if (ret < 0) {
        printError("httpTask: connect failed", ret);
    }
    ret = HTTPClient_sendRequest(httpClientHandle,HTTP_METHOD_GET,REQUEST_URI,NULL,0,0);
    if (ret < 0) {
        printError("httpTask: send failed", ret);
    }

    if (ret != HTTP_SC_OK) {
        printError("httpTask: cannot get status", ret);
    }

    char errorBuff[256]={0};
    sprintf(errorBuff,"HTTP Response Status Code: %d\r\n", ret);
    print(errorBuff);

    len = 0;
    do {
        ret = HTTPClient_readResponseBody(httpClientHandle, data, sizeof(data), &moreDataFlag);
        if (ret < 0) {
            printError("httpTask: response body processing failed", ret);
        }
        print(data);
        len += ret;
    }while (moreDataFlag);

    sprintf(errorBuff,"Received %d bytes of payload\r\n", len);
    print(errorBuff);

    ret = HTTPClient_disconnect(httpClientHandle);
    if (ret < 0)
    {
        printError("httpTask: disconnect failed", ret);
    }

    HTTPClient_destroy(httpClientHandle);
/*
     SlWlanNetworkEntry_t netEntries[10];
     int resultsCount = sl_WlanGetNetworkList(0,10,&netEntries[0]);
     int i;
     for(i=0; i< resultsCount; i++)
     {
         UART_write("%d \n",i+1);
         UART_write("SSID: %.32s        \n",netEntries[i].Ssid);
         UART_write("BSSID: %x:%x:%x:%x:%x:%x    \n",netEntries[i].Bssid[0],netEntries[i].Bssid[1],netEntries[i].Bssid[2],netEntries[i].Bssid[3],netEntries[i].Bssid[4],netEntries[i].Bssid[5]);
         UART_write("Channel: %d    \n",netEntries[i].Channel);
         UART_write("RSSI: %d    \n",netEntries[i].Rssi);
         UART_write("Security type: %d    \n",SL_WLAN_SCAN_RESULT_SEC_TYPE_BITMAP(netEntries[i].SecurityInfo));
         UART_write("Group Cipher: %d    \n",SL_WLAN_SCAN_RESULT_GROUP_CIPHER(netEntries[i].SecurityInfo));
         UART_write("Unicast Cipher bitmap: %d    \n",SL_WLAN_SCAN_RESULT_UNICAST_CIPHER_BITMAP(netEntries[i].SecurityInfo));
         UART_write("Key Mgmt suites bitmap: %d    \n",SL_WLAN_SCAN_RESULT_KEY_MGMT_SUITES_BITMAP(netEntries[i].SecurityInfo));
         UART_write("Hidden SSID: %d\r\n",SL_WLAN_SCAN_RESULT_HIDDEN_SSID(netEntries[i].SecurityInfo));
     }*/
    return(0);
}
