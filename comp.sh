#!/bin/bash
COMPILER=g++
#COMPILER=clang++
${COMPILER} input_ncurses.cpp input_common.cpp utils.cpp main_input.cpp tokenize.cpp -lncurses -o input
${COMPILER} input_ncurses.cpp input_common.cpp utils.cpp main_questions.cpp tokenize.cpp questions.cpp -lncurses -o questions
${COMPILER} input_ncurses.cpp input_common.cpp utils.cpp main_questions_hun.cpp tokenize.cpp questions_hun.cpp -lncurses -o questions_hun
${COMPILER} autoex.cpp tokenize.cpp input_ncurses.cpp input_common.cpp utils.cpp -lncurses -o autoex -g -ggdb -O0
${COMPILER} input_ncurses.cpp input_common.cpp utils.cpp main_characters.cpp tokenize.cpp questions.cpp -lncurses -o characters
${COMPILER} input_ncurses.cpp input_common.cpp utils.cpp main_characters_easy.cpp tokenize.cpp questions.cpp -lncurses -o characters_easy
cat words_* | sort | uniq > union_q.txt
cat hun_words_* | sort | uniq > union_q_hun.txt
./autoex words_*
cat autoex_words_* | sort | uniq > union_a.txt
