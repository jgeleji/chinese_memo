#include <iostream>

#include "questions.h"

int main()
{
	chinese::questions q("status2.txt");
	q.load_file("union_q.txt");
	q.load_file_hsk("union_hsk.txt");

	q.init();

	q.ask_all_until_fail_block10(std::max(0, q.cols()/7));

	return 0;
}
