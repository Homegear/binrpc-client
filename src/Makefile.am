AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

bin_PROGRAMS = binrpc
binrpc_SOURCES = main.cpp
binrpc_LDADD = -lpthread -lhomegear-base -lgcrypt -lgnutls

if BSDSYSTEM
else
binrpc_LDADD += -ldl
endif
