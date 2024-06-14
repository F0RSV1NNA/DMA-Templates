#include "kmboxNet.h"
#include "HidTable.h"
#include <time.h>
#include "my_enc.h"
#define monitor_ok    2
#define monitor_exit  0
SOCKET sockClientfd = 0;				// Handle for network communication with keyboard and mouse
SOCKET sockMonitorfd = 0;				// Handle for monitoring network communication
client_tx tx;							// Content to send
client_tx rx;							// Content to receive
SOCKADDR_IN addrSrv;
soft_mouse_t    softmouse;				// Software mouse data
soft_keyboard_t softkeyboard;			// Software keyboard data
static int monitor_run = 0;				// Whether physical keyboard and mouse monitoring is running
static int mask_keyboard_mouse_flag = 0;// Keyboard and mouse shielding status
static short monitor_port = 0;
static unsigned char key[16] = { 0 };	// Encryption key
static HANDLE m_hMutex_lock = NULL;	    // Multithreading mutex lock

#pragma pack(1)
typedef struct {
	unsigned char report_id;
	unsigned char buttons;		// 8 buttons available
	short x;					// -32767 to 32767
	short y;					// -32767 to 32767
	short wheel;				// -32767 to 32767
} standard_mouse_report_t;

typedef struct {
	unsigned char report_id;
	unsigned char buttons;      // 8 control buttons
	unsigned char data[10];     // Regular keys
} standard_keyboard_report_t;
#pragma pack()

standard_mouse_report_t		hw_mouse;   // Hardware mouse messages
standard_keyboard_report_t	hw_keyboard;// Hardware keyboard messages

// Generate a random number between A and B
int myrand(int a, int b)
{
	int min = a < b ? a : b;
	int max = a > b ? a : b;
	if (a == b) return a;
	return ((rand() % (max - min)) + min);
}

unsigned int StrToHex(char* pbSrc, int nLen)
{
	char h1, h2;
	unsigned char s1, s2;
	int i;
	unsigned int pbDest[16] = { 0 };
	for (i = 0; i < nLen; i++) {
		h1 = pbSrc[2 * i];
		h2 = pbSrc[2 * i + 1];
		s1 = toupper(h1) - 0x30;
		if (s1 > 9)
			s1 -= 7;
		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;
		pbDest[i] = s1 * 16 + s2;
	}
	return pbDest[0] << 24 | pbDest[1] << 16 | pbDest[2] << 8 | pbDest[3];
}

int NetRxReturnHandle(client_tx* rx, client_tx* tx)		 // Handle received content
{
	int ret = 0;
	if (rx->head.cmd != tx->head.cmd)
		ret = err_net_cmd; // Command code error
	if (rx->head.indexpts != tx->head.indexpts)
		ret = err_net_pts; // Timestamp error
	ReleaseMutex(m_hMutex_lock);
	return ret;				// Return 0 if no errors
}


/*
Connect to the kmboxNet box. The input parameters are the box's
ip  : The IP address of the box (displayed on the screen, e.g., 192.168.2.88)
port: Communication port number (displayed on the screen, e.g., 6234)
mac : The MAC address of the box (displayed on the screen, e.g., 12345)
Return value: 0 for normal, non-zero for error codes
*/
int kmNet_init(char* ip, char* port, char* mac)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) return err_creat_socket;
	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
		WSACleanup();
		sockClientfd = -1;
		return err_net_version;
	}

	if (m_hMutex_lock == NULL)
	{
		m_hMutex_lock = CreateMutex(NULL, TRUE, L"busy");
	}
	ReleaseMutex(m_hMutex_lock);
	memset(key, 0, 16);
	srand((unsigned)time(NULL));
	sockClientfd = socket(AF_INET, SOCK_DGRAM, 0);
	addrSrv.sin_addr.S_un.S_addr = inet_addr(ip);
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(atoi(port));// Port UUID[1]>>16 high 16 bits
	tx.head.mac = StrToHex(mac, 4);		 // MAC of the box, fixed UUID[1]
	tx.head.rand = rand();				 // Random value. Can be used for network packet encryption to avoid features. Reserved for future use.
	tx.head.indexpts = 0;				 // Command statistics value
	tx.head.cmd = cmd_connect;			 // Command
	memset(&softmouse, 0, sizeof(softmouse));	// Clear software mouse data
	memset(&softkeyboard, 0, sizeof(softkeyboard));// Clear software keyboard data
	key[0] = tx.head.mac >> 24; key[1] = tx.head.mac >> 16; // Set encryption key
	key[2] = tx.head.mac >> 8; key[3] = tx.head.mac >> 0;   // Set encryption key
	err = sendto(sockClientfd, (const char*)&tx, sizeof(cmd_head_t), 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	Sleep(20); // First connection may take longer
	int clen = sizeof(addrSrv);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&addrSrv, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Move the mouse by x,y units. Move once without trajectory simulation, fastest speed.
Use this function when writing the trajectory movement yourself.
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_move(short x, short y)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_move;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	softmouse.x = 0;
	softmouse.y = 0;
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Control with encryption
Move the mouse by x,y units. Move once without trajectory simulation, fastest speed.
Use this function when writing the trajectory movement yourself.
Return value: 0 for normal execution, other values for exceptions.
This function has encryption capability, ensuring that the same move command results in different network packet content, preventing box capture by network sniffing.
*/
int kmNet_enc_mouse_move(short x, short y)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_move;	//
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);														 // Encrypt sending data
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv)); //
	softmouse.x = 0;
	softmouse.y = 0;
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0; // As long as a response is received, consider sending OK, reducing box encryption process time
}

/*
Mouse left button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_left(int isdown)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_left;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x01) : (softmouse.button & (~0x01)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse left button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_enc_mouse_left(int isdown)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_left;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x01) : (softmouse.button & (~0x01)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Mouse middle button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_middle(int isdown)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_middle;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x04) : (softmouse.button & (~0x04)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse middle button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_enc_mouse_middle(int isdown)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_middle;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x04) : (softmouse.button & (~0x04)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Mouse right button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_right(int isdown)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x02) : (softmouse.button & (~0x02)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse right button control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_enc_mouse_right(int isdown)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x02) : (softmouse.button & (~0x02)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Mouse side button 1 control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_side1(int isdown)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x08) : (softmouse.button & (~0x08)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse side button 1 control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_enc_mouse_side1(int isdown)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x08) : (softmouse.button & (~0x08)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Mouse side button 2 control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_mouse_side2(int isdown)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x10) : (softmouse.button & (~0x10)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse side button 2 control
isdown :0 release, 1 press
Return value: 0 for normal execution, other values for exceptions.
*/
int kmNet_enc_mouse_side2(int isdown)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_right;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = (isdown ? (softmouse.button | 0x10) : (softmouse.button & (~0x10)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

// Mouse wheel control
int kmNet_mouse_wheel(int wheel)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_wheel;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.wheel = wheel;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.wheel = 0;
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Mouse wheel control
int kmNet_enc_mouse_wheel(int wheel)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_wheel;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.wheel = wheel;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.wheel = 0;
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Mouse full report control function
*/
int kmNet_mouse_all(int button, int x, int y, int wheel)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_wheel;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = button;
	softmouse.x = x;
	softmouse.y = y;
	softmouse.wheel = wheel;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;
	softmouse.y = 0;
	softmouse.wheel = 0;
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse full report control function
*/
int kmNet_enc_mouse_all(int button, int x, int y, int wheel)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_mouse_wheel;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	softmouse.button = button;
	softmouse.x = x;
	softmouse.y = y;
	softmouse.wheel = wheel;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;
	softmouse.y = 0;
	softmouse.wheel = 0;
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Move the mouse by x,y units. Simulate moving x,y units as if by a human. No abnormal keyboard and mouse detection.
Recommended to use this function if you haven't written the movement curve. This function will not cause jumps and approaches the target point with the smallest step. Takes more time than kmNet_mouse_move.
ms sets how many milliseconds the move should take. Note that ms should not be too small, or keyboard and mouse data abnormalities may occur.
Try to simulate human operation. Actual time will be less than ms.
*/
int kmNet_mouse_move_auto(int x, int y, int ms)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				 // Command statistics value
	tx.head.cmd = cmd_mouse_automove;// Command
	tx.head.rand = ms;			     // Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;				// Clear
	softmouse.y = 0;				// Clear
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Move the mouse by x,y units. Simulate moving x,y units as if by a human. No abnormal keyboard and mouse detection.
Recommended to use this function if you haven't written the movement curve. This function will not cause jumps and approaches the target point with the smallest step. Takes more time than kmNet_mouse_move.
ms sets how many milliseconds the move should take. Note that ms should not be too small, or keyboard and mouse data abnormalities may occur.
Try to simulate human operation. Actual time will be less than ms.
*/
int kmNet_enc_mouse_move_auto(int x, int y, int ms)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				 // Command statistics value
	tx.head.cmd = cmd_mouse_automove;// Command
	tx.head.rand = ms;			     // Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;				// Clear
	softmouse.y = 0;				// Clear
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

/*
Second-order Bezier curve control
x,y : Target point coordinates
ms  : Time taken for this process (in ms)
x1,y1 : Control point p1 coordinates
x2,y2 : Control point p2 coordinates
*/
int kmNet_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;			 // Command statistics value
	tx.head.cmd = cmd_bazerMove; // Command
	tx.head.rand = ms;			 // Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	softmouse.point[0] = x1;
	softmouse.point[1] = y1;
	softmouse.point[2] = x2;
	softmouse.point[3] = y2;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;
	softmouse.y = 0;
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

/*
Second-order Bezier curve control
x,y : Target point coordinates
ms  : Time taken for this process (in ms)
x1,y1 : Control point p1 coordinates
x2,y2 : Control point p2 coordinates
*/
int kmNet_enc_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;			 // Command statistics value
	tx.head.cmd = cmd_bazerMove; // Command
	tx.head.rand = ms;			 // Random obfuscation value
	softmouse.x = x;
	softmouse.y = y;
	softmouse.point[0] = x1;
	softmouse.point[1] = y1;
	softmouse.point[2] = x2;
	softmouse.point[3] = y2;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;
	softmouse.y = 0;
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

int kmNet_keydown(int vk_key)
{
	int i;
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl |= BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl |= BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl |= BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl |= BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl |= BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl |= BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl |= BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl |= BIT7; break;
		}
	}
	else
	{// Regular keys  
		for (i = 0; i < 10; i++) // First check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key)
				goto KM_down_send; // If vk_key is already in the queue, just send
		}
		// If vk_key is not in the queue 
		for (i = 0; i < 10; i++) // Traverse all data and add vk_key to the queue
		{
			if (softkeyboard.button[i] == 0)
			{// If vk_key is already in the queue, just send
				softkeyboard.button[i] = vk_key;
				goto KM_down_send;
			}
		}
		// If the queue is full, remove the first one
		memcpy(&softkeyboard.button[0], &softkeyboard.button[1], 9);
		softkeyboard.button[9] = vk_key;
	}
KM_down_send:
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_keyboard_all;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

int kmNet_enc_keydown(int vk_key)
{
	int i;
	client_tx tx_enc = { 0 };
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl |= BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl |= BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl |= BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl |= BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl |= BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl |= BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl |= BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl |= BIT7; break;
		}
	}
	else
	{// Regular keys  
		for (i = 0; i < 10; i++) // First check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key)
				goto KM_enc_down_send; // If vk_key is already in the queue, just send
		}
		// If vk_key is not in the queue 
		for (i = 0; i < 10; i++) // Traverse all data and add vk_key to the queue
		{
			if (softkeyboard.button[i] == 0)
			{// If vk_key is already in the queue, just send
				softkeyboard.button[i] = vk_key;
				goto KM_enc_down_send;
			}
		}
		// If the queue is full, remove the first one
		memcpy(&softkeyboard.button[0], &softkeyboard.button[1], 9);
		softkeyboard.button[9] = vk_key;
	}
KM_enc_down_send:

	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_keyboard_all;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

// Keyboard key release
int kmNet_keyup(int vk_key)
{
	int i;
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl &= ~BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl &= ~BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl &= ~BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl &= ~BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl &= ~BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl &= ~BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl &= ~BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl &= ~BIT7; break;
		}
	}
	else
	{// Regular keys  
		for (i = 0; i < 10; i++) // First check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key) // If vk_key is already in the queue
			{
				memcpy(&softkeyboard.button[i], &softkeyboard.button[i + 1], 9 - i);
				softkeyboard.button[9] = 0;
				goto KM_up_send;
			}
		}
	}
KM_up_send:
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_keyboard_all;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Keyboard key release
int kmNet_enc_keyup(int vk_key)
{
	int i;
	client_tx tx_enc = { 0 };
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl &= ~BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl &= ~BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl &= ~BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl &= ~BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl &= ~BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl &= ~BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl &= ~BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl &= ~BIT7; break;
		}
	}
	else
	{// Regular keys  
		for (i = 0; i < 10; i++) // First check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key) // If vk_key is already in the queue
			{
				memcpy(&softkeyboard.button[i], &softkeyboard.button[i + 1], 9 - i);
				softkeyboard.button[9] = 0;
				goto KM_enc_up_send;
			}
		}
	}
KM_enc_up_send:

	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_keyboard_all;	// Command
	tx.head.rand = rand();			// Random obfuscation value
	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));
	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

// Press specified key
int kmNet_keypress(int vk_key, int ms)
{
	kmNet_keydown(vk_key);
	Sleep(ms / 2);
	kmNet_keyup(vk_key);
	Sleep(ms / 2);
	return 0;
}

// Press specified key
int kmNet_enc_keypress(int vk_key, int ms)
{
	kmNet_enc_keydown(vk_key);
	Sleep(ms / 2);
	kmNet_enc_keyup(vk_key);
	Sleep(ms / 2);
	return 0;
}

// Restart the box
int kmNet_reboot(void)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_reboot;		// Command
	tx.head.rand = rand();			// Random obfuscation value
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	WSACleanup();
	sockClientfd = -1;
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

int kmNet_enc_reboot(void)
{
	int err;
	client_tx tx_enc = { 0 };
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_reboot;		// Command
	tx.head.rand = rand();			// Random obfuscation value
	int length = sizeof(cmd_head_t);
	memcpy(&tx_enc, &tx, length);
	my_encrypt((unsigned char*)&tx_enc, key);
	sendto(sockClientfd, (const char*)&tx_enc, 128, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	WSACleanup();
	sockClientfd = -1;
	ReleaseMutex(m_hMutex_lock);
	return 0;
}

// Monitor physical keyboard and mouse. If unable to monitor, please disable the firewall
DWORD WINAPI ThreadListenProcess(LPVOID lpParameter)
{
	WSADATA wsaData;
	int ret;
	WSAStartup(MAKEWORD(1, 1), &wsaData);			// Create a socket, SOCK_DGRAM indicates using UDP protocol
	sockMonitorfd = socket(AF_INET, SOCK_DGRAM, 0);	// Bind the socket
	sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));			// Fill each byte with 0
	servAddr.sin_family = PF_INET;					// Use IPv4 address
	servAddr.sin_addr.s_addr = INADDR_ANY;	        // Automatically obtain IP address
	servAddr.sin_port = htons(monitor_port);		// Monitor port
	ret = bind(sockMonitorfd, (SOCKADDR*)&servAddr, sizeof(SOCKADDR));
	SOCKADDR cliAddr;  // Client address information
	int nSize = sizeof(SOCKADDR);
	char buff[1024];  // Buffer
	monitor_run = monitor_ok;
	while (1)
	{
		int ret = recvfrom(sockMonitorfd, buff, 1024, 0, &cliAddr, &nSize);	// Block reading data
		if (ret > 0)
		{
			memcpy(&hw_mouse, buff, sizeof(hw_mouse));							// Physical mouse status
			memcpy(&hw_keyboard, &buff[sizeof(hw_mouse)], sizeof(hw_keyboard));	// Physical keyboard status
		}
		else
		{
			break;
		}
	}
	monitor_run = 0;
	sockMonitorfd = 0;
	return 0;
}

// Enable keyboard and mouse monitoring. Port number range is 1024 to 49151
int kmNet_monitor(short port)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_monitor;		// Command
	if (port) {
		monitor_port = port;				// Port to monitor physical keyboard and mouse data
		tx.head.rand = port | 0xaa55 << 16;	// Random obfuscation value
	}
	else
		tx.head.rand = 0;	// Random obfuscation value
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (sockMonitorfd > 0)	// Close monitoring
	{
		closesocket(sockMonitorfd);
		sockMonitorfd = 0;
		Sleep(100); // Give some time for the thread to run first
	}
	if (port)
	{
		CreateThread(NULL, 0, ThreadListenProcess, NULL, 0, NULL);
	}
	return NetRxReturnHandle(&rx, &tx);
}

/*
Monitor the status of the physical mouse left button
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Physical mouse left button released
1 : Physical mouse left button pressed
*/
int kmNet_monitor_mouse_left()
{
	if (monitor_run != monitor_ok) return -1;
	return (hw_mouse.buttons & 0x01) ? 1 : 0;
}

/*
// Monitor the status of the physical mouse middle button
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Physical mouse middle button released
1 : Physical mouse middle button pressed
*/
int kmNet_monitor_mouse_middle()
{
	if (monitor_run != monitor_ok) return -1;
	return (hw_mouse.buttons & 0x04) ? 1 : 0;
}

/*
// Monitor the status of the physical mouse right button
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Physical mouse right button released
1 : Physical mouse right button pressed
*/
int kmNet_monitor_mouse_right()
{
	if (monitor_run != monitor_ok) return -1;
	return (hw_mouse.buttons & 0x02) ? 1 : 0;
}

/*
// Monitor the status of the physical mouse side button 1
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Physical mouse side button 1 released
1 : Physical mouse side button 1 pressed
*/
int kmNet_monitor_mouse_side1()
{
	if (monitor_run != monitor_ok) return -1;
	return (hw_mouse.buttons & 0x08) ? 1 : 0;
}

/*
// Monitor the status of the physical mouse side button 2
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Physical mouse side button 2 released
1 : Physical mouse side button 2 pressed
*/
int kmNet_monitor_mouse_side2()
{
	if (monitor_run != monitor_ok) return -1;
	return (hw_mouse.buttons & 0x10) ? 1 : 0;
}

/*
Monitor the XY coordinates of the physical mouse
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : XY coordinates have not changed
1 : XY coordinates have changed
*/
int kmNet_monitor_mouse_xy(int* x, int* y)
{
	static int lastx, lasty;
	if (monitor_run != monitor_ok) return -1;
	*x = hw_mouse.x;
	*y = hw_mouse.y;
	if (*x != lastx || *y != lasty)
	{
		lastx = *x;
		lasty = *y;
		return 1;
	}
	return 0;
}

/*
Monitor the wheel coordinates of the physical mouse
Return values
-1: Monitoring function not enabled yet. Call kmNet_monitor(1) first.
0 : Wheel coordinates have not changed
1 : Wheel coordinates have changed
*/
int kmNet_monitor_mouse_wheel(int* wheel)
{
	static int lastwheel;
	if (monitor_run != monitor_ok) return -1;
	*wheel = hw_mouse.wheel;
	if (*wheel != lastwheel)
	{
		lastwheel = *wheel;
		return 1;
	}
	return 0;
}

// Monitor the status of specified keyboard key
int kmNet_monitor_keyboard(short vkey)
{
	unsigned char vk_key = vkey & 0xff;
	if (monitor_run != monitor_ok) return -1;
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: return  hw_keyboard.buttons & BIT0 ? 1 : 0;
		case KEY_LEFTSHIFT:   return  hw_keyboard.buttons & BIT1 ? 1 : 0;
		case KEY_LEFTALT:     return  hw_keyboard.buttons & BIT2 ? 1 : 0;
		case KEY_LEFT_GUI:    return  hw_keyboard.buttons & BIT3 ? 1 : 0;
		case KEY_RIGHTCONTROL:return  hw_keyboard.buttons & BIT4 ? 1 : 0;
		case KEY_RIGHTSHIFT:  return  hw_keyboard.buttons & BIT5 ? 1 : 0;
		case KEY_RIGHTALT:    return  hw_keyboard.buttons & BIT6 ? 1 : 0;
		case KEY_RIGHT_GUI:   return  hw_keyboard.buttons & BIT7 ? 1 : 0;
		}
	}
	else // Regular keys
	{
		for (int i = 0; i < 10; i++)
		{
			if (hw_keyboard.data[i] == vk_key)
			{
				return 1;
			}
		}
	}
	return 0;
}

// Shield mouse left button 
int kmNet_mask_mouse_left(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT0) : (mask_keyboard_mouse_flag &= ~BIT0);	// Shield mouse left button
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse right button 
int kmNet_mask_mouse_right(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT1) : (mask_keyboard_mouse_flag &= ~BIT1);	// Shield mouse right button
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse middle button 
int kmNet_mask_mouse_middle(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT2) : (mask_keyboard_mouse_flag &= ~BIT2);	// Shield mouse middle button
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse side button 1 
int kmNet_mask_mouse_side1(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT3) : (mask_keyboard_mouse_flag &= ~BIT3);	// Shield mouse side button 1
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse side button 2
int kmNet_mask_mouse_side2(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT4) : (mask_keyboard_mouse_flag &= ~BIT4);	// Shield mouse side button 2
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse X-axis coordinates
int kmNet_mask_mouse_x(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT5) : (mask_keyboard_mouse_flag &= ~BIT5);	// Shield mouse X-axis coordinates
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse Y-axis coordinates
int kmNet_mask_mouse_y(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT6) : (mask_keyboard_mouse_flag &= ~BIT6);	// Shield mouse Y-axis coordinates
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield mouse wheel
int kmNet_mask_mouse_wheel(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT7) : (mask_keyboard_mouse_flag &= ~BIT7);	// Shield mouse wheel
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Shield specified keyboard key
int kmNet_mask_keyboard(short vkey)
{
	int err;
	BYTE v_key = vkey & 0xff;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_mask_mouse;		// Command
	tx.head.rand = (mask_keyboard_mouse_flag & 0xff) | (v_key << 8);	// Shield keyboard vkey
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Unshield specified keyboard key
int kmNet_unmask_keyboard(short vkey)
{
	int err;
	BYTE v_key = vkey & 0xff;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_unmask_all;		// Command
	tx.head.rand = (mask_keyboard_mouse_flag & 0xff) | (v_key << 8);	// Unshield keyboard vkey
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Unshield all physical shields that have been set
int kmNet_unmask_all()
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_unmask_all;		// Command
	mask_keyboard_mouse_flag = 0;
	tx.head.rand = mask_keyboard_mouse_flag;
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Set configuration information, change IP and port number
int kmNet_setconfig(char* ip, unsigned short port)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_setconfig;		// Command
	tx.head.rand = inet_addr(ip);
	tx.u8buff.buff[0] = port >> 8;
	tx.u8buff.buff[1] = port;
	int length = sizeof(cmd_head_t) + 2;
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Set the VIDPID of the box device side
int kmNet_setvidpid(unsigned short vid, unsigned short pid)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;					// Command statistics value
	tx.head.cmd = cmd_setvidpid;		// Command
	tx.head.rand = vid | pid << 16;
	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}

// Fill the entire LCD screen with a specified color. Use black to clear the screen.
int kmNet_lcd_color(unsigned short rgb565)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	for (int y = 0; y < 40; y++)
	{
		tx.head.indexpts++;		    // Command statistics value
		tx.head.cmd = cmd_showpic;	// Command
		tx.head.rand = 0 | y * 4;
		for (int c = 0; c < 512; c++)
			tx.u16buff.buff[c] = rgb565;
		int length = sizeof(cmd_head_t) + 1024;
		sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
		SOCKADDR_IN sclient;
		int clen = sizeof(sclient);
		err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);
	}
	return NetRxReturnHandle(&rx, &tx);
}

// Display a 128x80 image at the bottom
int kmNet_lcd_picture_bottom(unsigned char* buff_128_80)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	for (int y = 0; y < 20; y++)
	{
		tx.head.indexpts++;		    // Command statistics value
		tx.head.cmd = cmd_showpic;	// Command
		tx.head.rand = 80 + y * 4;
		memcpy(tx.u8buff.buff, &buff_128_80[y * 1024], 1024);
		int length = sizeof(cmd_head_t) + 1024;
		sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
		SOCKADDR_IN sclient;
		int clen = sizeof(sclient);
		err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);
	}
	return NetRxReturnHandle(&rx, &tx);
}

// Display a 128x160 image at the bottom
int kmNet_lcd_picture(unsigned char* buff_128_160)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	for (int y = 0; y < 40; y++)
	{
		tx.head.indexpts++;		    // Command statistics value
		tx.head.cmd = cmd_showpic;	// Command
		tx.head.rand = y * 4;
		memcpy(tx.u8buff.buff, &buff_128_160[y * 1024], 1024);
		int length = sizeof(cmd_head_t) + 1024;
		sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
		SOCKADDR_IN sclient;
		int clen = sizeof(sclient);
		err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);
	}
	return NetRxReturnHandle(&rx, &tx);
}

// Enable the hardware correction function of the box
/*
type  :0: Bezier curve 1: Missile tracking curve, 2: Bezier real-time, 3: RM-RT
value : Less than or equal to 0 means to disable the hardware curve correction function. Greater than 0 means to enable the hardware curve correction function. The larger the value, the smoother the curve. But the higher the time consumption. The recommended value range is between 16 and 50. It can be up to 100.
*/
int kmNet_Trace(int type, int value)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;
	WaitForSingleObject(m_hMutex_lock, INFINITE);
	tx.head.indexpts++;				// Command statistics value
	tx.head.cmd = cmd_trace_enable;	// Command
	tx.head.rand = type << 24 | value;			// Span value
	sendto(sockClientfd, (const char*)&tx, sizeof(cmd_head_t), 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	return NetRxReturnHandle(&rx, &tx);
}
