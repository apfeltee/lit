
INCFLAGS =

WFLAGS = -Wunused -Wunused-macros -Wunused-local-typedefs

## used to find dead code. use sparingly.
#EXTRAFLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections
#EXTRAFLAGS = -fsanitize=address

## don't ever remove '-Wall -Wextra' - it's the bare minimum!
CC = clang  -Wall -Wextra $(EXTRAFLAGS) $(WFLAGS)

## only ever increase -O when in release. -O0 disables inlining entirely, which is needed for debugging.
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
CFLAGS = $(INCFLAGS) -O0 -g3 -ggdb 
LDFLAGS = -flto -ldl -lm  -lreadline -lpthread
target = outbin

srcfiles_all = $(wildcard *.c)
objfiles_all = $(srcfiles_all:.c=.o)
depfiles_all = $(objfiles_all:.o=.d)


$(target): $(objfiles_all)
	$(CC) -o $@ $(objfiles_all) $(LDFLAGS)

-include $(depfiles_all)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.o: %.c
	$(CC) $(CFLAGS) -c $(DBGFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(objfiles_all) $(target)

.PHONY: cleandep
cleandep:
	rm -f $(depfiles_all)

.PHONY: rebuild
rebuild: clean cleandep $(target)

