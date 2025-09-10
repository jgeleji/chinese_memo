#include <iostream>

#include "questions.h"

int main()
{
	chinese::questions q;
	q.load_file("union_q.txt");

	q.init();

	q.ask_all_until_fail(std::max(0, q.cols()/7));

	return 0;
}
