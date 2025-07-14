#!/bin/bash
g++ input.cpp main_input.cpp tokenize.cpp -lncurses -o input
g++ input.cpp main_questions.cpp tokenize.cpp questions.cpp -lncurses -o questions
g++ autoex.cpp tokenize.cpp input.cpp -lncurses -o autoex
cat words_* | sort | uniq > union_q.txt
./autoex words_*
cat autoex_words_* | sort | uniq > union_a.txt
