CC = clang++
FLAGS = -Wall -Wextra -pedantic -L ./raylib/linux/ -lraylib -lm
DISABLED_WARNINGS = -Wno-missing-field-initializers

main: src/main.cpp
	${CC} $^ ${FLAGS} ${DISABLED_WARNINGS} -o $@

clear:
	rm main

