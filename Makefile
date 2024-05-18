CC ?= gcc
CFLAGS ?= -O3 -fPIC -Wall -Werror -pipe
LDFLAGS ?=

# Enable threading
PTHREAD_CFLAGS = -DENABLE_PTHREAD -pthread
PTHREAD_LDFLAGS = -pthread

DEFAULT = anagram is_spellable spellable qwantzle
all: anagram is_spellable spellable qwantzle

test: test_grammar
	./test_grammar

anagram: anagram.o letter_pool.o phrase_list.o sentence.o
	$(CC) -o $@ $+ $(LDFLAGS) $(PTHREAD_LDFLAGS)

anagwin: anagwin/anagwin.o anagwin/run.o anagwin/window.o \
letter_pool.o phrase_list.o sentence.o
	$(CC) -mwindows -o $@ $+ $(LDFLAGS) -lcomctl32

is_spellable: is_spellable.o letter_pool.o
	$(CC) -o $@ $+ $(LDFLAGS)

spellable: spellable.o letter_pool.o phrase_list.o
	$(CC) -o $@ $+ $(LDFLAGS)

qwantzle: qwantzle.o letter_pool.o phrase_list.o sentence.o grammar.o
	$(CC) -o $@ $+ $(LDFLAGS) $(PTHREAD_LDFLAGS)

test_grammar: test_grammar.o letter_pool.o phrase_list.o
	$(CC) -o $@ $+ $(LDFLAGS)

# Special rules for source files using pthreads
anagram.o: anagram.c
	$(CC) -c $(CFLAGS) $(PTHREAD_CFLAGS) -o $@ $<
qwantzle.o: qwantzle.c
	$(CC) -c $(CFLAGS) $(PTHREAD_CFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
anagwin/%.o: anagwin/%.c
	$(CC) -c $(CFLAGS) -I. -o $@ $<
test_grammar.o: grammar.c
	$(CC) -c $(CFLAGS) -DTEST -o $@ $<

clean:
	rm -f anagram is_spellable spellable qwantzle test_grammar *.exe *.o
	rm -f anagwin/*.o
