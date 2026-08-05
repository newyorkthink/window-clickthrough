CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -pipe
WARNINGS := -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS := -lX11 -lXext

BUILD_DIR := build
TARGET := $(BUILD_DIR)/window-clickthrough
SOURCES := src/main.c src/runtime.c src/x11.c
HEADERS := src/window-clickthrough.h
TEST_WINDOW := $(BUILD_DIR)/test-window
QUERY_SHAPE := $(BUILD_DIR)/query-shape

.PHONY: all clean check install

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_WINDOW): tests/test-window.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 $< -o $@ $(LDFLAGS) -lX11

$(QUERY_SHAPE): tests/query-shape.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 $< -o $@ $(LDFLAGS) $(LDLIBS)

check: $(TARGET) $(TEST_WINDOW) $(QUERY_SHAPE)
	bash tests/integration.sh

install: $(TARGET)
	install -Dm755 $(TARGET) "$(DESTDIR)/usr/bin/window-clickthrough"
	install -Dm644 packaging/window-clickthrough.desktop "$(DESTDIR)/usr/share/applications/window-clickthrough.desktop"
	install -Dm644 packaging/window-clickthrough.svg "$(DESTDIR)/usr/share/icons/hicolor/scalable/apps/window-clickthrough.svg"

clean:
	rm -rf $(BUILD_DIR) AppDir dist
