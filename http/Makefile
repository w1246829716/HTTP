#当前工作目录  用shell的方式执行了pwd命令 结果保存在WORK中
WORK_PATH=$(shell pwd)
BIN=http			#依赖的可执行程序
SRC=http.c			#依赖的源文件
CC=gcc
LDFLAGS=-lpthread -DDEBUG

$(BIN):$(SRC)
	$(CC) $^ -o $@ -std=c99 $(LDFLAGS) 

.PHONY:clean
clean:
	rm -f $(BIN)
