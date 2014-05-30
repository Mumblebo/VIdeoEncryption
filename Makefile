# build an executable named myprog from myprog.c
all: videoencryption.c
	gcc -Wall -lcrypto -o encryption.exe encryptionlibrary.c videoencryption.c

clean:
	$(RM) encryption.exe
