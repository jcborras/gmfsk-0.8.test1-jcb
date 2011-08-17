INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/misc

noinst_LIBRARIES = libdominoex.a

libdominoex_a_SOURCES = \
	dominoex.c dominoex.h			\
	dominoexrx.c				\
	dominoextx.c				\
	varicode.c varicode.h
