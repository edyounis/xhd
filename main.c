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

#include "xhd_types.h"
#include "xhd_config.h"

// We keep track MAX_KEY_CODE*MAX_GROUPS*MAX_LEVELS distinct keys.
#define MAX_KEYCODE 256
#define MAX_GROUPS  4
#define MAX_LEVELS  2

// Global XKB and XCB Variables
int32_t             xkb_base = 0;
struct xkb_context* ctx      = NULL;
xcb_connection_t*   conn     = NULL;
xcb_screen_t*       screen   = NULL;
xcb_window_t        root;

// The state of XHD
xhd_mode_t current;

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
 * XHD Mode Init Function
 *
 * Initializes the current mode of XHD
 */
int xhd_mode_init ( void )
{
	int ret = 0;
	uint32_t i, j;
	uint32_t allocated_keysets = 0;
	uint32_t allocated_levelsets = 0;
	uint32_t allocated_grablists = 0;

	// Fallback values
	current.grabs     = NULL;
	current.keymap    = NULL;
	current.cur_group = 0;

	// Allocate Grab Map
	current.grabs = (xhd_grablist_t*) malloc( sizeof(xhd_grablist_t) * MAX_GROUPS );

	if ( current.grabs == NULL )
	{
		ret = -ENOMEM;
		goto fail1;
	}

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		current.grabs[i].num_grabs   = 0;
		current.grabs[i].alloc_grabs = GRAB_LIST_SIZE;
		current.grabs[i].list = (xhd_grab_t*) calloc ( GRAB_LIST_SIZE, sizeof(xhd_grab_t) );

		if ( current.grabs[i].list == NULL )
		{
			ret = -ENOMEM;
			goto fail2;
		}

		allocated_grablists++;
	}

	// Allocate Key Map
	current.keymap = (xhd_key_t***) calloc( MAX_GROUPS, sizeof(xhd_key_t**) );

	if ( current.keymap == NULL )
	{
		ret = -ENOMEM;
		goto fail1;
	}

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		current.keymap[i] = (xhd_key_t**) calloc( MAX_KEYCODE, sizeof(xhd_key_t*) );

		if ( current.keymap[i] == NULL )
		{
			ret = -ENOMEM;
			goto fail2;
		}

		allocated_keysets++;
		allocated_levelsets = 0;

		for ( j = 0; j < MAX_KEYCODE; ++j )
		{
			current.keymap[i][j] = (xhd_key_t*) calloc( MAX_LEVELS, sizeof(xhd_key_t) );

			if ( current.keymap[i][j] == NULL )
			{
				ret = -ENOMEM;
				goto fail2;
			}

			allocated_levelsets++;
		}
	}

	goto exit;

	fail4:
		for ( i = 0; i < allocated_keysets - 1; ++i )
		{
			for ( j = 0; j < MAX_LEVELS; ++j )
			{
				free( current.keymap[i][j] );
			}

			free( current.keymap[i] );
		}

		if ( allocated_keysets >= 1 )
		{
			for ( i = 0; i < allocated_levelsets; ++i )
			{
				free( current.keymap[allocated_keysets - 1][i] );
			}
		}

		free( current.keymap[allocated_keysets] );

	fail3:
		free( current.keymap );

	fail2:
		for ( i = 0; i < allocated_grablists; ++i )
		{
			free( current.grabs[i].list );
			current.grabs[i].alloc_grabs = 0;
		}

	fail1:
		free( current.grabs );

	exit:
		return ret;
}

/**
 * XHD Mode Final Function
 *
 * Cleans up mode memory
 */
int xhd_mode_fini ( void )
{
	int i, j, k, l, m;

	for ( i = 0; i < MAX_GROUPS; ++i )
		free( current.grabs[i].list );

	free( current.grabs );

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		for ( j = 0; j < MAX_KEYCODE; ++j )
		{
			for ( k = 0; k < MAX_LEVELS; ++k )
			{
				for ( l = 0; l < current.keymap[i][j][k].num_acts; ++l )
				{
					for ( m = 0; m < current.keymap[i][j][k].acts[l].num_cmds; ++m )
					{
						free( current.keymap[i][j][k].acts[l].cmds[m] );
					}
					free( current.keymap[i][j][k].acts[l].cmds );
				}
				free( current.keymap[i][j][k].acts );
			}
			free( current.keymap[i][j] );
		}
		free( current.keymap[i] );
	}
	free( current.keymap );

	return 0;
}

/**
 * XHD Mode Parse Keymap Function
 *
 * Retrieves and parses X keymap
 */
int xhd_mode_parse_keymap ( void )
{
	int ret = 0;
	uint32_t key_index;
	uint32_t level_index;
	uint32_t group_index;
	uint32_t max_groups;
	uint32_t max_levels;

	xkb_keysym_t* keysym;

	// Get Core Keyboard
	int32_t device_id = xkb_x11_get_core_keyboard_device_id( conn );

	if ( device_id == -1 )
	{
		ret = -1;
		goto fail1;
	}
	// Get XKB Keymap
	struct xkb_keymap* keymap = xkb_x11_keymap_new_from_device( ctx, conn, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS );

	if ( ! keymap )
	{
		ret = -2;
		goto fail2;
	}

	for ( key_index = 0; key_index < MAX_KEYCODE; ++key_index )
	{
		max_groups = xkb_keymap_num_layouts_for_key( keymap, key_index ) % MAX_GROUPS;
		for ( group_index = 0; group_index < max_groups; ++group_index )
		{
			max_levels = xkb_keymap_num_levels_for_key( keymap, key_index, group_index );
			if ( max_levels >= MAX_LEVELS ) max_levels = MAX_LEVELS;
			for ( level_index = 0; level_index < max_levels; ++level_index )
			{
				xkb_keymap_key_get_syms_by_level( keymap, key_index, group_index, level_index, (const xkb_keysym_t**)&keysym );
				if ( keysym )
				{
					current.keymap[ group_index ][ key_index ][ level_index ].symbol = *keysym;
				}
			}
		}
	}

	goto exit;

	fail2:
	fail1:
	exit:
		return ret;
}

/**
 * XHD Mode Register Grab Function
 *
 * Adds a grab to the correct grab list
 *
 * TODO: Search Grab list for same grab and avoid adding
 */
int xhd_mode_register_grab( uint32_t group_index, uint32_t key_index, uint8_t modifier )
{
	int i;

	xhd_grablist_t* grablist = &current.grabs[group_index];

	if ( grablist->alloc_grabs <= grablist->num_grabs )
	{
		grablist->alloc_grabs *= 2;
		grablist->alloc_grabs += 2;
		xhd_grab_t* tmp = (xhd_grab_t*) calloc( grablist->alloc_grabs, sizeof(xhd_grab_t) );

		if ( tmp == NULL )
		{
			return -1;
		}

		if ( grablist->num_grabs != 0 )
		{
			for ( i = 0; i < grablist->num_grabs; ++i )
			{
				tmp[i].modifier = grablist->list[i].modifier;
				tmp[i].keycode  = grablist->list[i].keycode;
			}
		}

		free( grablist->list );
		grablist->list = tmp;
	}

	grablist->list[ grablist->num_grabs ].keycode  = key_index;
	grablist->list[ grablist->num_grabs ].modifier = modifier;
	grablist->num_grabs++;
}

/**
 * XHD Mode Register Command Function
 *
 * Registers an action with a keycode and a modifier value
 *
 * TODO: Search the key actions for the same modifier to overwrite rather than create a new one
 */
int xhd_mode_register_action ( xhd_key_t* key, uint8_t modifier, const char* command )
{
	int i;

	// If no more available slots, allocate more
	if ( key->alloc_acts <= key->num_acts )
	{
		key->alloc_acts *= 2;
		key->alloc_acts += 2;
		xhd_action_t* tmp = (xhd_action_t*) calloc( key->alloc_acts, sizeof(xhd_action_t) );

		if ( tmp == NULL )
		{
			return -1;
		}

		if ( key->num_acts != 0 )
		{
			for ( i = 0; i < key->num_acts; ++i )
			{
				tmp[i].mod      = key->acts[i].mod;
				tmp[i].num_cmds = key->acts[i].num_cmds;
				tmp[i].cmds     = key->acts[i].cmds;
			}
		}

		free( key->acts );
		key->acts = tmp;
	}

	xhd_action_t* action = &key->acts[ key->num_acts++ ];

	action->mod      = (uint16_t) modifier;
	action->num_cmds = 1; // TODO Change like everything about parsing to allow for command block
	action->cmds     = (char**) malloc ( sizeof(char**) * action->num_cmds );
	action->cmds[0]  = strdup( command );
	return 0;
}

/**
 * XHD Mode Add Command Function
 *
 * Associates a command with a keysym and modifier value
 */
int xhd_mode_add_action ( const char* keystring, uint8_t modifier, const char* command )
{
	uint32_t key_index;
	uint32_t group_index;
	uint32_t level_index;

	// Translates the key string to a key symbol
	xkb_keysym_t keysym = xkb_keysym_from_name( keystring, XKB_KEYSYM_CASE_INSENSITIVE );

	if ( keysym == 0 )
	{
		fprintf( stderr, "Failed to translate key: %s\n", keystring );
		return -1;
	}

	// Looks up corresponding key codes
	for ( group_index = 0; group_index < MAX_GROUPS; ++group_index )
	{
		for ( key_index = 0; key_index < MAX_KEYCODE; ++key_index )
		{
			for ( level_index = 0; level_index < MAX_LEVELS; ++level_index )
			{
				if ( keysym == current.keymap[group_index][key_index][level_index].symbol )
				{
					// Auto-add shift level to shifted characters
					if ( level_index != 0 )
						modifier |= 1; // TODO: Make proper defined Shift Bit

					if ( (modifier & 1) == 1 )
						level_index = 1; // TODO this seems sloppy

					// Registers command with key code and modifier combination
					xhd_mode_register_action( &current.keymap[group_index][key_index][level_index], modifier, command );

					// Adds to correct grab list
					xhd_mode_register_grab( group_index, key_index, modifier );
				}
			}
		}
	}
	return 0;
}

/**
 * XHD Mode Load Function
 *
 * Parses X keymap table and config file
 * Loads data into current mode data structure
 */
int xhd_mode_load ( void )
{
	xhd_mode_parse_keymap();
	xhd_config_parse();
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
int xhd_runtime_grab_all_keys ( void )
{
	uint32_t    i;
	uint32_t    max_i = current.grabs[ current.cur_group ].num_grabs;
	xhd_grab_t* list  = current.grabs[ current.cur_group ].list;

	for ( i = 0; i < max_i; ++i )
		xcb_grab_key( conn, 0, root, list[i].modifier, list[i].keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC );

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
		char *cmd[] = {"/bin/bash", "-c", action->cmds[i], NULL}; // TODO do better

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
	xhd_init();
	xhd_mode_init();
	xhd_mode_load();
	xhd_runtime_grab_all_keys();

	xcb_generic_event_t* event;

	while ( 0 )
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

			xhd_key_t* key = &current.keymap[ current.cur_group ][ keycode ][ level ];

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
				if ( current.cur_group != state->group )
				{
					xhd_runtime_ungrab_all_keys();
					current.cur_group = state->group;
					xhd_runtime_grab_all_keys();
				}
			}
		}

		if ( event != NULL )
		{
			free( event );
		}
	}

	xhd_mode_fini();
	xhd_fini();
}
