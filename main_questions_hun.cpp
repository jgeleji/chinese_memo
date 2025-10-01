#include <iostream>

#include "questions_hun.h"

int main()
{
	chinese::questions_hun q;
	q.load_file("union_q_hun.txt");

	q.init();

	q.ask_all_until_fail(std::max(0, q.cols()/7));

	return 0;
}
