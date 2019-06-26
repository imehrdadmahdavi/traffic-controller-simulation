traffic_controller: traffic_controller.o list.o
	gcc -o traffic_controller -g traffic_controller.o list.o -lpthread -lm

traffic_controller.o: traffic_controller.c list.h
	gcc -g -c -Wall traffic_controller.c -D_POSIX_PTHREAD_SEMANTICS

list.o: list.c list.h
	gcc -g -c -Wall list.c

clean:
	@rm -f $(PROGRAMS) *.o *.gch core traffic_controller
