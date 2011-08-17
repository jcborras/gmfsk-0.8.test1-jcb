## Process this file with automake to produce Makefile.in

SUBDIRS = misc samplerate mfsk rtty throb psk31 mt63 feld cw olivia dominoex

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/src/misc \
	-I$(top_srcdir)/src/samplerate

bin_PROGRAMS = gmfsk gmfsk_send

gmfsk_SOURCES = \
	main.c main.h			\
	support.c support.h		\
	interface.c interface.h		\
	callbacks.c callbacks.h		\
	waterfall.c waterfall.h		\
	miniscope.c miniscope.h		\
	papertape.c papertape.h		\
	gtkdial.c gtkdial.h		\
	ascii.h				\
	conf.c conf.h			\
	confdialog.c confdialog.h	\
	druid.c druid.h			\
	hamlib.c hamlib.h		\
	log.c log.h			\
	macro.c macro.h			\
	ptt.c ptt.h			\
	qsodata.c qsodata.h		\
	remlog.h			\
	snd.c snd.h			\
	trx.c trx.h			\
	cwirc.c cwirc.h			\
	picture.c picture.h		\
	server.c server.h		\
	sha1.c sha1.h

gmfsk_LDADD = \
	mfsk/libmfsk.a \
	mt63/libmt63.a \
	rtty/librtty.a \
	throb/libthrob.a \
	psk31/libpsk31.a \
	feld/libfeld.a \
	cw/libcw.a \
	olivia/libolivia.a \
	dominoex/libdominoex.a \
	misc/libmisc.a \
	samplerate/libsamplerate.a \
	@PACKAGE_LIBS@ $(INTLLIBS)

# Force linking with g++, otherwise the MT63 won't work...
# 
gmfsk_LINK = $(CXX) $(AM_FLAGS) $(FLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@


gmfsk_send_SOURCES = \
	gmfsk_send.c \
	sha1.c sha1.h

gmfsk_send_LDADD = \
        @PACKAGE_LIBS@ $(INTLLIBS)

gmfsk_send_LINK = $(CC) $(AM_FLAGS) $(FLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
