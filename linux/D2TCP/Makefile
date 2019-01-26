obj-m += tcp_d2tcp.o

all:
	make -C /home/xiangge/linux-4.16 M=$(PWD) modules 

clean:
	rm -rf ./*.ko ./*.o ./*.order ./*.symvers ./*.mod.c
