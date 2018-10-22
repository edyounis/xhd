// TODO
// Add Mode switching
// Add config option on command line
// Add debug print statements
// Add debug option on command line
// keypress vs keyrelease distinct
// Serial vs Parallel commands in actions
// Handle Errors more carefully

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// Global XKB and XCB Variables
int32_t             xkb_base = 0;
struct xkb_context* ctx      = NULL;
xcb_connection_t*   conn     = NULL;
xcb_screen_t*       screen   = NULL;
xcb_window_t        root;

// We keep track MAX_KEY_CODE*MAX_GROUPS*MAX_LEVELS distinct keys.
#define MAX_KEYCODE 256
#define MAX_GROUPS  4
#define MAX_LEVELS  2

#include "xhd_types.h"
#include "xhd_modes.h"
#include "xhd_config.h"


/**
 * XHD Initialization Function
 *
 * 1) Connects to X Server
 * 2) Initializes XKB extensions
 * 3) Sets necessary flags
 */
int xhd_init ( void )
{
	int ret = 0;

	// Establish a connection to the X server
	conn = xcb_connect( NULL, NULL );

	if ( xcb_connection_has_error( conn ) )
	{
		fprintf( stderr, "Can't open display.\n" );
		ret = -1;
		goto exit;
	}

	// Get X Screen
	screen = xcb_setup_roots_iterator( xcb_get_setup( conn ) ).data;

	if ( screen == NULL )
	{
		fprintf( stderr, "Can't acquire screen.\n" );
		ret = -2;
		goto fail1;
	}

	// Get Root Window
	root = screen->root;

	// Get XKB Context
	ctx = xkb_context_new( XKB_CONTEXT_NO_FLAGS );

	if ( ! ctx )
	{
		fprintf( stderr, "Can't acquire context.\n" );
		ret = -3;
		goto fail1;
	}

	// Setup XKB Extension
	if ( ! xkb_x11_setup_xkb_extension
		(
			conn,
			XKB_X11_MIN_MAJOR_XKB_VERSION,
			XKB_X11_MIN_MINOR_XKB_VERSION,
			XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
			NULL, NULL, NULL, NULL
		)
	)
	{
		fprintf( stderr, "Can't setup xkb extensions.\n" );
		ret = -4;
		goto fail2;
	}


	// Get Extension Data
	const xcb_query_extension_reply_t* extreply;

	extreply = xcb_get_extension_data( conn, &xcb_xkb_id );

	if ( ! extreply->present )
	{
		fprintf( stderr, "XKB not supported.\n" );
		ret = -5;
		goto fail2;
	}

	xkb_base = extreply->first_event;

	const uint32_t mask = XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE |
						  XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED |
						  XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT;

	// Set more XKB flags to receive group layout
	xcb_xkb_per_client_flags_reply
	(
		conn,
		xcb_xkb_per_client_flags
		(
			conn,
			XCB_XKB_ID_USE_CORE_KBD,
			mask,
			mask,
			0 /* uint32_t ctrlsToChange */,
			0 /* uint32_t autoCtrls */,
			0 /* uint32_t autoCtrlsValues */
		),
		NULL
	);

	// Set even more XKB flags to receive group/layout change events
	xcb_xkb_select_events
	(
		conn,
		XCB_XKB_ID_USE_CORE_KBD,
		XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
		0,
		XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
		0xff,
		0xff,
		NULL
	);

	goto exit;

	fail2:
		xkb_context_unref( ctx );
	fail1:
		xcb_disconnect( conn );
	exit:
		return ret;
}

/**
 * XHD Final Function
 *
 * Safely exit
 */
int xhd_fini ( void )
{
	xkb_context_unref( ctx );
	xcb_disconnect( conn );
	return 0;
}

/**
 * XHD Runtime Ungrab All Keys Function
 *
 * Ungrabs all keys
 */
int xhd_runtime_ungrab_all_keys ( void )
{
	xcb_ungrab_key( conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY );
	return 0;
}

/**
 * XHD Runtime Grab All Keys
 *
 * Grabs all keys for the current group
 */
int xhd_runtime_grab_all_keys ( xhd_mode_t* mode )
{
	uint32_t    i;
	uint32_t    max_i = mode->grabs[ mode->cur_group ].num_grabs;
	xhd_grab_t* list  = mode->grabs[ mode->cur_group ].list;

	for ( i = 0; i < max_i; ++i )
	{
		printf("Grabbing key s=%d, k=%d\n", list[i].modifier, list[i].keycode );
		xcb_grab_key( conn, 0, root, list[i].modifier, list[i].keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC );
	}

	xcb_flush( conn );
	return 0;
}

/**
 * XHD Runtime Execute Function
 *
 * Executes an action
 */
int xhd_runtime_execute ( xhd_action_t* action )
{
	int i;

	for ( i = 0; i < action->num_cmds; ++i )
	{
		char *cmd[] = {"/bin/bash", "-c", action->cmds[i], NULL}; // TODO do better with custom shells

		if ( fork() == 0 )
		{
			if ( conn != NULL )
				close( xcb_get_file_descriptor( conn ) );

			execvp(cmd[0], cmd);
		}
	}
}


int main ( void )
{
	xhd_modelist_t modelist; // The current state of XHD

	xhd_init();
	xhd_modes_init( &modelist );
	xhd_config_parse( &modelist );
	xhd_runtime_grab_all_keys( &modelist.modes[ modelist.cur_mode ] );

	xcb_generic_event_t* event;

	while ( 1 )
	{
		// Take care of finished sub-processes
		waitpid( -1, NULL, WNOHANG );

		// Block until event
		event = xcb_wait_for_event( conn );

		if ( ! event )
			continue;

		if ( (event->response_type & ~0x80) == XCB_KEY_PRESS )
		{
			xcb_key_press_event_t* keypress = (xcb_key_press_event_t*) event;

			uint16_t keycode  = (uint16_t) keypress->detail;
			uint16_t modifier = ((uint16_t) keypress->state) & 0x9FFF;
			uint16_t level    = modifier & 1;
			printf("Got keypress s=%d, k=%d\n", modifier, keycode );

			xhd_key_t* key = &modelist.modes[ modelist.cur_mode ].keymap[ modelist.modes[ modelist.cur_mode ].cur_group ][ keycode ][ level ];

			int i;
			for ( i = 0; i < key->num_acts; ++i )
			{
				if ( key->acts[i].mod == modifier )
				{
					xhd_runtime_execute( &key->acts[i] );
					break;
				}
			}
		}
		else if ( event->response_type == xkb_base )
		{
			xcb_xkb_state_notify_event_t* state = (xcb_xkb_state_notify_event_t*) event;

			if ( state->xkbType == XCB_XKB_STATE_NOTIFY )
			{
				if ( modelist.modes[ modelist.cur_mode ].cur_group != state->group )
				{
					xhd_runtime_ungrab_all_keys();
					modelist.modes[ modelist.cur_mode ].cur_group = state->group;
					xhd_runtime_grab_all_keys( &modelist.modes[ modelist.cur_mode ] );
				}
			}
		}

		if ( event != NULL )
		{
			free( event );
		}
	}

	xhd_modes_fini( &modelist );
	xhd_fini();
}
