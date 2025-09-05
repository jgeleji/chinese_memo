#!/bin/bash
#COMPILER=g++
COMPILER=clang++
${COMPILER} input.cpp main_input.cpp tokenize.cpp -lncurses -o input
${COMPILER} input.cpp main_questions.cpp tokenize.cpp questions.cpp -lncurses -o questions
${COMPILER} autoex.cpp tokenize.cpp input.cpp -lncurses -o autoex
${COMPILER} input.cpp main_characters.cpp tokenize.cpp questions.cpp -lncurses -o characters
${COMPILER} input.cpp main_characters_easy.cpp tokenize.cpp questions.cpp -lncurses -o characters_easy
cat words_* | sort | uniq > union_q.txt
./autoex words_*
cat autoex_words_* | sort | uniq > union_a.txt
