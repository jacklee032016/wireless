# $Id: Makefile,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
#
DEPTH=	..

BINDIR=	/usr/local/bin

include ${DEPTH}/Makefile.inc

ALL_INSTALL=	athstats 80211stats athdebug 80211debug
ALL=	${ALL_INSTALL} athkey athchans athctrl

all:	${ALL}

INCS+=	-I. -I${HAL} -I${HAL}/linux -I${ATH_HAL} -I${WLAN} -I${DEPTH}
CFLAGS=	${INCS} -g
LDFLAGS=

all:	${ALL}

athstats: athstats.c
	${CC} -o athstats ${CFLAGS} -I../ath ${LDFLAGS} athstats.c
	mv $@ ${SYMBOLSDIR}/

80211stats: 80211stats.c
	${CC} -o 80211stats ${CFLAGS} ${LDFLAGS} 80211stats.c
	mv $@ ${SYMBOLSDIR}/

athkey: athkey.c
	${CC} -o athkey ${CFLAGS} ${LDFLAGS} athkey.c
	mv $@ ${SYMBOLSDIR}/

athctrl: athctrl.c
	${CC} -o athctrl ${CFLAGS} ${LDFLAGS} athctrl.c
	mv $@ ${SYMBOLSDIR}/

athdebug: athdebug.c
	${CC} -o athdebug ${CFLAGS} ${LDFLAGS} athdebug.c
	mv $@ ${SYMBOLSDIR}/

80211debug: 80211debug.c
	${CC} -o 80211debug ${CFLAGS} ${LDFLAGS} 80211debug.c
	mv $@ ${SYMBOLSDIR}/

install: ${ALL_INSTALL}
	for i in ${ALL_INSTALL}; do \
		cp $$i ${DESTDIR}${BINDIR}; \
	done

clean:
	rm -f ${ALL} core a.out
