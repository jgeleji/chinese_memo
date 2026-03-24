#include <iostream>

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

	q.ask_all_until_fail(
		//[] (int i) -> double { return -0.1*i*i; },
		[] (int i) -> double { return 0.0; },
		std::max(0, q.cols()/7));

	return 0;
}
