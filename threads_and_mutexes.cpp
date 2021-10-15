/*
    This program demonstrates some strategies for handling concurrent access to a shared doubly-linked list resource,
    for my 4th year Computer Systems course coursework task.
    Details of the task are as follows:
        1) Create a doubly-linked list with 140 nodes.
        2) Each node in the list should contain a random lowercase alphabetic string, with length 3-9 inclusive.
        3) Create a worker thread to concatenate all strings in the list and print out the concatenated string.
           The thread should repeat this until the list is empty.
        4) Create a second worker thread to select a node at random to delete, then sleep 500 ms before repeating.
           The thread should repeat until the list is empty.

    A rough outline of my strategy for handling concurrent access to the thread by the worker threads is:
        Each node includes a corresponding mutex.
        The list maintains a map of each thread's current position in the list.
        A thread must acquire the mutex for a node in order to update the thread's position to that node.
        Threads use hand-over-hand locking when traversing the list, acquiring a lock on the next node before
        position is updated and the previous node is released.
        To delete a node, synchronized locking is used to lock any previous/next nodes with the node to be deleted
        simultaneously.
*/
#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <cstdlib>
#include <ctime>
#include <string>

struct Node {
    std::string data;
    Node* next;
    Node* prev;
    std::mutex m;
};

class DoublyLinkedList {
public:
    DoublyLinkedList() {
        this->head = NULL;
        this->length = 0;
    }
    int get_length() { return this->length; }
    void insert_head(std::string data);
    std::string get_head_str();
    std::string get_next_str();
    void delete_node();

private:
    Node* head;
    int length;
    std::map<std::thread::id, Node*> thread_pos;
};

//
// DoubleLinkedList member functions
//

// Insert a new node at the head of the list
void DoublyLinkedList::insert_head(std::string data) {
    Node *node = new(Node);
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    if (this->head != NULL) {
        // point any previous head node to this node
        this->head->prev = node;
        node->next = this->head;
    }
    // this node becomes the new head
    this->head = node;
    this->length++;
}

// Initializes the thread to point (and lock) the head node in the list, and return the data string for that node
std::string DoublyLinkedList::get_head_str() {
    std::thread::id t = std::this_thread::get_id();

    // first acquire lock on node we're going to
    if (this->head != NULL) {
        this->head->m.lock();
    }
    this->thread_pos[t] = this->head;
    if (this->thread_pos[t] != NULL) {
        return this->thread_pos[t]->data;
    }
    // return empty string if list is empty
    return std::string();
}

// Move the current worker thread to the next node in the list and return its string data.
// Returns empty string once the thread has reached the end of the list.
// Uses hand-over-hand locking to cope with concurrent thread access to list.
std::string DoublyLinkedList::get_next_str() {
    std::thread::id t = std::this_thread::get_id();
    Node *current_node = this->thread_pos[t];
    if (current_node == NULL) {
        // thread is already at the end of the list, return empty string
        std::cout << "No thread position in list\n";
        return std::string();
    }
    Node *next_node = current_node->next;
    next_node = this->thread_pos[t]->next;
    if (next_node != NULL) {
        // acquire lock on node we're going to before updating thread position
        next_node->m.lock();
        // update thread position
        this->thread_pos[t] = next_node;
        // release lock on previous node
        current_node->m.unlock();
        return this->thread_pos[t]->data;
    }
    // thread is at the last node in the list
    // ensure current node is unlocked and return empty string
    this->thread_pos[t] = NULL;
    current_node->m.unlock();
    return std::string();
}

// Delete the node at the worker thread's current position.
// Uses synchronized locking to cope with concurrent thread access to list.
void DoublyLinkedList::delete_node() {
    std::thread::id t = std::this_thread::get_id();
    Node *current_node = this->thread_pos[t];

    if (current_node != NULL) {
        // release lock on current node to prevent deadlock when locking below
        current_node->m.unlock();

        Node *next_node = current_node->next;
        Node *prev_node = current_node->prev;

        // create unique lock for current node
        // use synchronised locking with dependent nodes for critical node deletion sections
        std::unique_lock<std::mutex> lock_this(current_node->m, std::defer_lock);

        if (next_node == NULL && prev_node == NULL) {
            // this is the only node in the list
            lock_this.lock();
            this->head = NULL;
        }
        else if (next_node == NULL) {
            // this is the last node in the list
            std::unique_lock<std::mutex> lock_prev(prev_node->m, std::defer_lock);
            std::lock(lock_prev, lock_this);
            prev_node->next = NULL;
        }
        else if (prev_node == NULL) {
            // this is the head node
            std::unique_lock<std::mutex> lock_next(next_node->m, std::defer_lock);
            std::lock(lock_this, lock_next);
            next_node->prev = NULL;
            this->head = next_node;
        }
        else {
            // this node has surrounding nodes
            std::unique_lock<std::mutex> lock_prev(prev_node->m, std::defer_lock);
            std::unique_lock<std::mutex> lock_next(next_node->m, std::defer_lock);
            std::lock(lock_prev, lock_this, lock_next);
            prev_node->next = next_node;
            next_node->prev = prev_node;
        }
    }
    // clear the thread's position in the list
    this->thread_pos[t] = NULL;
    // free dynamically-allocated memory for the node now there should be no references left to it
    delete current_node;
    // update list length
    this->length--;
}

// random string generator declaration
std::string get_random_str();

void worker_func_1(DoublyLinkedList& dll);
void worker_func_2(DoublyLinkedList& dll);

int main() {
    // cast time_t to unsigned int for random seed, to prevent warning
    std::srand((unsigned int)std::time(NULL));

    // initialize doubly linked list to start with 140 nodes
    DoublyLinkedList dll;
    const int total_nodes = 140;
    for (int i = 0; i < total_nodes; i++) {
        std::string data = get_random_str();
        dll.insert_head(data);
    }

    // start worker threads and wait until completion (for all nodes to be deleted)
    std::thread t1(worker_func_1, std::ref(dll));
    std::thread t2(worker_func_2, std::ref(dll));
    t1.join();
    t2.join();

    return 0;
}

// Return a random string of chars in {a-z} with length (3, 9)
std::string get_random_str() {
    int str_len = rand() % 7 + 3;
    std::string random_str;
    for (int i = 0; i < str_len; i++) {
        // pick a random char a-z
        char tmp = 'a' + rand() % 26;
        random_str.push_back(tmp);
    }
    return random_str;
}

// First worker thread - repeat: concatenate data from all nodes in list and print out at the end of the list
void worker_func_1(DoublyLinkedList& dll) {
    while (dll.get_length() > 0) {
        std::string concatenated;
        // initialize thread position and point
        std::string current = dll.get_head_str();
        // check for empty string to indicate when end of list has been reached
        while (!current.empty()) {
            concatenated += current;
            current = dll.get_next_str();
        }
        std::cout << "\nConcatenated thread: " << concatenated << "\n";
    }
    std::cout << "List empty: worker 1 stopping\n";
}

// Worker thread 2 - choose a node at random to delete from list, sleep for 500ms and repeat
void worker_func_2(DoublyLinkedList& dll) {
    while (dll.get_length() > 0) {
        // choose a node to delete from the list at random
        int pos_to_delete = rand() % dll.get_length();

        // initialize thread position and point to first node
        std::string tmp = dll.get_head_str();
        for (int i = 0; i < pos_to_delete; i++) {
            // use this to iterate over nodes until we reach the target node
            tmp = dll.get_next_str();
        }
        // delete the node at the current target position
        dll.delete_node();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "List empty: worker 2 stopping\n";
}
