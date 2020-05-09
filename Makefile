include config.mk

SRC = main.c
OBJ = ${SRC:.c=.o}

.c.o:
	@echo CC -c $<
	@${CC} -c ${CFLAGS} $<

aud2: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f aud2 ${OBJ}
