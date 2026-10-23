We are doing a minimal implementation of ping in C programming.
You have the subject here named ft_ping.pdf, it contain all the instructions to follow, we don't do any bonus, only the mandatory part.
We are taking as reference the implementation of ping from inetutils-2.0 , we reproduce it's behavior but in a simplified implementation with some compromises (ceck Compromise.md for a list of the explicit compromises.
The Folder Documentation contains some reference data : the output of the man of ping from inetutils-2.0 and the real code of ping from inetutils-2.0.
Our project structure is simple : we have the header file at includes/ft_ping.h , and all the C files into srcs folder.

I made VM so can run the inetutils-2.0 and our ft_ping :

1. Ping from inetutils-2.0 :
bully@192.168.122.175
There is a VM running ping from inetutils-2.0 that you can access through ssh at bully@192.168.122.175 to be able to test ping of inetutils-2.0, don't hesitate to use it.
2. Our ft_ping
root@192.168.122.32
In this VM I cloned the current repo (our code) at /root/ping

Don't hesitate to run ft_ping and inetutils-2.0 ping in the 2 VM through ssh and compare their behavior when needed.
