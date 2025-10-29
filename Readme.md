# AOSAssignment-3

 ## Project Overview

This project implements a basic peer-to-peer (P2P) file-sharing system, where files are divided into chunks and shared across multiple peers. The system also interacts with a tracker to coordinate file sharing among the peers. The tracker stores metadata about the files and the list of peers who have these files. This project includes functionalities such as uploading files, chunking, calculating SHA-1 hashes for files and chunks, connecting to peers, and downloading files from peers.we have implemented in c++ language.

`tracker.cpp:`

`1. Libraries and Headers:`

The code includes standard C++ libraries and system calls related to networking (sys/socket.h, netinet/in.h, arpa/inet.h) and file operations.
It also includes OpenSSL's SHA library (<openssl/sha.h>) for hashing purposes.

`2. Global Data Structures`

userdetails: Stores user IDs and their associated passwords.

groups: Maps a group ID to the admin (user) of the group.

pendingrequests: Maps group IDs to sets of users who have requested to join the group.

groupmembers: Maps group IDs to sets of members in the group.

ipandport: Stores IP addresses and port numbers of logged-in users.

groupfiles: Maps group IDs to lists of shared file names.

groupfileinfo: Stores file-related information like file paths, SHA hashes, and the list of peers that have the file.

`3. helper  datatructures`

ipports: A structure to hold an IP and a port.
filepeerinfo: A structure that contains information about a file, including its overall SHA hash, chunk-wise SHAs, and a list of peers who have the file.

`4. Multithreading and Mutex Locks`

Multiple mutex locks are used to ensure thread safety when accessing shared resources, such as user details, groups, pending requests, group members, and IP addresses.

`5. Tracker Information;`

trackerinformation(): Reads tracker information (IP and port) from a file. This is useful for connecting clients to trackers in the network.

`6. User Operations`

createuser(): Allows the creation of a new user by providing a user ID and password. It checks if the user ID is unique.

loginuser(): Handles user login. The function also stores the user's IP address and port for future connections.

`7. Group Operations`

creategroup(): Allows logged-in users to create a group. The user creating the group automatically becomes the group admin.

joingroup(): Allows users to request to join a group. The request is added to a pending list.

leavegroup(): Allows a user to leave a group. If the user is an admin, either the group is deleted (if no other members exist) or a new admin is chosen.

listrequests(): Allows the admin to view all pending join requests for their group.

acceptrequest(): The admin of a group can accept a pending join request, adding the user to the group.

listallgroups(): Returns a list of all groups in the network.

`8. File Upload and Download`

uploadfile(): Handles the upload of a file to a group. The file's SHA hash and chunk-wise hashes are stored along with the IP and port of the user uploading the file.

handleDownloadRequest(): Manages a download request by checking if the file exists in the specified group and retrieving the necessary information for downloading.

`9. other Functions`
split(): Splits a string into a vector of words (used for parsing commands).

ipport(): Stores the IP and port of a client in the ipandport map.

`Setup Instructions:`

* Compile the program using a C++ compiler:

* `g++ -o tracker tracker_info.txt`


* Start the peer by running:

* `./tracker tracker_info.txt tracker_id`



`client.cpp:`

```Key Features:```

`File Chunking and SHA-1 Hash Calculation:`

Files are split into fixed-size chunks (512 KB each), and for each chunk, an SHA-1 hash is calculated to ensure data integrity.
The overall SHA-1 hash for the entire file is also calculated after processing all chunks.

`Peer-to-Peer File Transfer:`

A peer can request specific chunks of a file from other peers who have the file, facilitating distributed file sharing.
Each peer runs a listener that waits for incoming requests from other peers, allowing them to send requested file chunks.

`Tracker Communication:`

Peers communicate with a tracker to get metadata about files (such as overall SHA-1 hashes and chunk hashes) and information about other peers who have the file.

`Peer and Tracker Connection:`

The tracker and peers connect using socket programming over TCP/IP. The peer sends file download requests to the tracker, which responds with information about the available file chunks and the peers who hold those chunks.

`Components:`

`1. FileInfo and Chunk Structures`
FileInfo: Stores the filename, the overall SHA-1 hash of the file, and a list of its chunks.
Chunk: Stores the SHA-1 hash and the actual data of each chunk.
struct Chunk {
    string sha1hash;
    vector<char> data;
};

struct FileInfo {
    string filename;
    string overallsha;
    vector<Chunk> chunks;
};

`2. SHA-1 Hash Calculation`

The function SHA1calculation() computes the SHA-1 hash of a chunk or entire file. This hash is used to verify the integrity of the file during transfer.
`string SHA1calculation(const char* data, size_t length);`

`3. File Processing`

The function processFile() reads the file, splits it into chunks, computes the SHA-1 hash for each chunk, and calculates the overall file hash.
`FileInfo processFile(const string &filePath);`

`4. Tracker Communication`

Peers read the tracker’s IP and port information from a file and use this information to connect to the tracker. The connecttotracker() function establishes a TCP connection to the tracker.
int connecttotracker(string &ip, int port);

`5. Peer Communication`

Client Listener: clientlistener() starts a socket server that listens for incoming requests from other peers and sends requested chunks of a file.
Download Function: The downloadFile() function handles downloading a file from the tracker by requesting specific chunks from peers.
void clientlistener(string ipclient, int portclient);
void downloadFile(int sock, const string& groupId, const string& fileName, const string& destinationPath);

`6. Tracker File Parsing`

The trackerfileopen() function reads the tracker configuration file, which contains the IP addresses and port numbers of the tracker, and stores them for later use.

`vector<trackeripandport> trackerfileopen(string filename);`

`Setup Instructions:`

* Compile the program using a C++ compiler:

* `g++ -o client client.cpp -lssl -lcrypto`


* Start the peer by running:

* `./client ip:port tracker_info.txt`

