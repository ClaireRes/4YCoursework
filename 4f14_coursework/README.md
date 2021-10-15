### Threads and Mutexes

This source file contains a simple C++ program for demonstrating the use of mutexes to 
handle access to a shared resource with multi-threaded code, as part of the Computer Systems course coursework from my university.

The program creates a doubly linked list, with nodes containing random alphabetical strings.
It spawns two worker threads, both with access to the list, to repeat the following until the list is empty and the program exits:

    1. Thread 1: iterate over the list from head->tail, concatenating all the values in the list.
        Print out the concatenated string once the end of the list is reached.
    2. Thread 2: randomly select a node to delete from the list. Sleep for 500 ms before repeating.

Since removal of a node is not atomic, locking strategies are needed to prevent another thread from accessing
a node while it is involved in a deletion.

This program uses mutexes and synchronised locking to address this problem as follows:
    1. All nodes in the list are assigned a mutex.
    2. The doubly linked list maintains a map of each thread's position in the list.
    3. In order to traverse the list, a thread must acquire a lock on a node via its mutex before it can move to that node's position.
    4. A thread will release the lock on the previous node, after it has acquired a lock on the node it is moving to i.e. hand-over-hand locking.
    5. Synchronized locks are used when deleting a node to lock the previous and next node in the list until the removal is complete.
