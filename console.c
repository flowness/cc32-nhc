/*
 * Copyright (c) 2016-2017, Texas Instruments Incorporated
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
 *  ======== console.c ========
 */
#include <stdint.h>
#include <stdio.h>

#include <string.h>
#include <stdbool.h>

/* POSIX Header files */
#include <pthread.h>
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/net/wifi/slnetifwifi.h>
#ifdef CC32XX
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC32XX.h>
#endif

/* Example/Board Header files */
#include "Board.h"

/* Console display strings */
const char consoleDisplay[]   = "\fConsole (h for help)\r\n";
const char helpPrompt[]       = "Valid Commands\r\n"                  \
                                "--------------\r\n"                  \
                                "h: help\r\n"                         \
                                "x: clear the screen\r\n"             \
                                "l: wifi List\r\n"                    \
                                "c: Connect wifi\r\n"                 \
                                "s: wifi Status";

const char byeDisplay[]       = "Bye! Hit button1 to start UART again\r\n";
const char tempStartDisplay[] = "Current temp = ";
const char SSIDinput[] = "Please enter new SSID: ";
const char Passwordinput[] = "Please enter SSID password: ";

const char tempMidDisplay[]   = "C (";
const char tempEndDisplay[]   = "F)\r\n";
const char cleanDisplay[]     = "\f";
const char userPrompt[]       = "> ";
const char readErrDisplay[]   = "Problem read UART.\r\n";
UART_Handle uart;

/* Used to determine whether to have the thread block */
volatile bool uartEnabled = true;
sem_t semConsole;

/* Used itoa instead of sprintf to help minimize the size of the stack */
static void itoa(int n, char s[]);
extern void print(const char *String);

/*
 *  ======== gpioButtonFxn ========
 *  Callback function for the GPIO interrupt on Board_GPIO_BUTTON1.
 *  There is no debounce logic here since we are just looking for
 *  a button push. The uartEnabled variable protects use against any
 *  additional interrupts cased by the bouncing of the button.
 */
void gpioButtonFxn(uint_least8_t index)
{

    /* If disabled, enable and post the semaphore */
    if (uartEnabled == false) {
        uartEnabled = true;
        sem_post(&semConsole);
    }
}

/*
 *  ======== simpleConsole ========
 *  Handle the user input. Currently this console does not handle
 *  user back-spaces or other "hard" characters.
 */
void *consoleThread(void *arg0)

//void simpleConsole(void)
{
    char cmd;
    int status;
    int i=0;

    char newSSID[256]={0};
    char SSIDpass[256]={0};
    SlWlanSecParams_t   secParams = {0};
    char printString[512]={0};
    _u16 len = sizeof(SlNetCfgIpV4Args_t);
    _u16 ConfigOpt = 0;   //return value could be one of the following: SL_NETCFG_ADDR_DHCP / SL_NETCFG_ADDR_DHCP_LLA / SL_NETCFG_ADDR_STATIC
    SlNetCfgIpV4Args_t ipV4 = {0};
    _u16 WlanLen = sizeof(SlWlanConnStatusParam_t) ;
    SlWlanConnStatusParam_t WlanConnectInfo = {0};
    SlWlanNetworkEntry_t netEntries[10];
    _i16 resultsCount;


    UART_write(uart, consoleDisplay, sizeof(consoleDisplay));

    /* Loop until read fails or user quits */
    while (1) {
        UART_write(uart, userPrompt, sizeof(userPrompt));
        status = UART_read(uart, &cmd, sizeof(cmd));
        if (status == 0) {
            UART_write(uart, readErrDisplay, sizeof(readErrDisplay));
            cmd = 'q';
        }

        switch (cmd) {
            case 'c':
                i=0;
                sl_Memset(newSSID,0,256);
                sl_Memset(SSIDpass,0,256);
                print(SSIDinput);
                do
                {
                    UART_read(uart,&newSSID[i],1);
                    if(newSSID[i]!=0x0D)
                    {
                        UART_write(uart, &newSSID[i],1);
                    }
                    else
                    {
                        newSSID[i]=0;
                        break;
                    }
                    i++;
                }while(1);
                i=0;
                print("");
                print(Passwordinput);
                do
                {
                    UART_read(uart,&SSIDpass[i],1);
                    if(SSIDpass[i]!=0x0D)
                    {
                        UART_write(uart, &SSIDpass[i],1);
                    }
                    else
                    {
                        SSIDpass[i]=0;
                        break;
                    }
                    i++;
                }while(1);
                print("");
                secParams.Key = (signed char*)SSIDpass;
                secParams.KeyLen = strlen(SSIDpass);
                secParams.Type = SL_WLAN_SEC_TYPE_WPA_WPA2;
                if(sl_WlanConnect((signed char*)newSSID, strlen(newSSID), 0, &secParams, 0)==0)
                {
                    sprintf(printString, "Wifi Connected to %s",newSSID);
                }
                else
                {
                    sprintf(printString, "Wifi not Connected to %s",newSSID);
                }
                print(printString);
                break;
            case 'l':
                print("getting network list");
                resultsCount = sl_WlanGetNetworkList(0,2,&netEntries[0]);
                print("printing network list");
                for(i=0; i< resultsCount; i++)
                {
                    sprintf(printString,"%d. - SSID: %.32s Security type: %d  RSSI: %d",i+1,netEntries[i].Ssid,SL_WLAN_SCAN_RESULT_SEC_TYPE_BITMAP(netEntries[i].SecurityInfo),netEntries[i].Rssi);
                    //sprintf(printString,"SSID: %.32s        ",netEntries[i].Ssid);
                    //sprintf(printString,"BSSID: %x:%x:%x:%x:%x:%x    ",netEntries[i].Bssid[0],netEntries[i].Bssid[1],netEntries[i].Bssid[2],netEntries[i].Bssid[3],netEntries[i].Bssid[4],netEntries[i].Bssid[5]);
                    //sprintf(printString,"Channel: %d    ",netEntries[i].Channel);
                    //sprintf(printString,"RSSI: %d    ",netEntries[i].Rssi);
                    //sprintf(printString,"Security type: %d    ",SL_WLAN_SCAN_RESULT_SEC_TYPE_BITMAP(netEntries[i].SecurityInfo));
/*                    sprintf(printString,"Group Cipher: %d    ",SL_WLAN_SCAN_RESULT_GROUP_CIPHER(netEntries[i].SecurityInfo));
                    sprintf(printString,"Unicast Cipher bitmap: %d    ",SL_WLAN_SCAN_RESULT_UNICAST_CIPHER_BITMAP(netEntries[i].SecurityInfo));
                    sprintf(printString,"Key Mgmt suites bitmap: %d    ",SL_WLAN_SCAN_RESULT_KEY_MGMT_SUITES_BITMAP(netEntries[i].SecurityInfo));
                    sprintf(printString,"Hidden SSID: %d\r\n",SL_WLAN_SCAN_RESULT_HIDDEN_SSID(netEntries[i].SecurityInfo));*/
                    print(printString);
                }

                break;
            case 's':
                sl_WlanGet(SL_WLAN_CONNECTION_INFO, NULL , &WlanLen, (_u8*)&WlanConnectInfo);
                sprintf(printString,"WLAN connected to: %s", WlanConnectInfo.ConnectionInfo.StaConnect.SsidName);
                print(printString);

                sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&ConfigOpt,&len,(_u8 *)&ipV4);

                sprintf(printString,"DHCP is %s IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d DNS %d.%d.%d.%d\n",
                    (ConfigOpt == SL_NETCFG_ADDR_DHCP) ? "ON" : "OFF",
                    SL_IPV4_BYTE(ipV4.Ip,3),SL_IPV4_BYTE(ipV4.Ip,2),SL_IPV4_BYTE(ipV4.Ip,1),SL_IPV4_BYTE(ipV4.Ip,0),
                    SL_IPV4_BYTE(ipV4.IpMask,3),SL_IPV4_BYTE(ipV4.IpMask,2),SL_IPV4_BYTE(ipV4.IpMask,1),SL_IPV4_BYTE(ipV4.IpMask,0),
                    SL_IPV4_BYTE(ipV4.IpGateway,3),SL_IPV4_BYTE(ipV4.IpGateway,2),SL_IPV4_BYTE(ipV4.IpGateway,1),SL_IPV4_BYTE(ipV4.IpGateway,0),
                    SL_IPV4_BYTE(ipV4.IpDnsServer,3),SL_IPV4_BYTE(ipV4.IpDnsServer,2),SL_IPV4_BYTE(ipV4.IpDnsServer,1),SL_IPV4_BYTE(ipV4.IpDnsServer,0));

                print(printString);
                break;
            case 'x':
                UART_write(uart, cleanDisplay, sizeof(cleanDisplay));
                break;
            case 'q':
                UART_write(uart, byeDisplay, sizeof(byeDisplay));
                //return;
            case 'h':
            default:
                UART_write(uart, helpPrompt, sizeof(helpPrompt));
                break;
        }
    }
}

/*
 *  ======== consoleThread ========
 */
/*
void *consoleThread(void *arg0)
{
    UART_Params uartParams;
    int retc;

#ifdef CC32XX
     //  The CC3220 examples by default do not have power management enabled.
     //  This allows a better debug experience. With the power management
     //  enabled, if the device goes into a low power mode the emulation
     //  session is lost.
     //  Let's enable it and also configure the button to wake us up.
     //
    PowerCC32XX_Wakeup wakeup;

    PowerCC32XX_getWakeup(&wakeup);
    wakeup.wakeupGPIOFxnLPDS = gpioButtonFxn;
    PowerCC32XX_configureWakeup(&wakeup);
    Power_enablePolicy();
#endif

    // Configure the button pin
    GPIO_setConfig(Board_GPIO_BUTTON1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    // install Button callback and enable it
    GPIO_setCallback(Board_GPIO_BUTTON1, gpioButtonFxn);
    GPIO_enableInt(Board_GPIO_BUTTON1);

    retc = sem_init(&semConsole, 0, 0);
    if (retc == -1) {
        while (1);
    }

    UART_init();

     // Initialize the UART parameters outside the loop. Let's keep
     //  most of the defaults (e.g. baudrate = 115200) and only change the
     //  following.

    UART_Params_init(&uartParams);
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uart = UART_open(Board_UART0, &uartParams);

    // Loop forever to start the console
    while (1) {
        if (uartEnabled == false) {
            retc = sem_wait(&semConsole);
            if (retc == -1) {
                while (1);
            }
        }

        // Create a UART for the console
        uart = UART_open(Board_UART0, &uartParams);
        if (uart == NULL) {
            while (1);
        }

        simpleConsole();


         // Since we returned from the console, we need to close the UART.
         // The Power Manager will go into a lower power mode when the UART
         // is closed.

        UART_close(uart);
        uartEnabled = false;
    }
}*/

/*
 * The following function is from good old K & R.
 */
static void reverse(char s[])
{
    int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/*
 * The following function is from good old K & R.
 */
static void itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
         s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}
