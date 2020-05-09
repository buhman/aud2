# paths
PREFIX = /usr/local

# includes and libs
LIBS = -lavcodec -lavutil -lasound -lavformat -lswresample

# flags
CFLAGS = -Wall -Werror -Wextra -g
LDFLAGS = -s ${LIBS}

# compiler and linker
CC = cc
