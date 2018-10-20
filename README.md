Current Config Grammar:

config_file = config_entry | config_file, config_entry ;
config_entry = key_combo, action ;
key_combo = { modifier, "+" }, keysym ;
modifier = "shift" | "lock" | "ctrl" | "mod1" | "mod2" | "mod3" | "mod4" | "mod5" ;
keysym = string_with_no_whitespace
action = string_with_no_newline

Current Config Example:

mod4+j bspc node -f left
mod4+k bspc node -f down
mod4+l bspc node -f right
mod4+i bspc node -f up

Desired Config Grammar:

config_file = mode_entry | config_file, mode_entry ;
mode_entry = mode_name, "{", hotkey_list, "}" ;
mode_name = STRING_NO_WHITESPACE
hotkey_list = hotkey_entry | hotkey_list, hotkey_entry ;
hotkey_entry = keycombo, [flag_list], "{", command_list, "}" ;
keycombo = { modifier, "+" }, keysym ;
modifier = "shift" | "lock" | "ctrl" | "mod1" | "mod2" | "mod3" | "mod4" | "mod5" ;
keysym = STRING_NO_WHITESPACE
flag_list = flag | flag_list, flag ;
flag = "--", STRING_NO_WHITESPACE ;
command_list = command | command_list, NEWLINE, command ;
command = STRING

Desired Config Example:

default
{
	mod4+c ....

	mod4+[i,j,k,l] --flag1 --flag2 --flag3
	{
		bspc node -f [left, down,right, up]
	}

	mod4+\[
	{
		do something1;
		do something2;
	}

	mod3 --modifier-only
	{

	}

	[_, shift]+[i,j,k,l]
	{
		bspc [node, desktop] -f [left, down, right, up]
	}
}
special
{
	// ***
}