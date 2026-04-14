cmake_minimum_required(VERSION 3.10)
project(chinese_memo)

find_package(Curses REQUIRED)

# Core library for common sources
add_library(core STATIC input_ncurses.cpp input_common.cpp utils.cpp tokenize.cpp)
target_link_libraries(core ${CURSES_LIBRARIES})

# Executable: input
add_executable(input main_input.cpp)
target_link_libraries(input core)

# Executable: input_notone
add_executable(input_notone main_input_notone.cpp)
target_link_libraries(input_notone core)

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

# Text-file processing steps from comp.sh
file(GLOB CONFIGURE_DEPENDS WORD_FILES RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_SOURCE_DIR}/words_*")

add_custom_target(union_q
    COMMAND bash -c "cat words_* | sort | uniq > union_q.txt"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(union_q_hun
    COMMAND bash -c "cat hun_words_* | sort | uniq > union_q_hun.txt"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(run_autoex
    COMMAND $<TARGET_FILE:autoex> ${WORD_FILES}
    DEPENDS autoex
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(union_a
    COMMAND bash -c "cat autoex_words_* | sort | uniq > union_a.txt"
    DEPENDS run_autoex
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(union_hsk
    COMMAND bash -c "cat hsk?.txt > union_hsk.txt"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(txt_ops ALL
    DEPENDS union_q union_q_hun union_a union_hsk
)