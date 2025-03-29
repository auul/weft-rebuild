OUT := weft
LIBFLAGS := -lm -lreadline

CC := gcc
CFLAGS := -O3
SRCDIR := src
OBJDIR := build

SRCFILES := $(wildcard $(SRCDIR)/*.c)
OBJFILES := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCFILES))

all: $(OUT)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OUT): $(OBJDIR) $(OBJFILES)
	$(CC) -o $(OUT) $(LIBFLAGS) $(OBJFILES)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(OUT)
	./$(OUT)

clean:
	rm -rf $(OBJDIR)
	rm -f $(OUT)

.phony:
	all clean
