all: NewFSP_client NewFSP_server

NewFSP_client: NewFSP_client.o send_packet.o send_packet.h rdp.o rdp.h
	gcc -g NewFSP_client.o send_packet.o rdp.o -o NewFSP_client

NewFSP_server: NewFSP_server.o send_packet.o send_packet.h rdp.o rdp.h
	gcc -g NewFSP_server.o send_packet.o rdp.o -o NewFSP_server


NewFSP_client.o: NewFSP_client.c rdp.c rdp.h
	gcc -g -c NewFSP_client.c rdp.c




NewFSP_server.o: NewFSP_server.c rdp.c rdp.h
	gcc -g -c NewFSP_server.c rdp.c

  

clean:
	rm -f *.o  
