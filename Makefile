CC = cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags x11)
X11_LIBS := $(shell $(PKG_CONFIG) --libs x11)
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean check install uninstall
all: game_of_life

game_of_life: game_of_life.c life.c life.h figures.c figures.h figures_data.inc scene.c scene.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(X11_CFLAGS) $(LDFLAGS) -o $@ game_of_life.c life.c figures.c scene.c $(X11_LIBS) $(LDLIBS)

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
