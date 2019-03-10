Config Grammar:

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
```
default
{
	mod4+i --flag1 --flag2 --flag3
	{
		bspc node -f up
	}
	
	mod4+j
	{
		bspc node -f down
	}
	
	..
}
special_mode
{
	..
}
```
