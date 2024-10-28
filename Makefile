include config.mk

SRC = ${SRCDIR}/*.c
OBJ = ${SRC:.c=.o}

all: dirs options ${MAIN}

dirs:
	mkdir -p bin
	mkdir -p obj

options:
	@echo ${MAIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} ${SRC}
	mv -f ./*.o ${OBJDIR}

${MAIN}: ${OBJ}
	${CC} -o ${BIN}/$@ ${OBJDIR}/*.o ${LDFLAGS}

clean:
	rm -f ${BIN}/${MAIN} ${OBJDIR}/*.o

install: all
	mkdir -p ${DESTDIR}${INSTALLDIR}
	install -m 0755 ${BIN}/${MAIN} ${DESTDIR}${INSTALLDIR}/${MAIN}

uninstall:
	rm -f ${DESTDIR}${INSTALLDIR}/${MAIN}

.PHONY: all options clean install uninstall

