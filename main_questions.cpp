#include <iostream>

#include "questions.h"

#include <ncurses.h>
#include <curses.h>

int main()
{
	questions q;
	q.load_file("union_q.txt");



	//input.write_dl_script();
	initscr();			/* Start curses mode 		*/
	//raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
	//q.ask_1(0, questions::DATATYPE_CHINESE, questions::DATATYPE_PINYIN);

	int y, x;

	q.ask_all_until_fail(std::max(0, COLS/7-3));

	endwin();			/* End curses mode		  */
	return 0;
}
