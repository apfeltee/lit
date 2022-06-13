
INCFLAGS =

CC = clang -Wall -Wextra
# ricing intensifies
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
CFLAGS = $(INCFLAGS) -Og -g3 -ggdb3
LDFLAGS = -flto -ldl -lm  -lreadline -lpthread
target = run

src = \
	$(wildcard *.c) \

obj = $(src:.c=.o)
dep = $(obj:.o=.d)


$(target): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(dep)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.o: %.c
	$(CC) $(CFLAGS) -c $(DBGFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(obj) $(target)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: rebuild
rebuild: clean cleandep $(target)

.PHONY: sanity
sanity:
	./run sanity.msl