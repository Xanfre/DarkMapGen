objdir = ./objs

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin
DOCDIR = $(PREFIX)/share/doc/DarkMapGen
LICDIR = $(PREFIX)/share/licenses/DarkMapGen

TARGET ?=
TARGET_OS ?= $(OS)

ifeq ($(TARGET),)
CXX ?= g++
else
CXX = $(TARGET)-g++
endif

CXXFLAGS_ALL = -O2 $(CXXFLAGS)
CPPFLAGS_ALL = -std=gnu++11 -Wall -Wextra -Wno-unused -Wno-unused-result $(CPPFLAGS)

ifeq ($(TARGET_OS),Windows_NT)
	CPPFLAGS += -Wno-cast-function-type
endif
ifeq ($(STATIC),)
	LDFLAGS_FLTK = $(shell fltk-config --use-images --ldflags) -lpng
else
	LDFLAGS_FLTK = $(shell fltk-config --use-images --ldstaticflags) -lpng
endif

LDFLAGS_ALL = $(LDFLAGS_FLTK) $(LDFLAGS)

ifeq ($(TARGET_OS),Windows_NT)
	BINEXT = .exe
else
	BINEXT =
endif

OBJS = $(objdir)/mapgen.o \
	$(objdir)/Fle_Colors.o \
	$(objdir)/Fle_Schemes.o

.PHONY: all install clean

all: $(objdir) DarkMapGen$(BINEXT)

install:
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DOCDIR)
	mkdir -p $(DESTDIR)$(LICDIR)
	install -p -m 755 DarkMapGen$(BINEXT) $(DESTDIR)$(BINDIR)
	install -p -m 644 DarkMapGen-ReadMe.txt $(DESTDIR)$(DOCDIR)
	install -p -m 644 license.txt $(DESTDIR)$(LICDIR)/LICENSE

clean:
	rm -rf $(objdir)
	rm -f DarkMapGen$(BINEXT)

$(objdir):
	mkdir -p $@

$(objdir)/%.o: %.cxx
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -c -o $@ $<

$(objdir)/%.o: %.cpp
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -c -o $@ $<

$(objdir)/mapgen.o: dmg_curs.h

DarkMapGen$(BINEXT): $(OBJS)
	$(CXX) $(CXXFLAGS_ALL) $^ -o $@ $(LDFLAGS_ALL)
