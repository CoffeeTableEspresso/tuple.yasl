all: tuple.c
	gcc -c -Wall -fPIC -Iyasl/src -Iyasl/src/common -Iyasl/src/interpreter -Iyasl/src/util tuple.c -lyaslapi
	gcc -shared -o libtuple.so tuple.o -lyaslapi
