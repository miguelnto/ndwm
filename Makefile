include config.mk

SRC = drw.c ndwm.c utils.c systray.c monitor.c xerror.c
OBJ = ${SRC:.c=.o}

all: options ndwm

options:
	@echo ndwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

ndwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ndwm ${OBJ} 

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ndwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ndwm

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ndwm

.PHONY: all options clean install uninstall
