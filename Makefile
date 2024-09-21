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

analyze:
	cppcheck --enable=all --check-level=exhaustive --suppress=missingIncludeSystem src/

clean:
	rm -f ${BIN}/${MAIN} ${OBJDIR}/*.o

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m 0755 ${BIN}/${MAIN} ${DESTDIR}${PREFIX}/bin/${MAIN}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${MAIN}

.PHONY: all options clean install uninstall

