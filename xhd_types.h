#ifndef XHD_TYPES_LIB_H
#define XHD_TYPES_LIB_H

#define GRAB_LIST_SIZE 100

typedef uint16_t xhd_modifier_t;
typedef uint16_t xhd_keycode_t;

/**
 * An XHD Action Object
 *
 * This represents a pairing between modifier flags and shell commands.
 * Each Key + Modifier combo can have one action
 * Each Action can have many commands
 * The Action Type is immutable
 */
typedef struct xhd_action_t
{
	xhd_modifier_t mod;			// Modifier flags
	uint32_t       num_cmds;	// Number of commands
	char**         cmds;		// List of commands

} xhd_action_t;

/**
 * An XHD Key Object
 *
 * This object represents a pairing of a key symbol to actions.
 * Each key symbol can be assigned many actions (with different modifier flags).
 *
 * The xhd_keymap_t is responsible for translating key press events to these.
 */
typedef struct xhd_key_t
{
	xkb_keysym_t  symbol;		// The symbol assigned to this key

	uint32_t      num_acts;		// The number of actions assigned to this key
	uint32_t      alloc_acts;	// The number of allocated action slots
	xhd_action_t* acts;			// The array of actions

} xhd_key_t;

/**
 * An XHD Key Map Object
 *
 * A hierarchically map:
 * 1st level: X Group/Layout Level (0 - 3)
 * 2nd level: X keycodes (0 - 255)
 * 3rd level: X Shift Level (0, 1 only)
 *
 * This is responsible for translating key press events to xhd_key_t's.
 *
 * Only 2 shift levels supported for naive keysym translation,
 * for specifying actions on other levels see main documentation
 */
typedef xhd_key_t*** xhd_keymap_t;

/**
 * An XHD Grab Object
 *
 * We need to tell the X server which key events we want to receive/grab.
 * To do so, we specify a keycode and a modifier.
 * Also, there is no notion of keyboard groups/layouts on grab events.
 */

typedef struct xhd_grab_t
{
	xhd_keycode_t  keycode;		// The keycode to grab
	xhd_modifier_t modifier;	// The modifier to grab

} xhd_grab_t;

/**
 * An XHD Grab List
 *
 * Grabbing key events from X passes no information about group/layout.
 * So, we register for keyboard group/layout events.
 * On such an event, we ungrab all previously grabbed keys and then
 * grab all keys associated with the new group/layout.
 * To do this efficiently, we keep a list of all the necessary keys
 * that need to be grabbed, aka, the grablist.
 */
typedef struct xhd_grablist_t
{
	uint32_t num_grabs;		// The number of grabs in the list
	uint32_t alloc_grabs;	// The number of allocated grab slots
	xhd_grab_t* list;		// The list

} xhd_grablist_t;

/**
 * An XHD Grab Map
 *
 * Maps group/layout id to the associated grablist.
 */
typedef xhd_grablist_t* xhd_grabmap_t;

/**
 * An XHD Mode
 *
 * This represents the working state of XHD.
 * Each mode has a mapping from keycodes, group/layout, and modifiers to actions
 * Each mode has a mapping from group/layout to grabs
 */
typedef struct xhd_mode_t
{
	xhd_grabmap_t grabs;		// The Grab Map
	xhd_keymap_t  keymap;		// The Key Map
	uint8_t       cur_group;	// Current Group/Layout

} xhd_mode_t;

#endif