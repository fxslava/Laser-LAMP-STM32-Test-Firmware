#ifndef WIFIDRIVER_H
#define WIFIDRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"

#define WIFI_EVENT_RECEIVE_COMPLETED	0x004
#define WIFI_EVENT_SEND_COMPLETED			0x008
#define WIFI_EVENT_RECEIVE_TIMEOUT		0x010

#define WIFI_EVENT_RECEIVE_STRING			0x020
#define WIFI_EVENT_RECEIVE_WIND				0x040
#define WIFI_EVENT_TEMPERATURE_UPDATE	0x080

#define WIFI_EVENT_REMOTECONTROL			0x100

#define WIFI_EVENT_SCANNINGCOMPLETE		0x200
#define WIFI_EVENT_LINKCOMPLETE				0x400

// WiFi CMD
#define WIFI_CMD_STARTSCANNING				1
#define WIFI_CMD_STARTLINKING					2

#define FRAME_SIZE	64
#define BUFFER_SIZE 32768
#define BUFFER_MASK 0x7ff

// WiFi AP scanning strucrure
typedef struct WIFI_AP_STRUCT
{
	uint16_t	Channel;
	char			SSID[32];
	int16_t		RSSI;
	bool wpa2;
	bool wps;
	uint16_t	live;
} WIFI_AP, *PWIFI_AP;

// WiFi AP scanning buffer
extern PWIFI_AP WiFi_APs[16];

// WiFi global state variables
extern bool WiFi_State_CommandMode;
extern bool WiFi_State_ScanningMode;
//extern bool WiFi_ConnectionEstebilished[8];
extern bool WiFi_SocketClosed[8];
extern bool WiFi_SocketConnected[8];
extern bool WiFi_OK_Received;
extern bool WiFi_ERROR_Received;
extern bool WiFi_PendingData;

// WiFi pending data size
extern uint16_t WiFi_PendingDataSize;
extern uint16_t WIFi_ConnectionTimeout;
extern uint16_t WIFi_RequestTimeout;

// WiFi Thread parsing buffers
extern char* tokenPtr[32];
extern char  buffer_rx[256];

// WiFi API
bool SendAT(char* str);
bool WaitOK(uint32_t timeout);
void AsyncSendAT(char* str);
char* GetResponsePtr(void);
int16_t GetID(uint32_t timeout); // Socket ID
int16_t WaitForWINDCommands(uint16_t timeout, uint16_t argc, ...);

// Socket
int16_t	socket_connect(char* name, uint16_t port);
void		socket_write	(uint16_t id, char* buffer, uint16_t len);

// Initialize WiFi
int Init_WiFiDriver_Thread (osThreadId userThread);

#endif
