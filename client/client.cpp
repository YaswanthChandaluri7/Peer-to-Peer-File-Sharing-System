#include<stdio.h>
#include<bits/stdc++.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib> 
#include <ctime> 
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <random> 
#include <string>
#include <sys/stat.h>
#include <openssl/sha.h>
#define CHUNK_SIZE 512 * 1024  
#define BUFFER_SIZE 1024

using namespace std;

struct ipport{
    string ip;
    int port;
};

size_t getFileSize(const string& filePath) {
    struct stat stat_buf;
    if (stat(filePath.c_str(), &stat_buf) == 0) {
        return stat_buf.st_size;
    }
    return 0; 
}
// struct PeerInfo {
//     string userId;
//     string ip;
//     int port;
// };

struct Chunk {
    string sha1hash;
    vector<char> data;
};

struct FileInfo {
    string filename;
    string overallsha;
    vector<Chunk> chunks;
};
struct filepeerinfo {
    string file_path;
    string overall_sha;
    vector<string> chunk_shas;
    vector<ipport> peers;  
};

//sha calcualtion of the file

string SHA1calculation(const char* data, size_t length) {
    unsigned char hashing[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data), length, hashing);

    stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hashing[i]);
    }
    return ss.str();
}

string SHA1calculation(const vector<char>& data) {
    return SHA1calculation(data.data(), data.size());  
}

string SHA1calculation(const string& data) {
    return SHA1calculation(data.c_str(), data.size());  // Call the raw data version
}

FileInfo processFile(const string &filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filePath << endl;
        exit(EXIT_FAILURE);
    }

    FileInfo fileinfo;
    fileinfo.filename = filePath;

    vector<char> chunkBuffer(CHUNK_SIZE);
    size_t bytesRead = 0;

    while (!file.eof()) {
        file.read(chunkBuffer.data(), CHUNK_SIZE);
        bytesRead = file.gcount();  

        if (bytesRead > 0) {
            
            vector<char> chunkData(chunkBuffer.begin(), chunkBuffer.begin() + bytesRead);

            Chunk chunk;
            chunk.sha1hash = SHA1calculation(string(chunkData.begin(), chunkData.end()));
            chunk.data = chunkData;
            fileinfo.chunks.push_back(chunk);
        }
    }

    
    string entireFileData;
    for (const auto& chunk : fileinfo.chunks) {
        entireFileData.append(chunk.data.begin(), chunk.data.end());
    }
    fileinfo.overallsha = SHA1calculation(entireFileData);

    file.close();
    return fileinfo;
}


vector<string> split(string input) {
    vector<string> arr;
    stringstream ss(input);
    string str;
    
    while (ss >> str) { 
        arr.push_back(str);
    }

    return arr;
}

vector<string> split2(string input, char delimiter) {
    vector<string> arr;
    size_t start = 0;  
    size_t pos = input.find(delimiter);

    while (pos != string::npos) {
        arr.push_back(input.substr(start, pos - start));
        start = pos + 1;
        pos = input.find(delimiter, start);
    }

    if (start < input.size()) {
        arr.push_back(input.substr(start));
    }

    return arr;
}

pair<string, string> split3(string str, char delimiter) {
    size_t pos = str.find(delimiter);
    if (pos == string::npos) return {str, ""}; 
    return {str.substr(0, pos), str.substr(pos + 1)};
}


struct trackeripandport{
    string ip;
    int port;
};

// information reading from tracker file
vector<trackeripandport> trackerfileopen(string filename){

    int fd=open(filename.c_str(),O_RDONLY);

    if(fd<0){
        cerr<<"failed to open"<<filename<<endl;
        return {};
    }
    char buffer[1024];
    ssize_t noofbytes=read(fd,buffer,sizeof(buffer)-1);

    buffer[noofbytes]='\0';

    string info(buffer);
    string dl="\n";
    ssize_t pos=0;
    vector<trackeripandport> trackers;
    while((pos=info.find(dl))!=string::npos){
        string str=info.substr(0,pos);

        ssize_t colon=str.find(":");

        if (colon != string::npos) {
            trackeripandport tracker;
            tracker.ip = str.substr(0, colon);
            tracker.port = stoi(str.substr(colon + 1));
            trackers.push_back(tracker);
        }
        info.erase(0,pos+dl.length());
    }
    return trackers;
}

void clientlistener(string ipclient, int portclient) {
    int listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd < 0) {
        cerr << "Failed to create listener socket" << endl;
        return;
    }

    struct sockaddr_in serveraddress;
    serveraddress.sin_family = AF_INET;
    serveraddress.sin_port = htons(portclient);
    serveraddress.sin_addr.s_addr = inet_addr(ipclient.c_str());

    if (bind(listener_fd, (struct sockaddr *)&serveraddress, sizeof(serveraddress)) < 0) {
        cerr << "Failed to bind listener socket" << endl;
        close(listener_fd);
        return;
    }

    if (listen(listener_fd, 10) < 0) {
        cerr << "Failed to listen on socket" << endl;
        close(listener_fd);
        return;
    }

    cout << "Listening for incoming peer connections on " << ipclient << ":" << portclient << endl;

    while (true) {
        int peer_fd;
        struct sockaddr_in peeraddress;
        socklen_t peeraddress_len = sizeof(peeraddress);

        if ((peer_fd = accept(listener_fd, (struct sockaddr *)&peeraddress, &peeraddress_len)) < 0) {
            cerr << "Failed to accept peer connection" << endl;
            continue;
        }

        cout << "Peer connected from IP: " << inet_ntoa(peeraddress.sin_addr) << " port: " << ntohs(peeraddress.sin_port) << endl;

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytes_received;
        while ((bytes_received = recv(peer_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            string request(buffer, bytes_received);
            cout << "Received request: " << request << endl;

            vector<string> requestParts = split2(request, ' ');
            if (requestParts.size() < 2) {
                cerr << "Invalid request format. Expected <filepath> <chunk_id>" << endl;
                break; // Exit loop on invalid format
            }

            string file_path = requestParts[0];
            int chunk_index = stoi(requestParts[1]);

            int file_fd = open(file_path.c_str(), O_RDONLY);
            if (file_fd < 0) {
                cerr << "Failed to open file: " << file_path << endl;
                break; // Exit loop on file open failure
            }

            off_t offset = lseek(file_fd, chunk_index * BUFFER_SIZE, SEEK_SET);
            if (offset == (off_t)-1) {
                cerr << "Error seeking to the correct chunk position in file" << endl;
                close(file_fd);
                break; // Exit loop on seek failure
            }

            vector<char> chunk_data(BUFFER_SIZE);
            ssize_t bytes_read = read(file_fd, chunk_data.data(), BUFFER_SIZE);
            if (bytes_read < 0) {
                cerr << "Failed to read file chunk" << endl;
                close(file_fd);
                break; // Exit loop on read failure
            }

            ssize_t bytes_sent = write(peer_fd, chunk_data.data(), bytes_read);
            if (bytes_sent < 0) {
                cerr << "Failed to send chunk data to peer" << endl;
            } else {
                cout << "Sent chunk " << chunk_index << " from file: " << file_path << endl;
            }

            close(file_fd);
        }

        if (bytes_received == 0) {
            cout << "Connection closed by peer." << endl;
        } else if (bytes_received < 0) {
            cerr << "Failed to receive chunk request from peer." << endl;
        }

        close(peer_fd);
    }

    close(listener_fd);
}




int connecttotracker(string &ip, int port) {
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "socket creation failed\n";
        return -1;
    }

    struct sockaddr_in serveraddress;
    serveraddress.sin_family = AF_INET;
    serveraddress.sin_port = htons(port);

    // convert ip address to binary form
    if (inet_pton(AF_INET, ip.c_str(), &serveraddress.sin_addr) <= 0) {
        cerr << "invalid ip address"<<endl;
        return -1;
    }

    // connect to the tracker
    if (connect(sock, (struct sockaddr *)&serveraddress, sizeof(serveraddress)) < 0) {
        cerr << "connection to tracker failed\n";
        return -1;
    }

    cout<< "connected to tracker at " << ip << ":" << port << "\n";
    return sock;
}



void downloadFile(int sock, const string& groupId, const string& fileName, const string& destinationPath) {
    string downloadCommand = "download_file " + groupId + " " + fileName + " " + destinationPath;
    if (send(sock, downloadCommand.c_str(), downloadCommand.size(), 0) < 0) {
        cerr << "Failed to send download command to tracker" << endl;
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived <= 0) {
        cerr << "Failed to receive response from tracker or connection closed" << endl;
        return;
    }
    buffer[bytesReceived] = '\0';

    string response(buffer);
    if (response.substr(0, 5) != "start") {
        cout << "Error: " << response << endl;
        return;
    }

    vector<string> parts = split2(response.substr(6), ' '); 
    if (parts.size() < 4) {
        cout << "Invalid response from tracker" << endl;
        return;
    }
    string filepath = parts[0];
    string overallSHA = parts[1];
    vector<string> chunkSHAs = split2(parts[2], ',');
    vector<string> peers = split2(parts[3], ',');
    vector<pair<string, string>> peersipandport;

    if (peers.empty()) {
        cerr << "No peers available for downloading the file." << endl;
        return;
    }

    for (const auto& peer : peers) {
        peersipandport.push_back(split3(peer, ':'));
    }

    cout << "File information received:" << endl;
    cout << "Overall SHA: " << overallSHA << endl;
    cout << "Number of chunks: " << chunkSHAs.size() << endl;
    cout << "Number of peers: " << peers.size() << endl;

    vector<vector<char>> fileChunks(chunkSHAs.size());

    srand(static_cast<unsigned int>(time(0))); // Seed for random number generation
    size_t currentFileSize = getFileSize(destinationPath);
    size_t lastchunksize=currentFileSize%CHUNK_SIZE;

    for (size_t chunk_id = 0; chunk_id < chunkSHAs.size(); ++chunk_id) {
        bool chunkVerified = false;
        int retries = 0;

        // Shuffle peers to randomize selection
        // random_shuffle(peersipandport.begin(), peersipandport.end());

        while (!chunkVerified && retries < 3) {
            // Use the first peer in the shuffled list
            //  int random = rand() % peers.size();
            int random=0;
            string peerIp = peersipandport[0].first;
            int peerPort = stoi(peersipandport[0].second);

            int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (clientSocket == -1) {
                cerr << "Error creating socket" << endl;
                retries++;
                continue;
            }

            sockaddr_in peerAddr;
            peerAddr.sin_family = AF_INET;
            peerAddr.sin_port = htons(peerPort);
            if (inet_pton(AF_INET, peerIp.c_str(), &peerAddr.sin_addr) <= 0) {
                cerr << "Invalid peer IP address" << endl;
                close(clientSocket);
                retries++;
                continue;
            }

            if (connect(clientSocket, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == -1) {
                cerr << "Error connecting to peer at " << peerIp << ":" << peerPort << endl;
                close(clientSocket);
                retries++;
                continue;
            }

            string chunkRequest = filepath + " " + to_string(chunk_id);
            if (send(clientSocket, chunkRequest.c_str(), chunkRequest.size(), 0) < 0) {
                cerr << "Failed to send chunk request" << endl;
                close(clientSocket);
                retries++;
                continue;
            }

            // Receive chunk size
            uint32_t chunk_size;
            ssize_t size_received = recv(clientSocket, &chunk_size, sizeof(chunk_size), 0);
            if (size_received != sizeof(chunk_size)) {
                cerr << "Failed to receive chunk size" << endl;
                close(clientSocket);
                retries++;
                continue;
            }
            chunk_size = ntohl(chunk_size);

            // Receive chunk data
            if(chunk_id==chunkSHAs.size()-1){
                chunk_size=lastchunksize;
            }
            vector<char> chunkData(chunk_size); 
            size_t total_received = 0;
            while (total_received < chunk_size) {
                ssize_t bytes_received = recv(clientSocket, chunkData.data() + total_received, chunk_size - total_received, 0);
                if (bytes_received <= 0) {
                    if (bytes_received == 0) {
                        cerr << "Connection closed by peer" << endl;
                    } else {
                        cerr << "Error receiving chunk data: " << strerror(errno) << endl;
                    }
                    close(clientSocket);
                    retries++;
                    break;
                }
                total_received += bytes_received;
            }

            if (total_received != chunk_size) {
                cerr << "Incomplete chunk received. Expected " << chunk_size << " bytes, got " << total_received << " bytes" << endl;
                close(clientSocket);
                retries++;
                continue;
            }

            string chunkSHA = SHA1calculation(chunkData.data(), chunkData.size());
            if (chunkSHA == chunkSHAs[chunk_id]) {
                fileChunks[chunk_id] = move(chunkData);
                chunkVerified = true;
                cout << "Chunk " << chunk_id << " verified and received. Size: " << chunk_size << " bytes" << endl;
            } else {
                cerr << "Chunk " << chunk_id << " verification failed. Expected SHA: " << chunkSHAs[chunk_id] 
                     << ", Received SHA: " << chunkSHA << ". Retrying..." << endl;
                retries++;
            }
            close(clientSocket);
        }

        if (!chunkVerified) {
            cerr << "Failed to verify chunk " << chunk_id << " after multiple attempts." << endl;
            return;
        }
    }

    ofstream outFile(destinationPath, ios::binary);
    if (!outFile.is_open()) {
        cerr << "Error opening destination file" << endl;
        return;
    }

    for (const auto& chunk : fileChunks) {
        outFile.write(chunk.data(), chunk.size());
        if (outFile.fail()) {
            cerr << "Error writing to file" << endl;
            outFile.close();
            return;
        }
    }
    outFile.close();
    cout << "File assembled successfully at: " << destinationPath << endl;

    
    ifstream verifyFile(destinationPath, ios::binary);
    if (!verifyFile.is_open()) {
        cerr << "Error opening file for verification" << endl;
        return;
    }

    string fileContents((istreambuf_iterator<char>(verifyFile)), istreambuf_iterator<char>());
    verifyFile.close();

    string fileSHA = SHA1calculation(fileContents.c_str(), fileContents.size());
    if (fileSHA == overallSHA) {
        cout << "File download complete and integrity verified." << endl;
    } else {
        cerr << "File integrity check failed. The downloaded file may be corrupted." << endl;
    }
}



vector<ipport> requestfile(int sock, const string& group_id, const string& file_name) {
    string request = "data " + group_id + " " + file_name;
    send(sock, request.c_str(), request.size(), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    buffer[bytes_received] = '\0';

    vector<ipport> clients;
    string response(buffer);
    stringstream ss(response);
    string peerinfo;

    while (getline(ss, peerinfo, ',')) {
        size_t pos = peerinfo.find(':');
        if (pos != string::npos) {
            ipport peer;
            peer.ip = peerinfo.substr(0, pos);
            peer.port = stoi(peerinfo.substr(pos + 1));
            clients.push_back(peer);
        }
    }
    return clients;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "You have wrong format in the command line" << endl;
        return -1;
    }

    string argv1 = argv[1];
    int pos = argv1.find(':');
    string ipclient = argv1.substr(0, pos);
    int portclient = stoi(argv1.substr(pos + 1));

    string trackeripport = argv[2];

    vector<trackeripandport> trackers = trackerfileopen(trackeripport);
    trackeripandport currenttracker = trackers[0];
    string ip = currenttracker.ip;
    int port = currenttracker.port;
    int sock = connecttotracker(ip, port);

    if (sock < 0) {
        cerr << "Failed to connect to tracker." << endl;
        return -1;
    }

    thread listenerthread(clientlistener, ipclient, portclient);

    string command;
    
    while (true) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        getline(cin, command);

        if (command == "quit") {
            break;  
        }

        if (command.substr(0, 5) == "login") {
            command += " " + ipclient + ":" + to_string(portclient);
            send(sock, command.c_str(), command.size(), 0);

            ssize_t recvfromtracker = recv(sock, buffer, BUFFER_SIZE, 0);
            if (recvfromtracker > 0) {
                buffer[recvfromtracker] = '\0';
                cout << buffer << endl;
            } else {
                cerr << "Failed to receive response from tracker" << endl;
            }
        }
        else if (command.substr(0, 13) == "download_file") {
            vector<string> tokens = split(command);
            if (tokens.size() != 4) {
                cout << "Usage: download_file <group_id> <file_name> <destination_path>" << endl;
                continue;
            }
            downloadFile(sock, tokens[1], tokens[2], tokens[3]);
        }
        else if (command.substr(0, 11) == "upload_file") {
            vector<string> arr = split(command);
            if (arr.size() != 3) {
                cout << "Usage: upload_file <file_path> <group_id>" << endl;
                continue;
            }
            string filepath = arr[1];
            string tempgroupid = arr[2];
            string groupid = "groupid " + tempgroupid;
            send(sock, groupid.c_str(), groupid.size(), 0);

            ssize_t recvfromtracker = recv(sock, buffer, BUFFER_SIZE, 0);
            if (recvfromtracker > 0) {
                buffer[recvfromtracker] = '\0';
                string res(buffer);

                if (res == "yes") {
                    FileInfo fileinfo = processFile(filepath);
                    string upload = "upload_file " + filepath + " " + tempgroupid + " " + fileinfo.overallsha + " ";
                    
                    for (size_t i = 0; i < fileinfo.chunks.size(); ++i) {
                        upload += fileinfo.chunks[i].sha1hash;
                        if (i != fileinfo.chunks.size() - 1) {
                            upload += ",";
                        }
                    }

                    send(sock, upload.c_str(), upload.size(), 0);

                    ssize_t bytesreceived = recv(sock, buffer, BUFFER_SIZE, 0);
                    if (bytesreceived > 0) {
                        buffer[bytesreceived] = '\0';
                        cout << "Response from tracker: " << buffer << endl;
                    } else {
                        cerr << "Failed to receive response from tracker" << endl;
                    }
                } else {
                    cout << "You are not a member of that group" << endl;
                }
            } else {
                cerr << "Failed to receive response from tracker" << endl;
            }
        }
        else {
            send(sock, command.c_str(), command.size(), 0);

            ssize_t recvfromtracker = recv(sock, buffer, BUFFER_SIZE, 0);
            if (recvfromtracker > 0) {
                buffer[recvfromtracker] = '\0';
                cout << buffer << endl;
            } else {
                cerr << "Failed to receive response from tracker" << endl;
            }
        }
    }

    close(sock);
    listenerthread.join();

    return 0;
}


