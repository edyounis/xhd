// TODO
// Add Escape Characters
// Add Comments
// Add multi-key lines

#ifndef XHD_PARSE_LIB_H
#define XHD_PARSE_LIB_H

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define MAXBUFLEN	512
#define MAXCOMBOLEN	8
#define MAXKEYLEN	40
#define MAXNAMELEN	40
#define MAXFLAGLEN	40
#define MAXCMDLEN	256

/**
 * Parser State Object
 */
typedef struct parser_t
{
	FILE*    config;
	uint32_t lines_read;
	uint32_t buffer_index;
	uint32_t buffer_len;
	char     buffer[ MAXBUFLEN + 1 ];

} parser_t;

//* config_file = mode_entry | config_file, mode_entry ;
//* mode_entry = mode_name, '{', hotkey_list, '}' ;
//* mode_name = STRING_NO_WHITESPACE
//* hotkey_list = hotkey_entry | hotkey_list, hotkey_entry ;
//* hotkey_entry = keycombo, [flag_list], '{', command_list, '}' ;
//* keycombo = { modifier, "+" }, keysym ;
//* modifier = "shift" | "lock" | "ctrl" | "mod1" | "mod2" | "mod3" | "mod4" | "mod5" ;
//* keysym = STRING_NO_WHITESPACE
//* flag_list = flag | flag_list, flag ;
//* flag = "--", STRING_NO_WHITESPACE ;
//* command_list = command | command_list, NEWLINE, command ;
//* command = STRING

static inline
void xhd_config_print_error ( parser_t* parser )
{
	fprintf ( stderr, "Config parse error on line %d.\n", parser->lines_read );
}

static inline
int xhd_config_is_whitespace ( char c )
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline
int xhd_config_read_to_buf ( parser_t* parser )
{
	if ( parser->config == NULL )
	{
		fprintf( stderr, "Error reading file.\n" );
		return -1;
	}

	if ( feof( parser->config ) )
	{
		fprintf( stderr, "Unexpected end-of-file.\n" );
		return -1;
	}

	uint32_t newLen = fread( parser->buffer, sizeof(char), MAXBUFLEN, parser->config );

	if ( ferror( parser->config ) != 0 )
	{
		fprintf( stderr, "Error reading file.\n" );
		return -1;
	}

	parser->buffer_index = 0;
	parser->buffer_len   = newLen;

	parser->buffer[ newLen++ ] = '\0';
}

static inline
char xhd_config_read_char ( parser_t* parser )
{
	if ( parser->buffer_index >= parser->buffer_len )
	{
		if ( xhd_config_read_to_buf( parser ) )
			return 0;
	}

	char c = parser->buffer[ parser->buffer_index++ ];

	if ( c == '\n' )
		parser->lines_read++;

	return c;
}

static inline
char xhd_config_get_char ( parser_t* parser )
{
	if ( parser->buffer_index >= parser->buffer_len )
	{
		if ( xhd_config_read_to_buf( parser ) )
			return 0;
	}

	return parser->buffer[ parser->buffer_index ];
}

static inline
int xhd_config_expect ( parser_t* parser, char c )
{
	char d = xhd_config_read_char( parser );

	if ( c != d )
	{
		fprintf( stderr, "Expected: %c, Got: %c\n", c, d );
		xhd_config_print_error( parser );
		return -1;
	}

	return 0;
}

static inline
int xhd_config_trim_whitespace ( parser_t* parser )
{
	while ( xhd_config_is_whitespace( xhd_config_get_char( parser ) ) )
		xhd_config_read_char( parser );

	return 0;
}

static inline
xhd_modifier_t xhd_config_parse_modifier ( const char* modifier )
{
	if ( strncasecmp( modifier, "shift", 5 ) == 0 )
		return 1 << 0;
	if ( strncasecmp( modifier, "lock", 4 ) == 0 )
		return 1 << 1;
	if ( strncasecmp( modifier, "ctrl", 4 ) == 0 )
		return 1 << 2;
	if ( strncasecmp( modifier, "mod1", 4 ) == 0 )
		return 1 << 3;
	if ( strncasecmp( modifier, "mod2", 4 ) == 0 )
		return 1 << 4;
	if ( strncasecmp( modifier, "mod3", 4 ) == 0 )
		return 1 << 5;
	if ( strncasecmp( modifier, "mod4", 4 ) == 0 )
		return 1 << 6;
	if ( strncasecmp( modifier, "mod5", 4 ) == 0 )
		return 1 << 7;
	return -1;
}

static inline
xkb_keysym_t xhd_config_parse_keysym ( parser_t* parser, const char* keystring )
{
	xkb_keysym_t keysym = xkb_keysym_from_name( keystring, XKB_KEYSYM_CASE_INSENSITIVE );

	if ( keysym == 0 )
	{
		fprintf( stderr, "Failed to translate key: %s\n", keystring );
		return -1;
	}

	return keysym;
}

int xhd_config_parse_keycombo ( parser_t* parser )
{
	char     c;
	char     key_buf[MAXCOMBOLEN][MAXKEYLEN];
	uint32_t key_index = 0;
	uint32_t chr_index = 0;

	memset( key_buf[ key_index ], 0, MAXKEYLEN );

	// Split the combo into pieces stored in key_buf
	while ( 1 )
	{
		if ( chr_index >= MAXKEYLEN - 1 )
		{
			xhd_config_print_error( parser );
			return -1;
		}

		// Get next character
		c = xhd_config_read_char( parser );

		if ( xhd_config_is_whitespace( c ) )
		{
			// Skip whitespaces
			continue;
		}
		else if ( c == '+' )
		{
			// Start reading next modifier
			key_index++;
			chr_index = 0;
			memset( key_buf[key_index], 0, MAXKEYLEN );
			continue;
		}
		else if ( c == '-' || c == '{' )
		{
			// End State
			parser->buffer_index--;
			break;
		}

		key_buf[ key_index ][ chr_index++ ] = c;
	}

	// Parse the key_buf into modifiers
	xhd_modifier_t modifier = 0;

	int i;
	for ( i = 0; i < key_index; ++i )
	{
		xhd_modifier_t tmp = xhd_config_parse_modifier( key_buf[i] );

		if ( tmp == (xhd_modifier_t) -1 )
		{
			fprintf( stderr, "Failed parsing modifier: %s\n", key_buf[i] );
			return -1;
		}

		modifier |= tmp;
	}

	xkb_keysym_t keysym = xhd_config_parse_keysym( parser, key_buf[ key_index ] );

	printf("\t key: %d, %d\n", modifier, keysym );
	return 0;
}

int xhd_config_parse_flag ( parser_t* parser )
{
	char     c;
	char     flag_buf[ MAXFLAGLEN ];
	uint32_t flag_index = 0;

	memset( flag_buf, 0, MAXFLAGLEN );

	while ( 1 )
	{
		if ( flag_index >= MAXFLAGLEN - 1 )
		{
			xhd_config_print_error( parser );
			return -1;
		}

		// Get next character
		c = xhd_config_read_char( parser );

		if ( xhd_config_is_whitespace( c ) || c == '{' )
		{
			// End State
			parser->buffer_index--;
			break;
		}

		flag_buf[ flag_index++ ] = c;
	}

	printf( "\t%s\n", flag_buf );
	return 0;
}

int xhd_config_parse_flag_list ( parser_t* parser )
{
	xhd_config_trim_whitespace( parser );

	while ( xhd_config_get_char( parser ) != '{' )
	{
		if ( xhd_config_expect( parser, '-' ) )
			return -1;

		if ( xhd_config_expect( parser, '-' ) )
			return -1;

		if ( xhd_config_parse_flag( parser ) )
			return -1;

		xhd_config_trim_whitespace( parser );
	}
	return 0;
}

int xhd_config_parse_command ( parser_t* parser )
{
	char     c;
	char     cmd_buf[ MAXCMDLEN ];
	uint32_t cmd_index = 0;

	memset( cmd_buf, 0, MAXCMDLEN );

	while ( 1 )
	{
		if ( cmd_index >= MAXCMDLEN - 1 )
		{
			xhd_config_print_error( parser );
			return -1;
		}

		// Get next character
		c = xhd_config_read_char( parser );

		if ( c == '\r' )
		{
			// Skip carriage return
			continue;
		}
		else if ( c == '\n' )
		{
			// End State
			// Swallow the new line
			break;
		}
		else if ( c == '}' )
		{
			// End State
			parser->buffer_index--;
			break;
		}

		cmd_buf[ cmd_index++ ] = c;
	}

	printf( "\t\t%s\n", cmd_buf );
	return 0;
}

int xhd_config_parse_command_list ( parser_t* parser )
{
	xhd_config_trim_whitespace( parser );

	while ( xhd_config_get_char( parser ) != '}' )
	{

		if ( xhd_config_parse_command( parser ) )
			return -1;

		xhd_config_trim_whitespace( parser );
	}

	return 0;
}

int xhd_config_parse_hotkey_entry ( parser_t* parser )
{
	if ( xhd_config_parse_keycombo( parser ) )
		return -1;

	if ( xhd_config_parse_flag_list( parser ) )
		return -1;

	if ( xhd_config_expect( parser, '{' ) )
		return -1;

	if ( xhd_config_parse_command_list( parser ) )
		return -1;

	if ( xhd_config_expect( parser, '}' ) )
		return -1;

	return 0;
}

int xhd_config_parse_hotkey_list ( parser_t* parser )
{
	xhd_config_trim_whitespace( parser );

	while ( xhd_config_get_char( parser ) != '}' )
	{
		if ( xhd_config_parse_hotkey_entry( parser ) )
			return -1;

		xhd_config_trim_whitespace( parser );
	}

	return 0;
}

int xhd_config_parse_mode_name ( parser_t* parser )
{
	char     c;
	char     name_buf[ MAXNAMELEN ];
	uint32_t name_index = 0;

	memset( name_buf, 0, MAXNAMELEN );

	while ( 1 )
	{
		if ( name_index >= MAXNAMELEN - 1 )
		{
			xhd_config_print_error( parser );
			return -1;
		}

		// Get next character
		c = xhd_config_read_char( parser );

		if ( xhd_config_is_whitespace( c ) )
		{
			// Skip whitespaces
			continue;
		}
		else if ( c == '{' )
		{
			// End State
			parser->buffer_index--;
			break;
		}

		name_buf[ name_index++ ] = c;
	}

	printf( "%s\n", name_buf );
	return 0;
}

int xhd_config_parse_mode_entry ( parser_t* parser )
{
	if ( xhd_config_parse_mode_name( parser ) )
		return -1;

	if ( xhd_config_expect( parser, '{' ) )
		return -1;

	if ( xhd_config_parse_hotkey_list( parser ) )
		return -1;

	if ( xhd_config_expect( parser, '}' ) )
		return -1;

	return 0;
}

int xhd_config_parse_config_file ( parser_t* parser )
{
	while ( ! feof( parser->config ) )
	{
		xhd_config_trim_whitespace( parser );

		if ( xhd_config_parse_mode_entry( parser ) )
			return -1;
	}

	return 0;
}

/**
 * XHD Config Parse Function
 *
 * This is the start of the config file parser.
 * It opens the file, starts the parser, and closes the file.
 */
int xhd_config_parse ( void )
{
	parser_t parser;
	char config_path [256];

	// Open File
	char* config_home = getenv( "XDG_CONFIG_HOME" );

	if ( config_home != NULL )
	{
		snprintf( config_path, sizeof(config_path), "%s/%s", config_home, "xhd/config" );
	}
	else
	{
		config_home = getenv( "HOME" );

		if ( config_home == NULL )
		{
			fprintf( stderr, "Error; unable to find config file." );
			return -1;
		}

		snprintf( config_path, sizeof(config_path), "%s/%s", config_home, ".config/xhd/config" );
	}

	// FILE* config = fopen( config_path, "r" );
	FILE* config = fopen( "config", "r" );

	if ( config == NULL )
	{
		fprintf( stderr, "Error; cannot open config file.\n" );
		return -1;
	}

	parser.config       = config;
	parser.buffer_index = 0;
	parser.buffer_len   = 0;
	parser.lines_read   = 0;

	// Parse File
	if ( xhd_config_parse_config_file( &parser ) )
	{
		fprintf( stderr, "Failed to parse config file.\n" );
		return -1;
	}

	// Close File
	fclose( config );
	return 0;
}

#endif