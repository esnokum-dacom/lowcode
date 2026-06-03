CC      = gcc
TARGET  = lowcode 
SRCS    = main.c render/render.c editor/editor.c
CFLAGS  = -Wall -Wextra 
BINDIR  = $(HOME)/.local/bin

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

install: $(TARGET)
	mkdir -p $(BINDIR) 
	cp $(TARGET) $(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
