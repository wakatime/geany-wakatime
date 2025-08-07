CC = gcc
SRCDIR = src
SOURCES = $(SRCDIR)/main.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = geany-wakatime.so

# Try to use pkg-config first, fall back to manual paths
GEANY_CFLAGS := $(shell pkg-config --cflags geany 2>/dev/null)
GEANY_LIBS := $(shell pkg-config --libs geany 2>/dev/null)

ifeq ($(GEANY_CFLAGS),)
    # Fallback paths for common installations
    HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifneq ($(HOMEBREW_PREFIX),)
        GEANY_CFLAGS = -I$(HOMEBREW_PREFIX)/include/geany -I$(HOMEBREW_PREFIX)/include/geany/scintilla -I$(HOMEBREW_PREFIX)/include/glib-2.0 -I$(HOMEBREW_PREFIX)/lib/glib-2.0/include -I$(HOMEBREW_PREFIX)/include/gtk-3.0 -I$(HOMEBREW_PREFIX)/include/cairo -I$(HOMEBREW_PREFIX)/include/pango-1.0 -I$(HOMEBREW_PREFIX)/include/gdk-pixbuf-2.0 -I$(HOMEBREW_PREFIX)/include/atk-1.0
        GEANY_LIBS = -L$(HOMEBREW_PREFIX)/lib -lgtk-3 -lglib-2.0 -lgobject-2.0
    else
        GEANY_CFLAGS = -I/usr/local/include/geany -I/usr/include/geany -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/gtk-3.0
        GEANY_LIBS = -lgtk-3 -lglib-2.0 -lgobject-2.0
    endif
endif

# Try to use pkg-config for libcurl, fall back to defaults
CURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS := $(shell pkg-config --libs libcurl 2>/dev/null)

ifeq ($(CURL_CFLAGS),)
    CURL_CFLAGS = -I/usr/include/curl -I/opt/homebrew/include
    CURL_LIBS = -lcurl
endif

CFLAGS = -Wall -fPIC $(GEANY_CFLAGS) $(CURL_CFLAGS)
LDFLAGS = -shared $(GEANY_LIBS) $(CURL_LIBS)

all: check-deps $(TARGET)

check-deps:
	@echo "Checking dependencies..."
	@if [ "$(shell uname -s)" = "Darwin" ]; then \
		if ! command -v geany >/dev/null 2>&1 && ! test -d /usr/local/Caskroom/geany; then \
			echo "ERROR: Geany is not installed."; \
			echo ""; \
			echo "On macOS, you need the formula version (not cask) for plugin development:"; \
			echo "  brew uninstall --cask geany  # if you have the cask version"; \
			echo "  brew install geany"; \
			echo ""; \
			exit 1; \
		elif test -d /usr/local/Caskroom/geany && ! command -v geany >/dev/null 2>&1; then \
			echo "ERROR: You have Geany cask installed, but need the formula version for plugin development."; \
			echo ""; \
			echo "Please run:"; \
			echo "  brew uninstall --cask geany"; \
			echo "  brew install geany"; \
			echo ""; \
			exit 1; \
		fi; \
	else \
		if ! command -v geany >/dev/null 2>&1; then \
			echo "ERROR: Geany is not installed."; \
			echo ""; \
			echo "On Ubuntu/Debian, install with:"; \
			echo "  sudo apt-get install geany geany-dev libgtk-3-dev"; \
			echo ""; \
			echo "On CentOS/RHEL, install with:"; \
			echo "  sudo yum install geany geany-devel gtk3-devel"; \
			echo ""; \
			exit 1; \
		fi; \
	fi
	@echo "Geany development environment ready"

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	@echo "Installing WakaTime plugin..."
	@if [ "$(shell uname -s)" = "Darwin" ]; then \
		mkdir -p "$(HOME)/.config/geany/plugins"; \
		cp $(TARGET) "$(HOME)/.config/geany/plugins/"; \
		echo "Plugin installed to $(HOME)/.config/geany/plugins/"; \
	else \
		mkdir -p "$(HOME)/.config/geany/plugins"; \
		cp $(TARGET) "$(HOME)/.config/geany/plugins/"; \
		echo "Plugin installed to $(HOME)/.config/geany/plugins/"; \
	fi
	@echo "Restart Geany and enable the WakaTime plugin in Tools -> Plugin Manager"

uninstall:
	@echo "Uninstalling WakaTime plugin..."
	@rm -f "$(HOME)/.config/geany/plugins/$(TARGET)"
	@echo "Plugin uninstalled"

.PHONY: all clean install uninstall check-deps
