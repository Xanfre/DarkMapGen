objdir = ./objs

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin
DOCDIR = $(PREFIX)/share/doc/DarkMapGen
LICDIR = $(PREFIX)/share/licenses/DarkMapGen

TARGET ?=
ifeq ($(TARGET),)
CXX ?= g++
LD ?= g++
else
CXX = $(TARGET)-g++
LD = $(TARGET)-g++
endif

CXXFLAGS_ALL = -O2 -Wall -Wno-unused-result -Wno-format-security $(CXXFLAGS)
CPPFLAGS_ALL = -std=gnu++11 $(CPPFLAGS)
LDFLAGS_ALL = $(LDFLAGS)

LIBS = -lfltk -lfltk_images

TARGET_OS ?= $(OS)
ifeq ($(TARGET_OS),Windows_NT)
	LIBS += -lfltk_png -lz
ifeq ($(STATIC),)
	LIBS += -luuid -lgdi32
else
	LIBS += -lole32 -luuid -lcomctl32 -lgdi32 -static
endif
	BINEXT = .exe
else
	LIBS += -lpng -lz -lX11
	BINEXT =
endif

OBJS = $(objdir)/Fl_Cursor_Shape.o \
	$(objdir)/mapgen.o

ifneq ($(CUSTOM_FLTK),)
	CPPFLAGS_ALL += -DCUSTOM_FLTK
	OBJS += $(objdir)/Fl_Copy_Surface.o \
			$(objdir)/Fl_Image_Surface.o
endif

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

DarkMapGen$(BINEXT): $(OBJS)
	$(CXX) $(CXXFLAGS_ALL) $(LDFLAGS_ALL) $^ -o $@ $(LIBS)
