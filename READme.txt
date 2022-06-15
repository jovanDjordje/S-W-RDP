DESCRIPTION

The Server: is sending an N number copies of the stated file to max N number of clients. Server ID is always 0.
The data about the clients is stored in the clients array.
Select function is used as a way to process/sort the inncoming packets as well as wait for a given
amount of time and re-send the packets which were not "ack-ed".
After a connection is accepted, file transfer is started immediately and continued until rdp_write return 0.

The Client: client is sending a connection request to the server, if response does not arrive within 1 sec, client exits.
If the connection is approved client is ready to write the data to the file.

STOP AND WAIT

Is implemented using two variables at the client side and one at server side. 
Client holds the data about the packet sequence number it has in one variable, 
and the next packet in the other, while the server only uses one variable for both purposes, 
which is updated only when acked by client.

MULTIPLEXING

Is achieved by storing the client data in "client" structs which are stored in the client array. 
Each client has his own id.

PROBLEMS:

1 and 2)
In the rdp_write method, i use select to see if the ACK packet has arrived. However, if the timer is 0,
it looks like the packets are never recieved, even though, client is sending ACK packets (loss_prob=0).
Thats why i set the timer to 100 microseconds there. And that is also the reason why i tried to
implement the same functionality in rdp_write_final using NON-Blocking recvfrom. However, while
testing, i was never able to get any data from recvfrom. Man pages state that the recvfrom
returns immediately in this case and I suspct that the data isnt arriving fast enough so I choose to cover both possibilities in the code. 
3) 
Using select() in the main Server loop to resend the packets after the timeout. I suspect this could lead to
starvation in case of too many connections. I have tried to implement the timeval field in client struct 
and keep the tab on time passed before a packet is sent again but i failed to test it. This is why it is not included.