TARGET := cli
GTK := gtk+-3.0
#GTK := gtk4

CFLAGS += -Iinclude $(shell pkg-config --cflags $(GTK))
LDFLAGS += $(shell pkg-config --libs $(GTK))

all: $(TARGET)

# GtkBuilder requires binary file to export all symbols
$(TARGET): $(wildcard *.c) $(wildcard nodes/*.c)
	$(CC) $(CFLAGS) -Wl,-export-dynamic -o $@ $^ $(LDFLAGS)
	@strip $@

.PHONY: clean
clean:
	-@rm -f $(TARGET)
