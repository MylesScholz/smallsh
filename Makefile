CC = gcc
CFLAGS := --std=gnu99

BINDIR = .
exe_file = "movies_by_year"

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CFLAGS += -g -Wall
else
	CFLAGS += -DNDEBUG -03
endif

SRCDIR = .
BUILDDIR = build
SRCEXT = c
SRCS = $(shell find $(SRCDIR) -type f -name "*.$(SRCEXT)")
OBJS = $(patsubst $(SRCDIR)/%, $(BUILDDIR)/%, $(SRCS:.$(SRCEXT)=.o))
DEP = $(OBJS:.o=.d)

all: $(exe_file)

$(exe_file): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) -o $(exe_file) $^ $(LIB) $(LDFLAGS) -lm

$(BUILDDIR)/%.d: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@$(CC) $(INC) $< -MM -MT $(@:.d=.o) >$@

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $< -w

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(exe_file)

-include $(DEP)

