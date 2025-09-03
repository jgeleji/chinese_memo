
#include <iostream>

#include <ncurses.h>
#include <curses.h>

#include "input.h"
#include "questions.h"

int main(int argc, const char* argv[])
{
	chinese::questions ques;
	ques.load_file("union_q.txt");
	ques.populate_chinese_char_to_index();

	initscr();			/* Start curses mode 		*/
	//raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
	//q.ask_1(0, questions::DATATYPE_CHINESE, questions::DATATYPE_PINYIN);

	int y, x;

	ques.ask_all_chinese_chars_easy(std::max(0, COLS/7));

	endwin();			/* End curses mode		  */
	return 0;
}
