#include <iostream>
#include <cstdlib>

#include "questions.h"

int main(int argc, const char* argv[])
{
	int status_number = 1;
	if(argc > 1)
	{
		try {
			status_number = std::stoi(argv[1]);
		}
		catch(std::exception& e)
		{
			std::cout << "Invalid argument! Usage: " << argv[0] << " [status_number]\n";
			return 1;
		}
	}

	chinese::questions q("status" + std::to_string(status_number) + ".txt");
	q.load_file("union_q.txt");

	q.init();

	// block mode is the shipped mode; CM_MIXED=1 (test harness) runs the
	// ungroupped gap mode instead, keeping tools/qdriver.py meaningful
	if(getenv("CM_MIXED"))
		q.ask_gap_mixed(std::max(0, q.cols()/7));
	else
		q.ask_gap_block15(std::max(0, q.cols()/7));

	return 0;
}
