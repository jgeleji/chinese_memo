#!/bin/bash
g++ input.cpp main_input.cpp tokenize.cpp -lncurses -o input
g++ input.cpp main_questions.cpp tokenize.cpp questions.cpp -lncurses -o questions
cat words_* | sort | uniq > union_q.txt
