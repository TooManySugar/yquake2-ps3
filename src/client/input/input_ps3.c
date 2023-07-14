/*
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 1997-2005 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Joystick threshold code is partially based on http://ioquake3.org code.
 *
 * =======================================================================
 *
 * This is the Quake II input system backend for DualShock 3 on PS3.
 *
 * =======================================================================
 * 
 * Notes on PS3 changes:
 *     Native input system for DualShock 3. Works w/o SDL.
 *     It may be possible to configure to use the keyboard and mouse too.
 * 
 * -----------------------------------------------------------------------
 */

#include <io/pad.h>
#include <sys/memory.h>
#include <sysutil/sysutil.h>
#include <sysutil/osk.h>

#include "header/input.h"
#include "../../client/header/keyboard.h"
#include "../../client/header/client.h"

// ----

// Maximal mouse move per frame
#define MOUSE_MAX 3000

// Minimal mouse move per frame
#define MOUSE_MIN 40

// ----

// These are used to communicate the events collected by
// IN_Update() called at the beginning of a frame to the
// actual movement functions called at a later time.
static float joystick_yaw, joystick_pitch;
static float joystick_forwardmove, joystick_sidemove;
static float joystick_up;

// The last time input events were processed.
// Used throughout the client.
int sys_frame_time;

// the joystick altselector that turns K_JOYX into K_JOYX_ALT
// is pressed
qboolean joy_altselector_pressed = false;

// Console Variables
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_up;
cvar_t *m_yaw;
cvar_t *sensitivity;

static cvar_t *exponential_speedup;
static cvar_t *in_grab;
static cvar_t *m_filter;

// ----

struct hapric_effects_cache {
	int effect_volume;
	int effect_duration;
	int effect_begin;
	int effect_end;
	int effect_attack;
	int effect_fade;
	int effect_id;
	int effect_x;
	int effect_y;
	int effect_z;
};

qboolean show_haptic;

// Joystick sensitivity
cvar_t *joy_yawsensitivity;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_forwardsensitivity;
cvar_t *joy_sidesensitivity;
cvar_t *joy_upsensitivity;
cvar_t *joy_expo;

// Joystick direction settings
static cvar_t *joy_axis_leftx;
static cvar_t *joy_axis_lefty;
static cvar_t *joy_axis_rightx;
static cvar_t *joy_axis_righty;
static cvar_t *joy_axis_triggerleft;
static cvar_t *joy_axis_triggerright;

// Joystick threshold settings
static cvar_t *joy_axis_leftx_threshold;
static cvar_t *joy_axis_lefty_threshold;
static cvar_t *joy_axis_rightx_threshold;
static cvar_t *joy_axis_righty_threshold;
static cvar_t *joy_axis_triggerleft_threshold;
static cvar_t *joy_axis_triggerright_threshold;

// Joystick haptic
static cvar_t *joy_haptic_magnitude;

/* ------------------------------------------------------------------ */
// OSK stuff

static sys_mem_container_t osk_containerid;

#define TEXT_BUFFER_LENGTH 256

// Buffer for OSK title
static uint16_t osk_message[TEXT_BUFFER_LENGTH];

// Buffer for OSK initial text
static uint16_t osk_startText[TEXT_BUFFER_LENGTH];

// Buffer for OSK entered text
static uint16_t osk_outText[TEXT_BUFFER_LENGTH];

oskInputFieldInfo osk_inputFieldInfo;
oskParam osk_parameters;
oskCallbackReturnParam osk_outputParam;

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */

static void
IN_xmbOpenOSK_f(void)
{
	int32_t res;

	res = oskLoadAsync(osk_containerid, &osk_parameters, &osk_inputFieldInfo);

	if (res != 0)
	{
		Com_Printf("Error oskLoadAsync: %08x\n", res);
	}
}

static inline void
IN_adjustJoyDirection(const unsigned int rawAxisValue, const char* direction, const float _threshold)
{
	// Com_Printf("IN_adjustJoyDirection rawAxisValue: 0x%04X direction: %s threshold: %0.4f\n", rawAxisValue, direction, _threshold);
	int axis_value; // [-128, 127]
	//Com_Printf("IN_adjustJoyDirection base axis_value: %d\n", axis_value);
	float fix_value;
	float threshold;
	if (_threshold > 0.9)
	{
		threshold = 0.9;
	}
	else
	{
		threshold = _threshold;
	}
	// Com_Printf("IN_adjustJoyDirection threshold: %0.04f\n", threshold);


	float axe_range_value = rawAxisValue;
	axe_range_value  /= 255.0f;
	axe_range_value -= 0.5f;
	axe_range_value /= 0.5f;
	axe_range_value = axe_range_value > 0.0f ? axe_range_value : -axe_range_value;

	// Smoothly ramp from dead zone to maximum value (from ioquake)
	// https://github.com/ioquake/ioq3/blob/master/code/sdl/sdl_input.c
	// fix_value = ((float) abs(rawAxisValue) / 255.0f - threshold) / (1.0f - threshold);
	fix_value = (axe_range_value - threshold) / (1.0f - threshold);
	// Com_Printf("IN_adjustJoyDirection base fix_value: %0.04f\n", fix_value);

	if (fix_value < 0.0f)
	{
		fix_value = 0.0f;
	}
	// Com_Printf("IN_adjustJoyDirection adjust fix_value: %0.04f\n", fix_value);

	// Apply expo
	fix_value = pow(fix_value, joy_expo->value);
	// Com_Printf("IN_adjustJoyDirection final fix_value: %0.04f\n", fix_value);

	axis_value = (int) (32767 * ((rawAxisValue < 0x7F) ? -fix_value : fix_value));
	// Com_Printf("IN_adjustJoyDirection final axis_value: %d\n", axis_value);

	if (cls.key_dest == key_game && (int) cl_paused->value == 0)
	{
		     if (strcmp(direction, "sidemove") == 0)
		{
			joystick_sidemove = axis_value * joy_sidesensitivity->value;

			// We need to be twice faster because with joystic we run...
			joystick_sidemove *= cl_sidespeed->value * 2.0f;
		}
		else if (strcmp(direction, "forwardmove") == 0)
		{
			joystick_forwardmove = axis_value * joy_forwardsensitivity->value;

			// We need to be twice faster because with joystic we run...
			joystick_forwardmove *= cl_forwardspeed->value * 2.0f;
		}
		else if (strcmp(direction, "yaw") == 0)
		{
			joystick_yaw = axis_value * joy_yawsensitivity->value;
			joystick_yaw *= cl_yawspeed->value;
		}
		else if (strcmp(direction, "pitch") == 0)
		{
			joystick_pitch = axis_value * joy_pitchsensitivity->value;
			joystick_pitch *= cl_pitchspeed->value;
		}
		else if (strcmp(direction, "updown") == 0)
		{
			joystick_up = axis_value * joy_upsensitivity->value;
			joystick_up *= cl_upspeed->value;
		}
	}

	static qboolean left_trigger = false;
	static qboolean right_trigger = false;

	if (strcmp(direction, "triggerleft") == 0)
	{
		qboolean new_left_trigger = abs(axis_value) > (32767 / 4);

		if (new_left_trigger != left_trigger)
		{
			left_trigger = new_left_trigger;
			Key_Event(K_TRIG_LEFT, left_trigger, true);
		}
	}
	else if (strcmp(direction, "triggerright") == 0)
	{
		qboolean new_right_trigger = abs(axis_value) > (32767 / 4);

		if (new_right_trigger != right_trigger)
		{
			right_trigger = new_right_trigger;
			Key_Event(K_TRIG_RIGHT, right_trigger, true);
		}
	}
}

static inline void
IN_adjustAxes(padData paddata)
{
	char* direction_type;
	float threshold;
	// Left horizontal
	{
		direction_type = joy_axis_leftx->string;
		threshold = joy_axis_leftx_threshold->value;
		IN_adjustJoyDirection(paddata.ANA_L_H, direction_type, threshold);
	}

	// Left vertical
	{
		direction_type = joy_axis_lefty->string;
		threshold = joy_axis_lefty_threshold->value;
		IN_adjustJoyDirection(paddata.ANA_L_V, direction_type, threshold);
	}

	// Right horizontal
	{
		direction_type = joy_axis_rightx->string;
		threshold = joy_axis_rightx_threshold->value;
		IN_adjustJoyDirection(paddata.ANA_R_H, direction_type, threshold);
	}

	// Right vertical
	{
		direction_type = joy_axis_righty->string;
		threshold = joy_axis_righty_threshold->value;
		IN_adjustJoyDirection(paddata.ANA_R_V, direction_type, threshold);
	}

	// L2
	// {
	// 	Com_Printf("L2: 0x%04X\n", paddata.PRE_L2);
	// 	direction_type = joy_axis_triggerleft->string;
	// 	threshold = joy_axis_triggerleft_threshold->value;
	// 	IN_adjustJoyDirection(paddata.PRE_L2, direction_type, threshold);
	// }

	// R2
	// {
	// 	Com_Printf("R2: 0x%04X\n", paddata.PRE_R2);
	// 	direction_type = joy_axis_triggerright->string;
	// 	threshold = joy_axis_triggerright_threshold->value;
	// 	IN_adjustJoyDirection(paddata.PRE_R2, direction_type, threshold);
	// }

	// Other presure sensitive buttons could be added as where L2 and R2
	// If doing so make sure their MAP_DS3_TO_QKEY macros are removed in
	// IN_Update
}

#define MAP_DS3_TO_QKEY(PAD_KEY, QKEY) \
if (pad_archive[i].PAD_KEY != paddata.PAD_KEY)\
{\
	Key_Event(QKEY, paddata.PAD_KEY, true);\
}

/*
 * Updates the input queue state. Called every
 * frame by the client and does nearly all the
 * input magic.
 */
void
IN_Update(void)
{
	padInfo padinfo;
	padData paddata;

	static padData pad_archive[MAX_PADS];

	sysUtilCheckCallback();

	ioPadGetInfo(&padinfo);
	for(int i = 0; i < MAX_PADS; ++i)
	{
		if(padinfo.status[i] == 0)
		{
			continue;
		}
		if (ioPadGetData(i, &paddata) != 0 )
		{
			continue;
		}

		if (paddata.len < 8)
		{
			break;
		}

		MAP_DS3_TO_QKEY(BTN_SELECT,   K_JOY_BACK);
		MAP_DS3_TO_QKEY(BTN_START,    K_BTN_START);

		// I want special mapping for buttons in console
		if (cls.key_dest == key_console)
		{
			// map dpad to arrows
			MAP_DS3_TO_QKEY(BTN_LEFT,     K_LEFTARROW);
			MAP_DS3_TO_QKEY(BTN_DOWN,     K_DOWNARROW);
			MAP_DS3_TO_QKEY(BTN_RIGHT,    K_RIGHTARROW);
			MAP_DS3_TO_QKEY(BTN_UP,       K_UPARROW);

			// square is backspace
			MAP_DS3_TO_QKEY(BTN_SQUARE,   K_BACKSPACE);
			// cross is enter (execute command)
			MAP_DS3_TO_QKEY(BTN_CROSS,    K_ENTER);
			// circle to special key to quit console
			MAP_DS3_TO_QKEY(BTN_CIRCLE,   K_CONSOLE);

			// press triangle to open OSK
			if (pad_archive[i].BTN_TRIANGLE != paddata.BTN_TRIANGLE
			&& paddata.BTN_TRIANGLE)
			{
				IN_xmbOpenOSK_f();
			}
		}
		else
		{
			MAP_DS3_TO_QKEY(BTN_LEFT,     K_DPAD_LEFT);
			MAP_DS3_TO_QKEY(BTN_DOWN,     K_DPAD_DOWN);
			MAP_DS3_TO_QKEY(BTN_RIGHT,    K_DPAD_RIGHT);
			MAP_DS3_TO_QKEY(BTN_UP,       K_DPAD_UP);

			MAP_DS3_TO_QKEY(BTN_SQUARE,   K_BTN_X);
			MAP_DS3_TO_QKEY(BTN_CROSS,    K_BTN_A);
			MAP_DS3_TO_QKEY(BTN_CIRCLE,   K_BTN_B);
			MAP_DS3_TO_QKEY(BTN_TRIANGLE, K_SC_E);
		}

		MAP_DS3_TO_QKEY(BTN_R1,       K_SHOULDER_RIGHT);
		MAP_DS3_TO_QKEY(BTN_L1,       K_SHOULDER_LEFT);

		// L2/R2 are ignored b/c YQ2 threat them as axes
		// See end note in IN_adjustAxes above
		//
		// For some reason they are not working as
		// axes LOL
		MAP_DS3_TO_QKEY(BTN_R2,       K_TRIG_RIGHT);
		MAP_DS3_TO_QKEY(BTN_L2,       K_TRIG_LEFT);

		MAP_DS3_TO_QKEY(BTN_R3,       K_STICK_RIGHT);
		MAP_DS3_TO_QKEY(BTN_L3,       K_STICK_LEFT);

		IN_adjustAxes(paddata);
		
		pad_archive[i] = paddata;
	}

	sys_frame_time = Sys_Milliseconds();
}

/*
 * Move handling
 */
void
IN_Move(usercmd_t *cmd)
{
	// To make the the viewangles changes independent of framerate we need to scale
	// with frametime (assuming the configured values are for 60hz)
	//
	// 1/32768 is to normalize the input values from SDL (they're between -32768 and
	// 32768 and we want -1 to 1) for movement this is not needed, as those are
	// absolute values independent of framerate
	float joyViewFactor = (1.0f/32768.0f) * (cls.rframetime/0.01666f);

	if (joystick_yaw)
	{
		cl.viewangles[YAW] -= (m_yaw->value * joystick_yaw) * joyViewFactor;
	}

	if(joystick_pitch)
	{
		cl.viewangles[PITCH] += (m_pitch->value * joystick_pitch) * joyViewFactor;
	}

	if (joystick_forwardmove)
	{
		cmd->forwardmove -= (m_forward->value * joystick_forwardmove) / 32768;
	}

	if (joystick_sidemove)
	{
		cmd->sidemove += (m_side->value * joystick_sidemove) / 32768;
	}

	if (joystick_up)
	{
		cmd->upmove -= (m_up->value * joystick_up) / 32768;
	}
}

/* ------------------------------------------------------------------ */

static void
IN_JoyAltSelectorDown(void)
{
	joy_altselector_pressed = true;
}

static void
IN_JoyAltSelectorUp(void)
{
	joy_altselector_pressed = false;
}

void
In_FlushQueue(void)
{
	Key_MarkAllUp();
	IN_JoyAltSelectorUp();
}

/* ------------------------------------------------------------------ */

/*
 * Haptic Feedback:
 *    effect_volume=0..SHRT_MAX
 *    effect{x,y,z} - effect direction
 *    name - sound file name
 */
void
Haptic_Feedback(char *name, int effect_volume, int effect_duration,
			   int effect_begin, int effect_end,
			   int effect_attack, int effect_fade,
			   int effect_x, int effect_y, int effect_z)
{
}

static void
IN_processOSKOutput(const oskCallbackReturnParam *oskOutput)
{
	if (oskOutput->res != OSK_OK)
	{
		return;
	}

	// Pass whole message to the game as it was wrote from keyboard
	uint16_t* str = oskOutput->str;
	// dont forget message is in UTF-16
	uint16_t key;

	for(; *str; str++)
	{
		key = *str;
		if (key < 32)
		{
			// non-printable ascii? Is it even posible?
			continue;
		}

		if (key > 127)
		{
			// punishment for not writing ascii only
			key = '?';
		}

		Key_Event(key, true, true);
		Key_Event(key, false, true);
	}
}

static void
IN_xmbEventCallback(uint64_t status, uint64_t param, void *user_data)
{
	switch(status)
	{
		// Exit game requested
		case SYSUTIL_EXIT_GAME:
			Com_Quit();
			Com_Printf("xdd\n");
			break;
		// Beginning of XMB menu drawing
		case SYSUTIL_DRAW_BEGIN:
		// End of XMB menu drawing
		case SYSUTIL_DRAW_END:
		// XMB menu has been opened
		case SYSUTIL_MENU_OPEN:
		// XMB menu has been closed
		case SYSUTIL_MENU_CLOSE:
			break;
		// On-screen keyboard has been loaded
		case SYSUTIL_OSK_LOADED:
			break;
		// On-screen keyboard has finished a user entry
		case SYSUTIL_OSK_DONE:
			// flush OSK return to output params
			oskUnloadAsync(&osk_outputParam);
			IN_processOSKOutput(&osk_outputParam);
			break;
		// On-screen keyboard has been unloaded
		case SYSUTIL_OSK_UNLOADED:
			// Com_Printf("OSK unloaded\n");
			break;
		// On-screen keyboard has canceled input
		case SYSUTIL_OSK_INPUT_CANCELED:
			// RSX NOTE: not sure when it happens
			oskAbort();
			break;
		default:
			Com_Printf("input::%s: unhandled event status (0x%lX)\n", __func__, status);
			break;
	}
}

static void
IN_utf16FromUtf8(uint16_t *dst, const uint8_t *src)
{
	int i;
	for (i = 0; src[i];)
	{
		if ((src[i] & 0xE0) == 0xE0)
		{
			*(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
			i += 3;
		}
		else if ((src[i] & 0xC0) == 0xC0)
		{
			*(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
			i += 2;
		}
		else
		{
			*(dst++) = src[i];
			i += 1;
		}
	}

	*dst = '\0';
}

// Init OSK related stuff
static void
IN_initOSK(void)
{
	IN_utf16FromUtf8(osk_message, (uint8_t *)"Input text:");
	IN_utf16FromUtf8(osk_startText, (uint8_t *)"");

	// Configure the title and initial text of the keyboard, and a maximum length
	osk_inputFieldInfo.message = osk_message;
	osk_inputFieldInfo.startText = osk_startText;
	osk_inputFieldInfo.maxLength = TEXT_BUFFER_LENGTH - 1;

	// Configure the type of panel
	osk_parameters.allowedPanels = OSK_PANEL_TYPE_DEFAULT;
	osk_parameters.firstViewPanel = OSK_PANEL_TYPE_DEFAULT;
	osk_parameters.controlPoint = (oskPoint) { 0, 0 };
	// This will disable entering a new line
	osk_parameters.prohibitFlags = OSK_PROHIBIT_RETURN;

	// Configure where the osk will write its result
	osk_outputParam.res = OSK_OK;
	osk_outputParam.len = TEXT_BUFFER_LENGTH - 1;
	osk_outputParam.str = osk_outText;

	for (int32_t res = sysMemContainerCreate(&osk_containerid, 4 * 1024 * 1024); res != 0; )
	{
		Com_Printf("Error sysMemContainerCreate: %08x\n", res);
		break;
	}

	oskSetInitialInputDevice(OSK_DEVICE_PAD);
	oskSetKeyLayoutOption(OSK_FULLKEY_PANEL);
	oskSetLayoutMode(OSK_LAYOUTMODE_HORIZONTAL_ALIGN_CENTER | OSK_LAYOUTMODE_VERTICAL_ALIGN_CENTER);
}

/*
 * Initializes the backend
 */
void
IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, IN_xmbEventCallback, NULL);

	ioPadInit(7);

	IN_initOSK();

	joystick_yaw = joystick_pitch = joystick_forwardmove = joystick_sidemove = 0;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", CVAR_ARCHIVE);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", CVAR_ARCHIVE);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_up = Cvar_Get("m_up", "1", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", CVAR_ARCHIVE);
	m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
	m_side = Cvar_Get("m_side", "0.8", CVAR_ARCHIVE);
	m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE);
	sensitivity = Cvar_Get("sensitivity", "3", CVAR_ARCHIVE);

	joy_haptic_magnitude = Cvar_Get("joy_haptic_magnitude", "0.0", CVAR_ARCHIVE);

	joy_yawsensitivity = Cvar_Get("joy_yawsensitivity", "1.0", CVAR_ARCHIVE);
	joy_pitchsensitivity = Cvar_Get("joy_pitchsensitivity", "1.0", CVAR_ARCHIVE);
	joy_forwardsensitivity = Cvar_Get("joy_forwardsensitivity", "1.0", CVAR_ARCHIVE);
	joy_sidesensitivity = Cvar_Get("joy_sidesensitivity", "1.0", CVAR_ARCHIVE);
	joy_upsensitivity = Cvar_Get("joy_upsensitivity", "1.0", CVAR_ARCHIVE);
	joy_expo = Cvar_Get("joy_expo", "2.0", CVAR_ARCHIVE);

	joy_axis_leftx = Cvar_Get("joy_axis_leftx", "sidemove", CVAR_ARCHIVE);
	joy_axis_lefty = Cvar_Get("joy_axis_lefty", "forwardmove", CVAR_ARCHIVE);
	joy_axis_rightx = Cvar_Get("joy_axis_rightx", "yaw", CVAR_ARCHIVE);
	joy_axis_righty = Cvar_Get("joy_axis_righty", "pitch", CVAR_ARCHIVE);
	joy_axis_triggerleft = Cvar_Get("joy_axis_triggerleft", "triggerleft", CVAR_ARCHIVE);
	joy_axis_triggerright = Cvar_Get("joy_axis_triggerright", "triggerright", CVAR_ARCHIVE);

	joy_axis_leftx_threshold = Cvar_Get("joy_axis_leftx_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_lefty_threshold = Cvar_Get("joy_axis_lefty_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_rightx_threshold = Cvar_Get("joy_axis_rightx_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_righty_threshold = Cvar_Get("joy_axis_righty_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_triggerleft_threshold = Cvar_Get("joy_axis_triggerleft_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_triggerright_threshold = Cvar_Get("joy_axis_triggerright_threshold", "0.15", CVAR_ARCHIVE);

	Cmd_AddCommand("+joyaltselector", IN_JoyAltSelectorDown);
	Cmd_AddCommand("-joyaltselector", IN_JoyAltSelectorUp);

	// funny enough to do so you need to open it first
	Cmd_AddCommand("openosk", IN_xmbOpenOSK_f);

	Com_Printf("------------------------------------\n\n");
}

void
IN_Shutdown(void)
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("openosk");

	sysMemContainerDestroy(osk_containerid);

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);

	Com_Printf("Shutting down input.\n");
}

/* ------------------------------------------------------------------ */
