cmake_minimum_required(VERSION 3.10)
project(chinese_memo)

find_package(Curses REQUIRED)

# Core library for common sources
add_library(core STATIC input_ncurses.cpp input_common.cpp utils.cpp tokenize.cpp)
target_link_libraries(core ${CURSES_LIBRARIES})

# Executable: input
add_executable(input main_input.cpp)
target_link_libraries(input core)

# Executable: questions
add_executable(questions main_questions.cpp questions.cpp)
target_link_libraries(questions core)

# Executable: questions10
add_executable(questions10 main_questions10.cpp questions.cpp)
target_link_libraries(questions10 core)

# Executable: questions10hsk
add_executable(questions10hsk main_questions10hsk.cpp questions.cpp)
target_link_libraries(questions10hsk core)

# Executable: questions_hun
add_executable(questions_hun main_questions_hun.cpp questions_hun.cpp)
target_link_libraries(questions_hun core)

# Executable: autoex (with debug flags)
add_executable(autoex autoex.cpp)
target_link_libraries(autoex core)
target_compile_options(autoex PRIVATE -g -ggdb -O0)

# Executable: characters
add_executable(characters main_characters.cpp questions.cpp)
target_link_libraries(characters core)

# Executable: characters_easy
add_executable(characters_easy main_characters_easy.cpp questions.cpp)
target_link_libraries(characters_easy core)

# Custom targets for data processing
add_custom_target(union_q
    COMMAND cat words_* | sort | uniq > union_q.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(union_q_hun
    COMMAND cat hun_words_* | sort | uniq > union_q_hun.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(run_autoex
    COMMAND ./autoex words_*
    DEPENDS autoex
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(union_a
    COMMAND cat autoex_words_* | sort | uniq > union_a.txt
    DEPENDS run_autoex
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(union_hsk
    COMMAND cat hsk?.txt > union_hsk.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)