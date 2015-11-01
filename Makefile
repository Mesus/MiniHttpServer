CC = gcc
all : clean httpserver
.PHONY:all
httpserver : httpserver.c
	$(CC) -o httpserver httpserver.c -lpthread
.PHONY:clean
clean:
	@if [ -e "httpserver" ];then rm httpserver;fi
