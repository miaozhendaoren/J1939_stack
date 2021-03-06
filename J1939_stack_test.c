/////////////////////////////////////////////////////////////////////////////////////////
// File:         J1939_stack_test.c
// Module:       TEST only
// Description:  Test the J1939 stack
// Originator:   MB
// Derived from: Vector Gmbh xlCANdemo.c
// Date created: 11 June 2014
//---------------------------------------------------------------------------------------
// Revision Log:
//   $Log: $
//
//---------------------------------------------------------------------------------------
// Notes:
//
//---------------------------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C)2014 by Timespace Technology Ltd.  All rights reserved.
//
// This software is confidential.  It is not to be copied or distributed in any form
// to any third party.
//
// This software is provided by Timespace Technology on an 'as is' basis, and Timespace
// Technology expressly disclaims any and all warranties, expressed or implied including,
// without limitation, warranties of merchantability and fitness for a particular
// purpose. In no event shall Timespace Technology be liable for any direct, indirect,
// incidental, punitive or consequential damages - of any kind whatsoever - with respect
// to the software.
/////////////////////////////////////////////////////////////////////////////////////////


/*------------------------------------------------------------------------------
| File:
|   xlCANdemo.C
| Project:
|   Sample for XL - Driver Library
|   Example application using 'vxlapi.dll'
|-------------------------------------------------------------------------------
| $Author: vismwf $    $Locker: $   $Revision: 13365 $
| $Header: /VCANDRV/XLAPI/samples/xlCANdemo/xlCANdemo.c 17    20.11.07 14:43 J�rg $
|-------------------------------------------------------------------------------
| Copyright (c) 2004 by Vector Informatik GmbH.  All rights reserved.
 -----------------------------------------------------------------------------*/


#if defined(_Windows) || defined(_MSC_VER) || defined (__GNUC__)
 #define  STRICT
 #include <windows.h>
#endif

#include <stdio.h>

#define UNUSED_PARAM(a) { a=a; }

#define RECEIVE_EVENT_SIZE 1                // DO NOT EDIT! Currently 1 is supported only
#define RX_QUEUE_SIZE      4096             // internal driver queue size in CAN events

#include "vxlapi.h"

#include "J1939_includes.h"
#include "FMS.h"


/////////////////////////////////////////////////////////////////////////////
// globals
char            g_AppName[XL_MAX_LENGTH+1]  = "J1939_stack_test";     // Application name which is displayed in VHWconf
XLportHandle    g_xlPortHandle              = XL_INVALID_PORTHANDLE;  // Global porthandle (we use only one!)
XLdriverConfig  g_xlDrvConfig;                                        // Contains the actual hardware configuration
XLaccess        g_xlChannelMask             = 0;                      // Global channelmask (includes all founded channels)
XLaccess        g_xlPermissionMask          = 0;                      // Global permissionmask (includes all founded channels)
unsigned int    g_BaudRate                  = 500000;                 // Default baudrate
int             g_silent                    = 0;                      // flag to visualize the message events (on/off)
unsigned int    g_TimerRate                 = 0;                      // Global timerrate (to toggel)
XLaccess        g_xlChanMaskTx              = 0;                      // Global transmit channel mask


/////////////////////////////////////////////////////////////////////////////
// thread variables
XLhandle        g_hMsgEvent;                                          // notification handle for the receive queue
HANDLE          g_hRXThread;                                          // thread handle (RX)
HANDLE          g_hTXThread;                                          // thread handle (TX)
int             g_RXThreadRun;                                        // flag to start/stop the RX thread
int             g_TXThreadRun;                                        // flag to start/stop the TX thread (for the transmission burst)


////////////////////////////////////////////////////////////////////////////
// functions (Threads)
DWORD     WINAPI RxThread( PVOID par );
DWORD     WINAPI TxThread( LPVOID par );


////////////////////////////////////////////////////////////////////////////
// functions (prototypes)
void     demoHelp(void);
void     demoPrintConfig(void);
XLstatus demoCreateRxThread(void);


////////////////////////////////////////////////////////////////////////////
// demoHelp()
// shows the program functionality
//
////////////////////////////////////////////////////////////////////////////
void demoHelp(void)
{
   printf("\n");
   printf("----------------------------------------------------------\n");
   printf("-            J1939_test_stack - HELP                     -\n");
   printf("----------------------------------------------------------\n");
   printf("- Keyboard commands:                                     -\n");
   printf("- 't'      Transmit a message                            -\n");
   printf("- 'b'      Transmit a message burst (toggle)             -\n");
   printf("- 'm'      Transmit a remote message                     -\n");
   printf("- 'g'      Request chipstate                             -\n");
   printf("- 's'      Start/Stop                                    -\n");
   printf("- 'r'      Reset clock                                   -\n");
   printf("- '+'      Select channel      (up)                      -\n");
   printf("- '-'      Select channel      (down)                    -\n");
   printf("- 'i'      Select transmit Id  (up)                      -\n");
   printf("- 'I'      Select transmit Id  (down)                    -\n");
   printf("- 'x'      Toggle extended/standard Id                   -\n");
   printf("- 'o'      Toggle output mode                            -\n");
   printf("- 'a'      Toggle timer                                  -\n");
   printf("- 'v'      Toggle logging to screen                      -\n");
   printf("- 'p'      Show hardware configuration                   -\n");
   printf("- 'h'      Help                                          -\n");
   printf("- 'ESC'    Exit                                          -\n");
   printf("----------------------------------------------------------\n");
   printf("- 'PH'->PortHandle; 'CM'->ChannelMask; 'PM'->Permission  -\n");
   printf("----------------------------------------------------------\n");
   printf("\n");
}


////////////////////////////////////////////////////////////////////////////
// demoPrintConfig()
// shows the actual hardware configuration
//
////////////////////////////////////////////////////////////////////////////
void demoPrintConfig(void)
{
   unsigned int i;
   char         str[XL_MAX_LENGTH + 1]="";

   printf("----------------------------------------------------------\n");
   printf("- %02d channels       Hardware Configuration               -\n", g_xlDrvConfig.channelCount);
   printf("----------------------------------------------------------\n");

   for (i=0; i < g_xlDrvConfig.channelCount; i++)
   {
      printf("- Ch:%02d, CM:0x%03I64x,", 
      g_xlDrvConfig.channel[i].channelIndex, g_xlDrvConfig.channel[i].channelMask);

      strncpy( str, g_xlDrvConfig.channel[i].name, 23);
      printf(" %23s,", str);

      memset(str, 0, sizeof(str));
    
      if (g_xlDrvConfig.channel[i].transceiverType != XL_TRANSCEIVER_TYPE_NONE)
      {
         strncpy( str, g_xlDrvConfig.channel[i].transceiverName, 13);
         printf("%13s -\n", str);
      }
      else printf("    no Cab!   -\n", str);
   }
  
   printf("----------------------------------------------------------\n\n");
}


////////////////////////////////////////////////////////////////////////////
// demoTransmit
// transmit a CAN message (depending on an ID, channel)
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoTransmit(unsigned int txID, XLaccess xlChanMaskTx)
{
   static unsigned char loop;
#if 1
   J1939_TX_MESSAGE_T msg;

   memset(&msg, 0, sizeof(msg));
   msg.byte_count = 8;
   msg.PGN = FMS_DD;
   msg.priority = 3;
   for (unsigned char i = 0; i < 8; i++)
	   msg.data[i] = i + loop;
   loop++;

   // set global channel mask for HAL layer
   g_xlChanMaskTx = xlChanMaskTx;

   Transmit_J1939msg(&msg);
#else
   static XLevent       xlEvent;
   XLstatus             xlStatus;
   unsigned int         messageCount = 1;

   memset(&xlEvent, 0, sizeof(xlEvent));

   xlEvent.tag                 = XL_TRANSMIT_MSG;
   xlEvent.tagData.msg.id      = txID;
   xlEvent.tagData.msg.dlc     = 8;
   xlEvent.tagData.msg.flags   = 0;
   for (unsigned char i = 0; i < 8; i++)
      xlEvent.tagData.msg.data[i] = i + loop;
   loop++;

   xlStatus = xlCanTransmit(g_xlPortHandle, xlChanMaskTx, &messageCount, &xlEvent);

   printf("- Transmit         : CM(0x%I64x), %s\n", xlChanMaskTx, xlGetErrorString(xlStatus));

   return xlStatus;
#endif
}


////////////////////////////////////////////////////////////////////////////
// demoTransmitBurst
// transmit a message burst (also depending on an IC, channel).
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoTransmitBurst(unsigned int txID) 
{
   DWORD               ThreadId=0;
   static unsigned int TXID;

   TXID = txID;

   if (!g_TXThreadRun)
   {
      printf("- print txID: %d\n", txID);

      g_hTXThread = CreateThread(0, 0x1000, TxThread, (LPVOID)&TXID, 0, &ThreadId);
   }
   else
   {
      g_TXThreadRun = 0;
      Sleep(10);
      WaitForSingleObject(g_hTXThread, 10);
   }

   return XL_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////
// demoTransmitRemote
// transmit a remote frame
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoTransmitRemote(unsigned int txID, XLaccess xlChanMaskTx)
{
   XLstatus      xlStatus;
   XLevent       xlEvent;
   unsigned int  messageCount = 1;

   memset(&xlEvent, 0, sizeof(xlEvent));

   xlEvent.tag               = XL_TRANSMIT_MSG;
   xlEvent.tagData.msg.id    = txID;
   xlEvent.tagData.msg.flags = XL_CAN_MSG_FLAG_REMOTE_FRAME;
   xlEvent.tagData.msg.dlc   = 8;

   xlStatus = xlCanTransmit(g_xlPortHandle, xlChanMaskTx, &messageCount, &xlEvent);
   printf("- Transmit REMOTE  : CM(0x%I64x), %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
  
   return XL_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////
// demoStartStop
// toggle the channel activate/deactivate
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoStartStop(int activated)
{
   XLstatus xlStatus;

   if (activated)
   {
      xlStatus = xlActivateChannel(g_xlPortHandle, g_xlChannelMask, XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
      printf("- ActivateChannel : CM(0x%I64x), %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
   }
   else
   {
      xlStatus = xlDeactivateChannel(g_xlPortHandle, g_xlChannelMask);
      printf("- DeactivateChannel: CM(0x%I64x), %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
   }

   return XL_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////
// demoSetOutput
// toggle NORMAL/SILENT mode of a CAN channel
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoSetOutput(int outputMode, XLaccess xlChanMaskTx)
{
   XLstatus xlStatus;
   char *sMode = "NORMAL";
  
   switch (outputMode)
   {
      case XL_OUTPUT_MODE_NORMAL:
         sMode = "NORMAL";
      break;

      case XL_OUTPUT_MODE_SILENT:
         sMode = "SILENT";
      break;

      case XL_OUTPUT_MODE_TX_OFF:
         sMode = "SILENT-TXOFF";
      break;
   }

   // to get an effect we deactivate the channel first.
   xlStatus = xlDeactivateChannel(g_xlPortHandle, g_xlChannelMask);

   xlStatus = xlCanSetChannelOutput(g_xlPortHandle, xlChanMaskTx, outputMode);
   printf("- SetChannelOutput: CM(0x%I64x), %s, %s, %d\n", xlChanMaskTx, sMode, xlGetErrorString(xlStatus), outputMode);
 
   // and activate the channel again.
   xlStatus = xlActivateChannel(g_xlPortHandle, g_xlChannelMask, XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
  
   return xlStatus;
}


////////////////////////////////////////////////////////////////////////////
// demoCreateRxThread
// set the notification and creates the thread.
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoCreateRxThread(void)
{
   XLstatus      xlStatus = XL_ERROR;
   DWORD         ThreadId=0;
 
   if (g_xlPortHandle != XL_INVALID_PORTHANDLE)
   {
      // Send an event for each Msg!!!
      xlStatus = xlSetNotification (g_xlPortHandle, &g_hMsgEvent, 1);
         
      g_hRXThread = CreateThread(0, 0x1000, RxThread, (LPVOID) 0, 0, &ThreadId);
   }

   return xlStatus;
}


////////////////////////////////////////////////////////////////////////////
// demoInitDriver
// initializes the driver with one port and all founded channels which
// have a connected CAN cab/piggy.
//
////////////////////////////////////////////////////////////////////////////
XLstatus demoInitDriver(XLaccess *pxlChannelMaskTx, unsigned char *pxlChannelIndex)
{
   XLstatus          xlStatus;
   XLaccess          xlChannelMaskTx = 0;
   unsigned int      i;
  
  
   // ------------------------------------
   // open the driver
   // ------------------------------------
   xlStatus = xlOpenDriver ();
  
   // ------------------------------------
   // get/print the hardware configuration
   // ------------------------------------
   if (XL_SUCCESS == xlStatus)
   {
      xlStatus = xlGetDriverConfig(&g_xlDrvConfig);
   }
  
   if (XL_SUCCESS == xlStatus)
   {
      demoPrintConfig();
    
      printf("Usage: J1939_stack_test <BaudRate> <ApplicationName> <Identifier>\n\n");
    
      // ------------------------------------
      // select the wanted channels
      // ------------------------------------
      g_xlChannelMask = 0;

      for (i=0; i < g_xlDrvConfig.channelCount; i++)
      {
         // we take all hardware we found and
         // check that we have only CAN cabs/piggy's
         // at the moment there is no VN8910 XLAPI support!
         if (g_xlDrvConfig.channel[i].channelBusCapabilities & XL_BUS_ACTIVE_CAP_CAN)
         { 
            if (!*pxlChannelMaskTx)
            {
               *pxlChannelMaskTx = g_xlDrvConfig.channel[i].channelMask;
               *pxlChannelIndex  = g_xlDrvConfig.channel[i].channelIndex;
            }

            g_xlChannelMask |= g_xlDrvConfig.channel[i].channelMask;
         }
      }
    
      if (!g_xlChannelMask)
      {
         printf("ERROR: no available channels found! (e.g. no CANcabs...)\n\n");
         xlStatus = XL_ERROR;
      }
   }

   g_xlPermissionMask = g_xlChannelMask;
  
   // ------------------------------------
   // open ONE port including all channels
   // ------------------------------------
   if (XL_SUCCESS == xlStatus)
   {
      xlStatus = xlOpenPort(&g_xlPortHandle, g_AppName, g_xlChannelMask, &g_xlPermissionMask, RX_QUEUE_SIZE, XL_INTERFACE_VERSION, XL_BUS_TYPE_CAN);
      printf("- OpenPort         : CM=0x%I64x, PH=0x%02X, PM=0x%I64x, %s\n", 
      g_xlChannelMask, g_xlPortHandle, g_xlPermissionMask, xlGetErrorString(xlStatus));
   }

   if ((XL_SUCCESS == xlStatus) && (XL_INVALID_PORTHANDLE != g_xlPortHandle))
   { 
      // ------------------------------------
      // if we have permission we set the
      // bus parameters (baudrate)
      // ------------------------------------
      if (g_xlChannelMask == g_xlPermissionMask)
      {
         xlStatus = xlCanSetChannelBitrate(g_xlPortHandle, g_xlChannelMask, g_BaudRate);
         printf("- SetChannelBitrate: baudr.=%u, %s\n",g_BaudRate, xlGetErrorString(xlStatus));
      } 
      else
      {
         printf("-                  : we have NO init access!\n");
      } 
   }
   else
   {
      xlClosePort(g_xlPortHandle);
      g_xlPortHandle = XL_INVALID_PORTHANDLE;
      xlStatus = XL_ERROR;
   }
  
   return xlStatus;
}                    


////////////////////////////////////////////////////////////////////////////
// demoCleanUp()
// close the port and the driver
//
////////////////////////////////////////////////////////////////////////////
static XLstatus demoCleanUp(void)
{
   XLstatus xlStatus;
    
   if (g_xlPortHandle != XL_INVALID_PORTHANDLE)
   {
      xlStatus = xlClosePort(g_xlPortHandle);
      printf("- ClosePort        : PH(0x%x), %s\n", g_xlPortHandle, xlGetErrorString(xlStatus));
   }

   g_xlPortHandle = XL_INVALID_PORTHANDLE;
   xlCloseDriver();

   return XL_SUCCESS;    // No error handling
}


////////////////////////////////////////////////////////////////////////////
// main
// 
//
////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
   XLstatus      xlStatus;
   XLaccess      xlChanMaskTx = 0;

   int           stop = 0;
   int           activated = 0;
   int           c;
   unsigned char xlChanIndex = 0;
   unsigned int  txID = 0x01;
   int           outputMode = XL_OUTPUT_MODE_NORMAL;


   printf("----------------------------------------------------------\n");
   printf("- J1939_stack_test - Test Application for J1939 stack    -\n");
   printf("-             using XL Family Driver API                 -\n");
   printf("-             Timespace Technology,  " __DATE__"         -\n");
#ifdef WIN64
   printf("-             - 64bit Version -                          -\n");
#endif
   printf("----------------------------------------------------------\n");

   // ------------------------------------
   // commandline may specify application 
   // name and baudrate
   // ------------------------------------
   if (argc > 1)
   {
      g_BaudRate = atoi(argv[1]);

      if (g_BaudRate)
      {
         printf("Baudrate = %u\n", g_BaudRate);
         argc--;
         argv++;
      } 
   }

   if (argc > 1)
   {
      strncpy(g_AppName, argv[1], XL_MAX_APPNAME);
      g_AppName[XL_MAX_APPNAME] = 0;
      printf("AppName = %s\n", g_AppName);
      argc--;
      argv++;
   }

   if (argc > 1)
   {
      sscanf (argv[1], "%lx", &txID ) ;

      if (txID)
      {
         printf("TX ID = %lx\n", txID);
      }
   }

   // ------------------------------------
   // initialize the driver structures 
   // for the application
   // ------------------------------------
   xlStatus = demoInitDriver(&xlChanMaskTx, &xlChanIndex);
   printf("- Init             : %s\n", xlGetErrorString(xlStatus));
  
#if 0
   if (XL_SUCCESS == xlStatus)
   {
      // ------------------------------------
      // create the RX thread to read the
      // messages
      // ------------------------------------
      xlStatus = demoCreateRxThread();
      printf("- Create RX thread : %s\n",  xlGetErrorString(xlStatus));
   }
#endif

   if (XL_SUCCESS == xlStatus)
   {
      // ------------------------------------
      // go with all selected channels on bus
      // ------------------------------------
      xlStatus = xlActivateChannel(g_xlPortHandle, g_xlChannelMask, XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
      printf("- ActivateChannel  : CM=0x%I64x, %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
      if (xlStatus == XL_SUCCESS)
      {
         activated = 1;
      }
   }

   printf("\n: Press <h> for help - actual channel Ch=%d, CM=0x%02I64x\n", xlChanIndex, xlChanMaskTx);

   // initialise the J1939 stack
   J1939_stk_init();

   // ------------------------------------
   // parse the key - commands
   // ------------------------------------
   while (stop == 0)
   {
      unsigned long n;
      INPUT_RECORD ir;

      // run the J1939 stack
      J1939_stk_periodic();

      ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &ir, 1, &n);

      if ((n == 1) && (ir.EventType == KEY_EVENT))
      {
         if (ir.Event.KeyEvent.bKeyDown)
         {
            c = ir.Event.KeyEvent.uChar.AsciiChar;

            switch (c)
            {
               case 'v':
                  if (g_silent)
                  {
                     g_silent = 0;
                     printf("- screen on\n");
                  }
                  else
                  {
                     g_silent = 1;
                     printf("- screen off\n");
                  }
               break;

               case 't': // transmit a message
                  demoTransmit(txID, xlChanMaskTx);
               break;

               case 'b':  // transmit message burst 
                  demoTransmitBurst(txID);
               break; 

               case 'm': // transmit a remote message
                  demoTransmitRemote(txID, xlChanMaskTx);
               break;

               case '-': // channel selection
                  if (xlChanIndex == 0)
                  {
                     xlChanIndex = g_xlDrvConfig.channelCount;
                  }
                  xlChanIndex--;
            
				  xlChanMaskTx = g_xlDrvConfig.channel[xlChanIndex].channelMask;
                  printf("- TX Channel set to channel: %02d, %s CM(0x%I64x)\n", 
                     g_xlDrvConfig.channel[xlChanIndex].channelIndex, g_xlDrvConfig.channel[xlChanIndex].name, xlChanMaskTx);
               break;
            
               case '+': // channel selection   
                  xlChanIndex++;
                  if (xlChanIndex >= g_xlDrvConfig.channelCount)
                  {
                     xlChanIndex = 0;
                  }

                  xlChanMaskTx = g_xlDrvConfig.channel[xlChanIndex].channelMask;
                  printf("- TX Channel set to channel: %02d, %s CM(0x%I64x)\n", 
                  g_xlDrvConfig.channel[xlChanIndex].channelIndex, g_xlDrvConfig.channel[xlChanIndex].name, xlChanMaskTx);
               break;

               case 'x':
                  txID ^= XL_CAN_EXT_MSG_ID; // toggle ext/std
                  printf("- Id set to 0x%08X\n", txID);
               break;

               case 'I': // id selection
                  if (txID & XL_CAN_EXT_MSG_ID)
                  {
                     txID = (txID-1) | XL_CAN_EXT_MSG_ID;
                  }
                  else if (txID == 0)
                  {
                     txID = 0x7FF;
                  }
                  else
                  {
                     txID--;
                  }
                  printf("- Id set to 0x%08X\n", txID);
               break;

               case 'i':
                  if (txID & XL_CAN_EXT_MSG_ID)
                  {
                     txID = (txID+1) | XL_CAN_EXT_MSG_ID;
                  }
                  else if (txID == 0x7FF)
                  {
                     txID = 0;
                  }
                  else
                  {
                     txID++;
                  }
                  printf("- Id set to 0x%08X\n", txID);
               break;

               case 'g':
                  xlStatus = xlCanRequestChipState(g_xlPortHandle, g_xlChannelMask);
                  printf("- RequestChipState : CM(0x%I64x), %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
               break;

               case 'a':
                  if (g_TimerRate)
                  {
                     g_TimerRate = 0; 
                  }
                  else
                  {
                     g_TimerRate = 20000;
                  }
                  xlStatus = xlSetTimerRate(g_xlPortHandle, g_TimerRate);
                  printf("- SetTimerRate     : %d, %s\n", g_TimerRate, xlGetErrorString(xlStatus));
               break;

               case 'o':
                  switch(outputMode)
                  {
                     case XL_OUTPUT_MODE_NORMAL:
                        outputMode = XL_OUTPUT_MODE_SILENT;
                     break;

                     case XL_OUTPUT_MODE_SILENT:
                        outputMode = XL_OUTPUT_MODE_TX_OFF;
                     break;

                     case XL_OUTPUT_MODE_TX_OFF:
                     default:
                        outputMode = XL_OUTPUT_MODE_NORMAL;
                     break;
                  }
            
                  demoSetOutput(outputMode, xlChanMaskTx);
               break;

               case 'r':
                  xlStatus = xlResetClock(g_xlPortHandle);
                  printf("- ResetClock       : %s\n", xlGetErrorString(xlStatus));
               break;

               case 's':
                  if (activated)
                  {
                     activated = 0;
                  }
                  else
                  {
                     activated = 1;
                  }
                  demoStartStop(activated);
               break;

               case 'p':
                  demoPrintConfig();
               break;

               case 27: // end application
                  stop = 1;
               break;

               case 'h':
                  demoHelp();
               break;

               default:
               break;
               // end switch
            }
         }
      }
   } // end while
  

   if ((XL_SUCCESS != xlStatus) && activated)
   { 
      xlStatus = xlDeactivateChannel(g_xlPortHandle, g_xlChannelMask);
      printf("- DeactivateChannel: CM(0x%I64x), %s\n", g_xlChannelMask, xlGetErrorString(xlStatus));
   } 
   demoCleanUp();

   return(0);
} // end main()


////////////////////////////////////////////////////////////////////////////
// TxThread
// 
//
////////////////////////////////////////////////////////////////////////////
DWORD WINAPI TxThread( LPVOID par ) 
{
   XLstatus      xlStatus = XL_SUCCESS;
   XLevent       xlEvent;
   unsigned int  n = 1;
   unsigned int  TxID = *(unsigned int*) par;
   static unsigned char loop = 0;

   memset(&xlEvent, 0, sizeof(xlEvent));

   xlEvent.tag                 = XL_TRANSMIT_MSG;
   xlEvent.tagData.msg.id      = TxID;
   xlEvent.tagData.msg.dlc     = 8;
   xlEvent.tagData.msg.flags   = 0;
   xlEvent.tagData.msg.data[0] = 1;
   xlEvent.tagData.msg.data[1] = 2;
   xlEvent.tagData.msg.data[2] = 3;
   xlEvent.tagData.msg.data[3] = 4;
   xlEvent.tagData.msg.data[4] = 5;
   xlEvent.tagData.msg.data[5] = 6;
   xlEvent.tagData.msg.data[6] = 7;
   xlEvent.tagData.msg.data[7] = 8;
   g_TXThreadRun = 1;

   while (g_TXThreadRun && XL_SUCCESS == xlStatus)
   { 
      ++xlEvent.tagData.msg.data[0];
      xlStatus = xlCanTransmit(g_xlPortHandle, g_xlChannelMask, &n, &xlEvent);
    
      Sleep(10);
   }

   if (XL_SUCCESS != xlStatus)
   {
      printf("Error xlCanTransmit:%s\n", xlGetErrorString(xlStatus));
   }

   return NO_ERROR; 
}


///////////////////////////////////////////////////////////////////////////
// RxThread
// thread to readout the message queue and parse the incoming messages
//
////////////////////////////////////////////////////////////////////////////
DWORD WINAPI RxThread(LPVOID par) 
{
   XLstatus        xlStatus;
  
   unsigned int    msgsrx = RECEIVE_EVENT_SIZE;
   XLevent         xlEvent; 
  
   UNUSED_PARAM(par); 
  
   g_RXThreadRun = 1;

   while (g_RXThreadRun)
   { 
      WaitForSingleObject(g_hMsgEvent,10);

      xlStatus = XL_SUCCESS;
    
      while (!xlStatus)
      {
         msgsrx = RECEIVE_EVENT_SIZE;
         xlStatus = xlReceive(g_xlPortHandle, &msgsrx, &xlEvent);

         if (xlStatus != XL_ERR_QUEUE_IS_EMPTY)
         {
            if (!g_silent)
            {
               printf("%s\n", xlGetEventString(&xlEvent));
            }

            //ResetEvent(g_hMsgEvent);
         }  
      }         
   }

   return NO_ERROR; 
}

