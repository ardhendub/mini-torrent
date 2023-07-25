AOS Assignment 3: P2P file sharing

The tool has been developed to be able to share files between peers in a local network.
Compile the client program as : g++ client.cpp -w -lcrypto -o client
Compile the tracker program as : g++ tracker.cpp -w -o tracker

the tracker program only takes one input "quit" to close the tracker.

using the client program operations like login, create group, upload_file and download_file can be performed.

Few points have been taken into consideration:
1. the tracker program will always be running.
2. there will always be atleast one peer available with all chunks of any file.
3. users from one system will always be using a particular login credentials to login and use the network.

to execute the tracker program, execute: ./tracker <portNumber>
to execute the client program, execute: ./client <portNumber of the client> <portNumber of the tracker>