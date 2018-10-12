#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define ACTION_MAX_LENGTH 256
#define MAX_KEYCODE 256
#define MAX_GROUPS 4

typedef uint32_t xhd_keysym_t;
typedef uint16_t xhd_modifier_t;
typedef uint8_t  xhd_keycode_t;

typedef struct xhd_actionable_item_t
{
	xhd_modifier_t mod;
	char action[ ACTION_MAX_LENGTH ];

} xhd_actionable_item_t;

typedef struct xhd_key_t
{
	uint32_t num_symbols;
	xhd_keysym_t symbols[16];

	uint32_t num_actions;
	xhd_actionable_item_t actions[16];

} xhd_key_t;

typedef xhd_key_t** keylist_t;

struct xkb_context* ctx;
int32_t xkb_base;

xcb_window_t      root;
xcb_connection_t* conn;

xhd_key_t keylist[ MAX_GROUPS ][ MAX_KEYCODE ];
int actionlist[ MAX_GROUPS ][ MAX_KEYCODE ];

uint8_t cur_group = 0;

int setup ( void );
int parse_keymap ( keylist_t kl );
int parse_config_entry ( const char* entry, keylist_t kl );
int find_key_and_group ( xkb_keysym_t keysym, keylist_t kl, int* num_found, int* found_pairs );
int parse_modifer ( const char* modifier );
int run ( char* command );
int grab_all_keys ( uint8_t group );
int ungrab_all_keys ( uint8_t group );

int main ( void )
{
	setup();
	parse_keymap ( keylist );
	FILE *config = fopen( "/home/edyounis/.config/xhd/config", "r" );
	char buf[256];
	while ( fgets (buf, sizeof(buf), config) != NULL )
		//printf("%s\n", buf);
		parse_config_entry( buf, keylist );
	grab_all_keys(cur_group);

	while (1) {
		xcb_generic_event_t *ev = xcb_wait_for_event(conn);

		if (ev && ((ev->response_type & ~0x80) == XCB_KEY_PRESS))
		{
			xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
			printf ("Got key press s: %d k: %d\n", (int)kp->state, (int)kp->detail);
			int maxAction = keylist[cur_group][(int)kp->detail].num_actions;
			int i;
			for ( i = 0; i < maxAction; ++i )
			{
				if ( keylist[cur_group][(int)kp->detail].actions[i].mod == ((int)kp->state & 0x9FFF) )
				{
					run( keylist[cur_group][(int)kp->detail].actions[i].action );
					break;
				}
			}
		}
		else if ( ev->response_type == xkb_base )
		{
			xcb_xkb_state_notify_event_t *state = (xcb_xkb_state_notify_event_t *)ev;
			if (state->xkbType == XCB_XKB_STATE_NOTIFY)
			{
				if ( cur_group != state->group )
				{
					ungrab_all_keys(cur_group);
					cur_group = state->group;
					grab_all_keys(cur_group);
				}
			}
		}

		if (ev != NULL) {
			free(ev);
		}
	}
}

int grab_all_keys ( uint8_t group )
{
	int key_index;
	for ( key_index = 1; key_index <= actionlist[group][0]; ++key_index )
	{
		int key = actionlist[group][key_index];
		int keycode  = key & 0xFF;
		int modifier = key >> 8;
		printf("grabbing key %d with modifiers %d on group %d\n", keycode, modifier, group );
		xcb_grab_key( conn, 0, root, modifier, keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC );
	}
	xcb_flush(conn);
}

int ungrab_all_keys ( uint8_t group )
{
	xcb_ungrab_key( conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY );
}

int parse_config_entry ( const char* entry, keylist_t kl )
{
	if ( entry[0] == ' ' || entry[0] == '#' || entry[0] == '\n' || entry[0] == '\t' )
		return -1;

	int entry_index = 0;
	int buf_index = 0;
	int buf_key_index = 0;
	int action_index = 0;
	int action_flag = 0;

	char i = entry[0];
	char key_buf[8][40];
	char action[256];

	memset( key_buf[buf_key_index], 0, 40 );
	memset( action, 0, 256 );

	// First Pass through
	while( i )
	{
		if ( action_flag == 0 && i == ' ' )
		{
			action_flag = 1;
		}
		else if ( action_flag == 0 && i == '+' )
		{
			buf_index = 0;
			++buf_key_index;

			if ( buf_key_index >= 8 )
				return -1;

			memset( key_buf[buf_key_index], 0, 40 );
		}
		else if ( action_flag == 0 )
		{
			key_buf[buf_key_index][buf_index++] = i;
		}
		else if ( action_flag = 1)
		{
			action[action_index++] = i;
		}

		i = entry[ ++entry_index ];
	}

	uint8_t modifier = 0;

	int j;
	for ( j = 0; j < buf_key_index; ++j )
	{
		uint8_t tmp = parse_modifer( key_buf[j] );

		if ( tmp == (uint8_t)-1 )
			return -1;

		modifier |= (uint8_t) tmp;
	}


	// Translate keysym
	xkb_keysym_t keysym = xkb_keysym_from_name( key_buf[buf_key_index], XKB_KEYSYM_CASE_INSENSITIVE );

	// Get its associated key and group
	int num_key_group_pairs = 0;
	int key_group_pairs[16];

	int key_and_group = find_key_and_group( keysym, keylist, &num_key_group_pairs, key_group_pairs );
	printf("%d\n", num_key_group_pairs);
	for ( j = 0; j < num_key_group_pairs; ++j )
	{
		int key = key_group_pairs[j] & 0xFF;
		int group = key_group_pairs[j] >> 8;

		printf("keysym %d, keycode %d, modifier %d, group %d\n", keysym, key, modifier, group);

		// Add action to the keylist
		strcpy( keylist[group][key].actions[ keylist[group][key].num_actions ].action, action );
		keylist[group][key].actions[ keylist[group][key].num_actions++ ].mod = modifier;

		// Record this key has action on this group
		actionlist[group][ ++actionlist[group][0] ] = (modifier << 8) | key;
	}
}


int run ( char* command )
{
	char *cmd[] = {"/bin/bash", "-c", command, NULL};
	if ( fork() == 0 )
	{
		if (conn != NULL)
			close(xcb_get_file_descriptor(conn));
		execvp(cmd[0], cmd);
	}
}

int parse_modifer ( const char* modifier )
{
	if ( strcmp( modifier, "shift" ) == 0 )
		return 1 << 0;
	if ( strcmp( modifier, "lock" ) == 0 )
		return 1 << 1;
	if ( strcmp( modifier, "ctrl" ) == 0 )
		return 1 << 2;
	if ( strcmp( modifier, "mod1" ) == 0 )
		return 1 << 3;
	if ( strcmp( modifier, "mod2" ) == 0 )
		return 1 << 4;
	if ( strcmp( modifier, "mod3" ) == 0 )
		return 1 << 5;
	if ( strcmp( modifier, "mod4" ) == 0 )
		return 1 << 6;
	if ( strcmp( modifier, "mod5" ) == 0 )
		return 1 << 7;
	return -1;
}

int find_key_and_group ( xkb_keysym_t keysym, keylist_t kl, int* num_found, int* found_pairs )
{
	int key_index;
	int group_index;
	int level_index;
	int max_level;

	*num_found = 0;

	for ( key_index = 0; key_index < MAX_KEYCODE; ++key_index )
	{
		for ( group_index = 0; group_index < MAX_GROUPS; ++group_index )
		{
			max_level = keylist[group_index][key_index].num_symbols;
			for ( level_index = 0; level_index < max_level; ++level_index )
			{
				if ( keysym == keylist[group_index][key_index].symbols[level_index] )
				{
					found_pairs[ (*num_found)++ ] = (group_index << 8) | key_index;
				}
			}
		}
	}
	return 0;
}


int parse_keymap ( keylist_t kl )
{
	int32_t device_id = xkb_x11_get_core_keyboard_device_id(conn);

	if ( device_id == -1 )
		return -1;

	struct xkb_keymap* keymap = xkb_x11_keymap_new_from_device( ctx, conn, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS );

	if ( !keymap )
		return -1;

	memset( (void*)kl, 0, sizeof(xhd_key_t) * MAX_GROUPS * MAX_KEYCODE );

	int group_index;
	for ( group_index = 0; group_index < MAX_GROUPS; ++group_index )
	{
		actionlist[group_index][0] = 0;
	}

	int key_index;
	int level_index;
	int max_groups;
	int max_levels;
	xkb_keysym_t* keysym;
	for ( key_index = 0; key_index < MAX_KEYCODE; ++key_index )
	{
		max_groups = xkb_keymap_num_layouts_for_key( keymap, key_index );
		for ( group_index = 0; group_index < max_groups; ++group_index )
		{
			max_levels = xkb_keymap_num_levels_for_key( keymap, key_index, group_index );
			for ( level_index = 0; level_index < max_levels; ++level_index )
			{
				xkb_keymap_key_get_syms_by_level( keymap, key_index, group_index, level_index, &keysym );
				if ( keysym )
				{
					keylist[group_index][key_index].symbols[ keylist[group_index][key_index].num_symbols++ ] = *keysym;
					//printf("%d:%d:%d, %x\n", i,j,k, *keysym);
				}
			}
		}
	}
}

int setup ( void )
{
	conn = xcb_connect(NULL, NULL);

	if ( xcb_connection_has_error( conn ) )
		fprintf( stderr, "Can't open display.\n" );

	xcb_screen_t* screen = xcb_setup_roots_iterator( xcb_get_setup( conn ) ).data;

	if ( screen == NULL )
		fprintf( stderr, "Can't acquire screen.\n" );

	root = screen->root;
	ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	if ( !ctx )
		return -1;

	if ( ! xkb_x11_setup_xkb_extension( conn,
										XKB_X11_MIN_MAJOR_XKB_VERSION,
										XKB_X11_MIN_MINOR_XKB_VERSION,
										XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
										NULL, NULL, NULL, NULL ) )
		return -2;


	const xcb_query_extension_reply_t *extreply;
    extreply = xcb_get_extension_data(conn, &xcb_xkb_id);
    xkb_base = extreply->first_event;
        const uint32_t mask = XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE |
                              XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED |
                              XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT;

    xcb_xkb_per_client_flags_reply(
            conn,
            xcb_xkb_per_client_flags(
                conn,
                XCB_XKB_ID_USE_CORE_KBD,
                mask,
                mask,
                0 /* uint32_t ctrlsToChange */,
                0 /* uint32_t autoCtrls */,
                0 /* uint32_t autoCtrlsValues */),
            NULL);

    xcb_xkb_select_events(conn,
                      XCB_XKB_ID_USE_CORE_KBD,
                      XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
                      0,
                      XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
                      0xff,
                      0xff,
                      NULL);

}
