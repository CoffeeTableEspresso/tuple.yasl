all: tuple.c
	gcc -c -Wall -Werror -fPIC -Iyasl -Iyasl/interpreter -Iyasl/util tuple.c -lyaslapi
	gcc -shared -o libtuple.so tuple.o -lyaslapi
