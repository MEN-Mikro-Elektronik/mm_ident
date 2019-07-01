CC=gcc

all: mm_ident

mm_ident: mm_ident.c
	$(CC) -static -o mm_ident mm_ident.c

clean:
	$(RM) mm_ident
