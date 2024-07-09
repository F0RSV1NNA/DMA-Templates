#include <stdio.h>
#include <Winsock2.h>
#include "math.h"
#pragma warning(disable : 4996)
// Command codes
#define 	cmd_connect			0xaf3c2828 // ok connect to the box
#define     cmd_mouse_move		0xaede7345 // ok mouse move
#define		cmd_mouse_left		0x9823AE8D // ok mouse left button control
#define		cmd_mouse_middle	0x97a3AE8D // ok mouse middle button control
#define		cmd_mouse_right		0x238d8212 // ok mouse right button control
#define		cmd_mouse_wheel		0xffeead38 // ok mouse wheel control
#define     cmd_mouse_automove	0xaede7346 // ok mouse automatic simulated human movement control
#define     cmd_keyboard_all    0x123c2c2f // ok keyboard all parameter control
#define		cmd_reboot			0xaa8855aa // ok box reboot
#define     cmd_bazerMove       0xa238455a // ok mouse Bezier move
#define     cmd_monitor         0x27388020 // ok monitor physical keyboard and mouse data on the box
#define     cmd_debug           0x27382021 // ok enable debug information
#define     cmd_mask_mouse      0x23234343 // ok shield physical keyboard and mouse
#define     cmd_unmask_all      0x23344343 // ok unshield physical keyboard and mouse
#define     cmd_setconfig       0x1d3d3323 // ok set IP configuration information
#define     cmd_setvidpid       0xffed3232 // ok set device VIDPID
#define     cmd_showpic         0x12334883 // display picture
#define     cmd_trace_enable    0xbbcdddac // enable hardware correction function

extern SOCKET sockClientfd; // socket communication handle
typedef struct
{
	unsigned int  mac;			// MAC address of the box (mandatory)
	unsigned int  rand;			// random value
	unsigned int  indexpts;		// timestamp
	unsigned int  cmd;			// command code
} cmd_head_t;

typedef struct
{
	unsigned char buff[1024];	// buffer
} cmd_data_t;

typedef struct
{
	unsigned short buff[512];	// U16 buffer
} cmd_u16_t;

// Mouse data structure
typedef struct
{
	int button;
	int x;
	int y;
	int wheel;
	int point[10]; // for Bezier curve control (reserved for 5th order)
} soft_mouse_t;

// Keyboard data structure
typedef struct
{
	char ctrl;
	char resvel;
	char button[10];
} soft_keyboard_t;

// Union
typedef struct
{
	cmd_head_t head;
	union {
		cmd_data_t      u8buff;		  // buffer
		cmd_u16_t       u16buff;	  // U16
		soft_mouse_t    cmd_mouse;    // mouse command
		soft_keyboard_t cmd_keyboard; // keyboard command
	};
} client_tx;

enum
{
	err_creat_socket = -9000,	// failed to create socket
	err_net_version,			// socket version error
	err_net_tx,					// socket send error
	err_net_rx_timeout,			// socket receive timeout
	err_net_cmd,				// command error
	err_net_pts,				// timestamp error
	success = 0,				// normal execution
	usb_dev_tx_timeout,			// USB device send failure
};

/*
Connect to kmboxNet box. The input parameters are the IP address, port number, and MAC address of the box.
ip  : IP address of the box (displayed on the screen)
port: communication port number (displayed on the screen)
mac : MAC address of the box (displayed on the screen)
Return value: 0 for successful connection, see error codes for other values
*/
int kmNet_init(char* ip, char* port, char* mac); // ok
int kmNet_mouse_move(short x, short y);			// ok
int kmNet_mouse_left(int isdown);				// ok
int kmNet_mouse_right(int isdown);				// ok
int kmNet_mouse_middle(int isdown);				// ok
int kmNet_mouse_wheel(int wheel);				// ok
int kmNet_mouse_side1(int isdown);				// ok
int kmNet_mouse_side2(int isdown);				// ok
int kmNet_mouse_all(int button, int x, int y, int wheel); // ok
int kmNet_mouse_move_auto(int x, int y, int time_ms);	// ok
int kmNet_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2); // second order curve
// Keyboard functions
int kmNet_keydown(int vkey); // ok
int kmNet_keyup(int vkey);   // ok
int kmNet_keypress(int vk_key, int ms = 10); // ok
int kmNet_enc_keydown(int vkey); // ok
int kmNet_enc_keyup(int vkey);   // ok
int kmNet_enc_keypress(int vk_key, int ms = 10); // ok

// Functions with encryption. Recommended for single machine use to prevent network data packet capture features
int kmNet_enc_mouse_move(short x, short y);	    // encrypted mouse move ok
int kmNet_enc_mouse_left(int isdown);			// ok
int kmNet_enc_mouse_right(int isdown);			// ok
int kmNet_enc_mouse_middle(int isdown);			// ok
int kmNet_enc_mouse_wheel(int wheel);			// ok
int kmNet_enc_mouse_side1(int isdown);			// ok
int kmNet_enc_mouse_side2(int isdown);			// ok
int kmNet_enc_mouse_all(int button, int x, int y, int wheel); // ok
int kmNet_enc_mouse_move_auto(int x, int y, int time_ms);	// ok
int kmNet_enc_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2); // second order curve
int kmNet_enc_keydown(int vkey); // ok
int kmNet_enc_keyup(int vkey);   // ok

// Monitoring series
int kmNet_monitor(short port);			// enable/disable physical keyboard and mouse monitoring
int kmNet_monitor_mouse_left();			// query physical mouse left button status
int kmNet_monitor_mouse_middle();		// query mouse middle button status
int kmNet_monitor_mouse_right();		// query mouse right button status
int kmNet_monitor_mouse_side1();		// query mouse side button 1 status
int kmNet_monitor_mouse_side2();		// query mouse side button 2 status 
int kmNet_monitor_mouse_xy(int* x, int* y); // query mouse XY coordinates
int kmNet_monitor_mouse_wheel(int* wheel); // query mouse wheel value
int kmNet_monitor_keyboard(short vk_key); // query specified keyboard key status
// Physical keyboard and mouse shielding series
int kmNet_mask_mouse_left(int enable);	// shield mouse left button 
int kmNet_mask_mouse_right(int enable);	// shield mouse right button 
int kmNet_mask_mouse_middle(int enable); // shield mouse middle button 
int kmNet_mask_mouse_side1(int enable);	// shield mouse side button 1 
int kmNet_mask_mouse_side2(int enable);	// shield mouse side button 2
int kmNet_mask_mouse_x(int enable);		// shield mouse X-axis coordinates
int kmNet_mask_mouse_y(int enable);		// shield mouse Y-axis coordinates
int kmNet_mask_mouse_wheel(int enable);	// shield mouse wheel
int kmNet_mask_keyboard(short vkey);	// shield specified keyboard key
int kmNet_unmask_keyboard(short vkey);	// unshield specified keyboard key
int kmNet_unmask_all();					// unshield all previously set physical shields

// Configuration functions
int kmNet_reboot(void);									  // reboot the box
int kmNet_setconfig(char* ip, unsigned short port);		  // configure box IP address
int kmNet_setvidpid(unsigned short vid, unsigned short pid); // set box VIDPID, needs reboot to take effect
int kmNet_debug(short port, char enable);				  // enable debugging
int kmNet_lcd_color(unsigned short rgb565);				  // fill the entire LCD screen with the specified color. Use black to clear the screen
int kmNet_lcd_picture_bottom(unsigned char* buff_128_80); // display 128x80 picture on the bottom half
int kmNet_lcd_picture(unsigned char* buff_128_160);		  // display 128x160 picture on the whole screen
