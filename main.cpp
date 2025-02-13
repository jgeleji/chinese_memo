#include <iostream>

#include "input.h"

#include <ncurses.h>
#include <curses.h>

int main()
{
	chinese::Input input;
	initscr();			/* Start curses mode 		*/
	//raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
	std::string inputted = input.do_input();

	endwin();			/* End curses mode		  */
	std::cout << "inputted=" << inputted << std::endl;
	return 0;
}
