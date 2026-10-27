CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
WITH_XSS ?= $(shell $(PKG_CONFIG) --exists xscrnsaver 2>/dev/null && echo 1 || echo 0)
X11_PACKAGES = x11
ifeq ($(WITH_XSS),1)
X11_PACKAGES += xscrnsaver
endif
X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(X11_PACKAGES) 2>/dev/null)
X11_LIBS := $(shell $(PKG_CONFIG) --libs $(X11_PACKAGES) 2>/dev/null)
APP_CPPFLAGS = -DLIFE_HAVE_XSS=$(WITH_XSS) -DLIFE_VERSION='"$(shell cat VERSION)"'
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean check install uninstall check-deps
all: game_of_life

check-deps:
	@$(PKG_CONFIG) --exists $(X11_PACKAGES) || { echo 'Install X11 development packages; see docs/INSTALL.md (or use CMake).'; exit 1; }

game_of_life: game_of_life.c life.c life.h figures.c figures.h figures_data.inc scene.c scene.h journal.c journal.h VERSION Makefile | check-deps
	$(CC) $(CPPFLAGS) $(APP_CPPFLAGS) $(CFLAGS) $(X11_CFLAGS) $(LDFLAGS) -o $@ game_of_life.c life.c figures.c scene.c journal.c $(X11_LIBS) $(LDLIBS)

check: tests/life_test tests/figures_test tests/scene_test
	./tests/life_test
	./tests/figures_test
	./tests/scene_test

tests/scene_test: tests/scene_test.c scene.c scene.h life.c life.h figures.c figures.h figures_data.inc
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -I. -o $@ tests/scene_test.c scene.c life.c figures.c $(LDLIBS)

tests/life_test: tests/life_test.c life.c life.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -I. -o $@ tests/life_test.c life.c $(LDLIBS)

tests/figures_test: tests/figures_test.c tests/figures_expected.inc life.c life.h figures.c figures.h figures_data.inc
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -I. -o $@ tests/figures_test.c life.c figures.c $(LDLIBS)

install: game_of_life
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 game_of_life "$(DESTDIR)$(BINDIR)/game_of_life"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/game_of_life"

clean:
	rm -f game_of_life tests/life_test tests/figures_test tests/scene_test
