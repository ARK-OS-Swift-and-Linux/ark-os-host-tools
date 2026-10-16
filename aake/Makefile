# ==============================================================================
# ARK-OS Aake Bootstrap Makefile
# Compiles the standalone Aake build orchestrator binary using the host C compiler.
# Independent of Ninja, SwiftPM, and QEMU.
# ==============================================================================

CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -I include
LDFLAGS ?=

BIN_DIR ?= bin
TARGET = $(BIN_DIR)/aake

PREFIX ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin

SRCS = src/main.c \
       src/utils.c \
       src/ui.c \
       src/manifest.c \
       src/doctor.c \
       src/ninja_gen.c \
       src/ninja_runner.c \
       src/test_runner.c

OBJS = $(SRCS:.c=.o)

all: check-compiler $(TARGET)

check-compiler:
	@which $(CC) >/dev/null 2>&1 || { \
		echo "Error: Host C compiler '$(CC)' not found."; \
		echo "Please install gcc or clang, or specify CC=<compiler>."; \
		exit 1; \
	}

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

src/%.o: src/%.c include/aake.h include/utils.h include/ui.h include/manifest.h include/doctor.h include/ninja_gen.h include/ninja_runner.h include/test_runner.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(BIN_DIR) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@echo "[aake-bootstrap] Aake built successfully at $(TARGET)"

install: $(TARGET)
	mkdir -p $(DESTDIR)$(INSTALL_BIN)
	cp $(TARGET) $(DESTDIR)$(INSTALL_BIN)/aake
	chmod 755 $(DESTDIR)$(INSTALL_BIN)/aake
	@echo "[aake-bootstrap] Installed to $(DESTDIR)$(INSTALL_BIN)/aake"

uninstall:
	rm -f $(DESTDIR)$(INSTALL_BIN)/aake
	@echo "[aake-bootstrap] Removed from $(DESTDIR)$(INSTALL_BIN)/aake"

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf $(BIN_DIR)
	@echo "[aake-bootstrap] Cleaned bootstrap build artifacts."

.PHONY: all check-compiler install uninstall clean
