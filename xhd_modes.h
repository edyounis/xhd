#ifndef XHD_MODES_LIB_H
#define XHD_MODES_LIB_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "xhd_types.h"

/**
 * XHD Modes Allocate Mode Function
 *
 * Allocates memory for a mode
 */
int xhd_modes_alloc_mode ( xhd_mode_t* mode )
{
	int ret = 0;
	uint32_t i, j;
	uint32_t allocated_keysets = 0;
	uint32_t allocated_levelsets = 0;
	uint32_t allocated_grablists = 0;

	// Fallback values
	mode->name      = NULL;
	mode->grabs     = NULL;
	mode->keymap    = NULL;
	mode->cur_group = 0;

	// Allocate Grab Map
	mode->grabs = (xhd_grablist_t*) malloc( sizeof(xhd_grablist_t) * MAX_GROUPS );

	if ( mode->grabs == NULL )
	{
		ret = -ENOMEM;
		goto fail1;
	}

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		mode->grabs[i].num_grabs   = 0;
		mode->grabs[i].alloc_grabs = GRAB_LIST_SIZE;
		mode->grabs[i].list = (xhd_grab_t*) calloc ( GRAB_LIST_SIZE, sizeof(xhd_grab_t) );

		if ( mode->grabs[i].list == NULL )
		{
			ret = -ENOMEM;
			goto fail2;
		}

		allocated_grablists++;
	}

	// Allocate Key Map
	mode->keymap = (xhd_key_t***) calloc( MAX_GROUPS, sizeof(xhd_key_t**) );

	if ( mode->keymap == NULL )
	{
		ret = -ENOMEM;
		goto fail1;
	}

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		mode->keymap[i] = (xhd_key_t**) calloc( MAX_KEYCODE, sizeof(xhd_key_t*) );

		if ( mode->keymap[i] == NULL )
		{
			ret = -ENOMEM;
			goto fail2;
		}

		allocated_keysets++;
		allocated_levelsets = 0;

		for ( j = 0; j < MAX_KEYCODE; ++j )
		{
			mode->keymap[i][j] = (xhd_key_t*) calloc( MAX_LEVELS, sizeof(xhd_key_t) );

			if ( mode->keymap[i][j] == NULL )
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
				free( mode->keymap[i][j] );
			}

			free( mode->keymap[i] );
		}

		if ( allocated_keysets >= 1 )
		{
			for ( i = 0; i < allocated_levelsets; ++i )
			{
				free( mode->keymap[allocated_keysets - 1][i] );
			}
		}

		free( mode->keymap[allocated_keysets] );

	fail3:
		free( mode->keymap );

	fail2:
		for ( i = 0; i < allocated_grablists; ++i )
		{
			free( mode->grabs[i].list );
			mode->grabs[i].alloc_grabs = 0;
		}

	fail1:
		free( mode->grabs );

	exit:
		return ret;
}

/**
 * XHD Modes Free Mode Function
 *
 * Cleans up mode memory
 */
int xhd_modes_free_mode ( xhd_mode_t* mode )
{
	int i, j, k, l, m;

	for ( i = 0; i < MAX_GROUPS; ++i )
		free( mode->grabs[i].list );

	free( mode->grabs );

	for ( i = 0; i < MAX_GROUPS; ++i )
	{
		for ( j = 0; j < MAX_KEYCODE; ++j )
		{
			for ( k = 0; k < MAX_LEVELS; ++k )
			{
				for ( l = 0; l < mode->keymap[i][j][k].num_acts; ++l )
				{
					for ( m = 0; m < mode->keymap[i][j][k].acts[l].num_cmds; ++m )
					{
						free( mode->keymap[i][j][k].acts[l].cmds[m] );
					}
					free( mode->keymap[i][j][k].acts[l].cmds );
				}
				free( mode->keymap[i][j][k].acts );
			}
			free( mode->keymap[i][j] );
		}
		free( mode->keymap[i] );
	}
	free( mode->keymap );

	return 0;
}

/**
 * XHD Modes Init Function
 *
 * Initializes the XHD mode list
 */
int xhd_modes_init ( xhd_modelist_t* modelist )
{
	int ret = 0;
	uint32_t i;
	uint32_t j;

	// Fallback values
	modelist->cur_mode    = 0;
	modelist->num_modes   = 0;
	modelist->alloc_modes = 0;
	modelist->modes       = NULL;

	modelist->modes = (xhd_mode_t*) malloc( sizeof(xhd_mode_t) * MODE_LIST_SIZE );

	if ( modelist->modes == NULL )
	{
		fprintf( stderr, "Error initializing mode list.\n" );
		ret = -ENOMEM;
		goto fail1;
	}

	for ( i = 0; i < MODE_LIST_SIZE; ++i )
	{
		ret = xhd_modes_alloc_mode( &modelist->modes[i] );

		if ( ret )
		{
			fprintf( stderr, "Error allocating memory for a mode.\n" );
			goto fail2;
		}

		modelist->alloc_modes++;
	}

	goto exit;

	fail2:
		for ( j = 0; j < i; ++j )
		{
			if ( xhd_modes_free_mode( &modelist->modes[j] ) )
			{
				fprintf( stderr, "Failing; failed to handle error.\n" );
				return -1;
			}
		}

	fail1:
	exit:
		return ret;
}

/**
 * XHD Modes Fini Function
 *
 * Cleans up modelist memory
 */
int xhd_modes_fini ( xhd_modelist_t* modelist )
{
	int ret = 0;
	uint32_t i;

	for ( i = 0; i < modelist->alloc_modes; ++i )
	{
		if ( xhd_modes_free_mode( &modelist->modes[i] ) )
		{
			fprintf( stderr, "Failing; failed to handle error.\n" );
			return -1;
		}
	}

	free( modelist->modes );

	modelist->cur_mode    = 0;
	modelist->num_modes   = 0;
	modelist->alloc_modes = 0;
	modelist->modes       = NULL;
}

/**
 * XHD Modes Keymap Init Function
 *
 * Retrieves and parses X keymap
 * Organizes data into XHD keymap
 *
 * TODO: only do calculation once
 */
int xhd_modes_keymap_init ( xhd_mode_t* mode )
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
					mode->keymap[ group_index ][ key_index ][ level_index ].symbol = *keysym;
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
 * XHD Modes Register Mode Function
 *
 * Adds a mode to a modelist
 */
int xhd_modes_register_mode ( xhd_modelist_t* modelist, const char* name )
{
	uint32_t i = 0;

	// If no more available slots, allocate more
	if ( modelist->alloc_modes <= modelist->num_modes )
	{
		modelist->alloc_modes *= 2;
		modelist->alloc_modes += 1;
		xhd_mode_t* tmp = (xhd_mode_t*) calloc( modelist->alloc_modes, sizeof(xhd_mode_t) );

		if ( tmp == NULL )
		{
			fprintf( stderr, "Failed to register mode: no memory\n" );
			modelist->alloc_modes -= 1;
			modelist->alloc_modes /= 2;
			return -ENOMEM;
		}

		if ( modelist->num_modes != 0 )
		{
			for ( i = 0; i < modelist->num_modes; ++i )
			{
				tmp[i].name      = modelist->modes[i].name;
				tmp[i].grabs     = modelist->modes[i].grabs;
				tmp[i].keymap    = modelist->modes[i].keymap;
				tmp[i].cur_group = modelist->modes[i].cur_group;
			}
		}

		for ( ; i < modelist->alloc_modes; ++i )
		{
			if ( xhd_modes_alloc_mode( &modelist->modes[i] ) )
			{
				fprintf( stderr, "Failed to allocate mode.\n" );
				fprintf( stderr, "Failed to handle error.\n" ); // TODO: figure how to handle it
				return -1;
			}
		}

		free( modelist->modes );
		modelist->modes = tmp;
	}

	xhd_modes_keymap_init( &modelist->modes[ modelist->num_modes ] );
	modelist->modes[ modelist->num_modes++ ].name = strdup( name );
	return 0;
}

/**
 * XHD Modes Register Grab Function
 *
 * Adds a grab to a grab list
 *
 * TODO: Search Grab list for same grab and avoid adding
 */
int xhd_modes_register_grab ( xhd_grablist_t* grablist, xhd_keycode_t key_index, xhd_modifier_t modifier )
{
	uint32_t i;

	// If no more available slots, allocate more
	if ( grablist->alloc_grabs <= grablist->num_grabs )
	{
		grablist->alloc_grabs *= 2;
		grablist->alloc_grabs += 2;
		xhd_grab_t* tmp = (xhd_grab_t*) calloc( grablist->alloc_grabs, sizeof(xhd_grab_t) );

		if ( tmp == NULL )
		{
			fprintf( stderr, "Failed to register grab: no memory\n" );
			grablist->alloc_grabs -= 2;
			grablist->alloc_grabs /= 2;
			return -ENOMEM;
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
	return 0;
}

/**
 * XHD Modes Register Action Function
 *
 * Registers an action with a key
 *
 * TODO: Search the key actions for the same modifier to overwrite rather than create a new one
 */
int xhd_modes_register_action ( xhd_key_t* key, xhd_action_t* action )
{
	uint32_t i;

	// If no more available slots, allocate more
	if ( key->alloc_acts <= key->num_acts )
	{
		key->alloc_acts *= 2;
		key->alloc_acts += 2;
		xhd_action_t* tmp = (xhd_action_t*) calloc( key->alloc_acts, sizeof(xhd_action_t) );

		if ( tmp == NULL )
		{
			fprintf( stderr, "Failed to register action: no memory\n" );
			key->alloc_acts -= 2;
			key->alloc_acts /= 2;
			return -ENOMEM;
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
		return 0;
	}

	key->acts[ key->num_acts++ ] = *action;
	return 0;
}

/**
 * XHD Modes Register Command Function
 *
 * Registers a command with an action
 */
int xhd_modes_register_command ( xhd_action_t* action, const char* cmd )
{
	uint32_t i;

	// If no more available slots, allocate more
	if ( action->alloc_cmds <= action->num_cmds )
	{
		action->alloc_cmds *= 2;
		action->alloc_cmds += 2;
		char** tmp = (char**) calloc( action->alloc_cmds, sizeof(char*) );

		if ( tmp == NULL )
		{
			fprintf( stderr, "Failed to register command: no memory\n" );
			action->alloc_cmds -= 2;
			action->alloc_cmds /= 2;
			return -ENOMEM;
		}

		if ( action->num_cmds != 0 )
		{
			for ( i = 0; i < action->num_cmds; ++i )
			{
				tmp[i] = action->cmds[i];
			}
		}

		free( action->cmds );
		action->cmds = tmp;
		return 0;
	}

	action->cmds[ action->num_cmds++ ] = strdup( cmd );
	return 0;
}

/**
 * XHD Modes Add Action Function
 *
 * Associates an action with a keysym and modifier value
 */
int xhd_modes_add_action ( xhd_mode_t* mode, xkb_keysym_t keysym, xhd_action_t* action )
{
	uint32_t key_index;
	uint32_t group_index;
	uint32_t level_index;

	// Looks up corresponding key codes
	for ( group_index = 0; group_index < MAX_GROUPS; ++group_index )
	{
		for ( key_index = 0; key_index < MAX_KEYCODE; ++key_index )
		{
			for ( level_index = 0; level_index < MAX_LEVELS; ++level_index )
			{
				if ( keysym == mode->keymap[group_index][key_index][level_index].symbol )
				{
					// Auto-add shift level to shifted characters
					if ( level_index != 0 )
						action->mod |= 1; // TODO: Make proper defined Shift Bit

					if ( (action->mod & 1) == 1 )
						level_index = 1; // TODO this seems sloppy

					// Registers command with key code and modifier combination
					xhd_modes_register_action( &mode->keymap[group_index][key_index][level_index], action );

					// Adds to correct grab list
					xhd_modes_register_grab( &mode->grabs[group_index], key_index, action->mod );
				}
			}
		}
	}
	return 0;
}

#endif