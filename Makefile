all:
	gcc main.c -lxcb -lxkbcommon -lxcb-xkb -lxkbcommon-x11 -o xhd

