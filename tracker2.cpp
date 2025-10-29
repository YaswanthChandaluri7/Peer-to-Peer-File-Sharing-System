#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <sys/stat.h>
#include <fstream>
#include <unordered_map>
#include <set>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

// User and group management data structures
unordered_map<string, string> userdetails;
unordered_map<string, string> groups;
unordered_map<string, set<string>> pendingrequests;
unordered_map<string, set<string>> groupmembers;

// File sharing data structures
unordered_map<string, vector<string>> groupfiles; // groupid -> list of files
unordered_map<string, unordered_map<string, vector<string>>> filepeers; // groupid -> filename -> list of peers

#define BUFFER_SIZE 8192  // Increased buffer size for better performance
#define MAX_FILE_SIZE 1024*1024*100  // 100MB max file size

struct trackeripandport {
    string ip;
    int port;
};

struct FileInfo {
    string filename;
    long filesize;
    string checksum;
};

// Utility functions
vector<trackeripandport> trackerinformation(string filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "failed to open " << filename << endl;
        return {};
    }
    
    char buffer[1024];
    ssize_t noofbytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (noofbytes < 0) {
        cerr << "Error reading file" << endl;
        close(fd);
        return {};
    }
    
    buffer[noofbytes] = '\0';
    string info(buffer);
    string dl = "\n";
    ssize_t pos = 0;
    vector<trackeripandport> trackers;
    
    while ((pos = info.find(dl)) != string::npos) {
        string str = info.substr(0, pos);
        ssize_t colon = str.find(":");
        
        if (colon != string::npos) {
            trackeripandport tracker;
            tracker.ip = str.substr(0, colon);
            tracker.port = stoi(str.substr(colon + 1));
            trackers.push_back(tracker);
        }
        info.erase(0, pos + dl.length());
    }
    close(fd);
    return trackers;
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

// Send file size before file data
void sendFileSize(int socket, long filesize) {
    string sizeStr = to_string(filesize) + "\n";
    send(socket, sizeStr.c_str(), sizeStr.length(), 0);
}

// Send file data in chunks
bool sendFile(int socket, const string& filepath) {
    int filefd = open(filepath.c_str(), O_RDONLY);
    if (filefd < 0) {
        cerr << "Failed to open file: " << filepath << endl;
        return false;
    }
    
    // Get file size
    struct stat fileStat;
    if (fstat(filefd, &fileStat) < 0) {
        close(filefd);
        return false;
    }
    
    long filesize = fileStat.st_size;
    
    // Send file size first
    sendFileSize(socket, filesize);
    
    // Send file data in chunks
    char buffer[BUFFER_SIZE];
    long totalSent = 0;
    ssize_t bytesRead;
    
    while (totalSent < filesize && (bytesRead = read(filefd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytesSent = send(socket, buffer, bytesRead, 0);
        if (bytesSent < 0) {
            cerr << "Failed to send file data" << endl;
            close(filefd);
            return false;
        }
        totalSent += bytesSent;
    }
    
    close(filefd);
    return (totalSent == filesize);
}

// Receive file with proper chunking
bool receiveFile(int socket, const string& filepath) {
    // First receive file size
    char sizeBuffer[32];
    ssize_t sizeReceived = recv(socket, sizeBuffer, sizeof(sizeBuffer) - 1, 0);
    if (sizeReceived <= 0) {
        return false;
    }
    
    sizeBuffer[sizeReceived] = '\0';
    long expectedSize = stol(string(sizeBuffer));
    
    // Open file for writing
    int filefd = open(filepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (filefd < 0) {
        cerr << "Failed to create file: " << filepath << endl;
        return false;
    }
    
    // Receive file data in chunks
    char buffer[BUFFER_SIZE];
    long totalReceived = 0;
    
    while (totalReceived < expectedSize) {
        ssize_t remaining = min((long)BUFFER_SIZE, expectedSize - totalReceived);
        ssize_t bytesReceived = recv(socket, buffer, remaining, 0);
        
        if (bytesReceived <= 0) {
            close(filefd);
            return false;
        }
        
        ssize_t bytesWritten = write(filefd, buffer, bytesReceived);
        if (bytesWritten != bytesReceived) {
            close(filefd);
            return false;
        }
        
        totalReceived += bytesReceived;
    }
    
    close(filefd);
    return (totalReceived == expectedSize);
}

// User management functions
string createuser(string str) {
    vector<string> arr = split(str);
    if (arr.size() != 2) return "Invalid format: create_user <userid> <password>";
    
    string userid = arr[0];
    string password = arr[1];
    
    if (userdetails.find(userid) == userdetails.end()) {
        userdetails[userid] = password;
        return "User " + userid + " created successfully";
    } else {
        return "User " + userid + " already exists";
    }
}

string loginuser(string str, string& userid) {
    vector<string> arr = split(str);
    if (arr.size() != 2) return "Invalid format: login <userid> <password>";
    
    string str1 = arr[0];
    string str2 = arr[1];
    
    if (userdetails.find(str1) != userdetails.end() && userdetails[str1] == str2) {
        userid = str1;
        return "User " + str1 + " logged in successfully";
    } else {
        return "Invalid userid or password";
    }
}

string creategroup(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 1) return "Invalid format: create_group <groupid>";
    
    string groupid = arr[0];
    
    if (groups.find(groupid) != groups.end()) {
        return "Group " + groupid + " already exists";
    } else {
        groups[groupid] = userid;
        groupmembers[groupid].insert(userid);
        pendingrequests[groupid];
        groupfiles[groupid]; // Initialize file list for group
        return "Group " + groupid + " created successfully";
    }
}

string joingroup(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 1) return "Invalid format: join_group <groupid>";
    
    string groupid = arr[0];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groupmembers[groupid].find(userid) != groupmembers[groupid].end()) {
        return "You are already a member of group " + groupid;
    }
    
    pendingrequests[groupid].insert(userid);
    return "Join request sent to group " + groupid;
}

string leavegroup(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 1) return "Invalid format: leave_group <groupid>";
    
    string groupid = arr[0];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groupmembers[groupid].find(userid) == groupmembers[groupid].end()) {
        return "You are not a member of group " + groupid;
    }
    
    groupmembers[groupid].erase(userid);
    
    if (groups[groupid] == userid) {
        if (groupmembers[groupid].empty()) {
            groups.erase(groupid);
            pendingrequests.erase(groupid);
            groupmembers.erase(groupid);
            groupfiles.erase(groupid);
            filepeers.erase(groupid);
            return "Left group " + groupid + ". Group deleted as no members remain.";
        } else {
            string new_admin = *groupmembers[groupid].begin();
            groups[groupid] = new_admin;
            return "Left group " + groupid + ". " + new_admin + " is the new admin.";
        }
    }
    
    return "Left group " + groupid + " successfully";
}

string listrequests(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 1) return "Invalid format: list_requests <groupid>";
    
    string groupid = arr[0];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groups[groupid] != userid) {
        return "You are not the admin of group " + groupid;
    }
    
    if (pendingrequests[groupid].empty()) {
        return "No pending requests for group " + groupid;
    }
    
    string result = "Pending requests for group " + groupid + ":\n";
    for (const string& user : pendingrequests[groupid]) {
        result += user + "\n";
    }
    return result;
}

string acceptrequest(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 2) return "Invalid format: accept_request <groupid> <userid>";
    
    string groupid = arr[0];
    string otheruser = arr[1];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groups[groupid] != userid) {
        return "You are not the admin of group " + groupid;
    }
    
    if (pendingrequests[groupid].find(otheruser) == pendingrequests[groupid].end()) {
        return "No pending request from user " + otheruser;
    }
    
    pendingrequests[groupid].erase(otheruser);
    groupmembers[groupid].insert(otheruser);
    return "User " + otheruser + " added to group " + groupid + " successfully";
}

string listallgroups(string& userid) {
    if (groups.empty()) {
        return "No groups created in the network";
    }
    
    string result = "Available groups:\n";
    for (const auto& group : groups) {
        result += group.first + "\n";
    }
    return result;
}

// File sharing functions
string uploadfile(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 2) return "Invalid format: upload_file <groupid> <filename>";
    
    string groupid = arr[0];
    string filename = arr[1];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groupmembers[groupid].find(userid) == groupmembers[groupid].end()) {
        return "You are not a member of group " + groupid;
    }
    
    // Add file to group's file list
    auto& files = groupfiles[groupid];
    if (find(files.begin(), files.end(), filename) == files.end()) {
        files.push_back(filename);
    }
    
    // Add user as a peer for this file
    filepeers[groupid][filename].push_back(userid);
    
    return "File " + filename + " uploaded to group " + groupid + " successfully. Ready for sharing.";
}

string downloadfile(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 3) return "Invalid format: download_file <groupid> <filename> <destination>";
    
    string groupid = arr[0];
    string filename = arr[1];
    string destination = arr[2];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groupmembers[groupid].find(userid) == groupmembers[groupid].end()) {
        return "You are not a member of group " + groupid;
    }
    
    // Check if file exists in group
    auto& files = groupfiles[groupid];
    if (find(files.begin(), files.end(), filename) == files.end()) {
        return "File " + filename + " not found in group " + groupid;
    }
    
    // Get list of peers who have this file
    auto& peers = filepeers[groupid][filename];
    if (peers.empty()) {
        return "No peers available for file " + filename;
    }
    
    string result = "Available peers for " + filename + " in group " + groupid + ":\n";
    for (const string& peer : peers) {
        result += peer + "\n";
    }
    result += "Connect to any peer to download the file.";
    
    return result;
}

string listfiles(string str, string& userid) {
    if (userid.empty()) return "Please login first";
    
    vector<string> arr = split(str);
    if (arr.size() != 1) return "Invalid format: list_files <groupid>";
    
    string groupid = arr[0];
    
    if (groups.find(groupid) == groups.end()) {
        return "Group " + groupid + " does not exist";
    }
    
    if (groupmembers[groupid].find(userid) == groupmembers[groupid].end()) {
        return "You are not a member of group " + groupid;
    }
    
    auto& files = groupfiles[groupid];
    if (files.empty()) {
        return "No files shared in group " + groupid;
    }
    
    string result = "Files in group " + groupid + ":\n";
    for (const string& file : files) {
        result += file + " (peers: " + to_string(filepeers[groupid][file].size()) + ")\n";
    }
    return result;
}

void processrequest(int clientsocket) {
    string userid = "";
    
    while (true) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesreceived = recv(clientsocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesreceived > 0) {
            buffer[bytesreceived] = '\0';
            string str(buffer);
            
            ssize_t pos = str.find(' ');
            string command = str.substr(0, pos);
            string args = (pos != string::npos) ? str.substr(pos + 1) : "";
            
            string response;
            
            if (command == "create_user") {
                response = createuser(args);
            }
            else if (command == "login") {
                response = loginuser(args, userid);
            }
            else if (command == "create_group") {
                response = creategroup(args, userid);
            }
            else if (command == "join_group") {
                response = joingroup(args, userid);
            }
            else if (command == "leave_group") {
                response = leavegroup(args, userid);
            }
            else if (command == "list_requests") {
                response = listrequests(args, userid);
            }
            else if (command == "accept_request") {
                response = acceptrequest(args, userid);
            }
            else if (command == "list_groups") {
                response = listallgroups(userid);
            }
            else if (command == "upload_file") {
                response = uploadfile(args, userid);
            }
            else if (command == "download_file") {
                response = downloadfile(args, userid);
            }
            else if (command == "list_files") {
                response = listfiles(args, userid);
            }
            else if (command == "logout") {
                userid = "";
                response = "Logged out successfully";
            }
            else if (command == "exit") {
                cout << "Client requested to terminate the session." << endl;
                break;
            }
            else {
                response = "Unknown command. Available commands: create_user, login, create_group, join_group, leave_group, list_requests, accept_request, list_groups, upload_file, download_file, list_files, logout, exit";
            }
            
            cout << "Response: " << response << endl;
            
            // Send response with proper size handling
            ssize_t totalSent = 0;
            ssize_t responseSize = response.size();
            
            while (totalSent < responseSize) {
                ssize_t sent = send(clientsocket, response.c_str() + totalSent, responseSize - totalSent, 0);
                if (sent < 0) {
                    cerr << "Failed to send response" << endl;
                    break;
                }
                totalSent += sent;
            }
        }
        else if (bytesreceived == 0) {
            cout << "Client disconnected." << endl;
            break;
        }
        else {
            cerr << "Failed to receive client request." << endl;
            break;
        }
    }
    
    close(clientsocket);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./tracker tracker_info.txt tracker_id" << endl;
        return -1;
    }
    
    string trackerfile = argv[1];
    int trackerid = stoi(argv[2]);
    
    vector<trackeripandport> trackers = trackerinformation(trackerfile);
    
    if (trackers.empty() || trackerid <= 0 || trackerid > (int)trackers.size()) {
        cerr << "Invalid tracker ID" << endl;
        return -1;
    }
    
    trackeripandport currenttracker = trackers[trackerid - 1];
    
    cout << "Starting tracker on " << currenttracker.ip << ":" << currenttracker.port << endl;
    
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        cerr << "Socket creation failed" << endl;
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        cerr << "Set socket options error" << endl;
        return -1;
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(currenttracker.ip.c_str());
    address.sin_port = htons(currenttracker.port);
    
    if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed" << endl;
        return -1;
    }
    
    if (listen(fd, 10) < 0) {
        cerr << "Listen failed" << endl;
        return -1;
    }
    
    cout << "Tracker server listening for connections..." << endl;
    
    while (true) {
        struct sockaddr_in clientaddress;
        socklen_t clientaddresslength = sizeof(clientaddress);
        
        int clientsocket = accept(fd, (struct sockaddr*)&clientaddress, &clientaddresslength);
        if (clientsocket < 0) {
            cerr << "Accept failed" << endl;
            continue;
        }
        
        cout << "Client connected from " << inet_ntoa(clientaddress.sin_addr) 
             << ":" << ntohs(clientaddress.sin_port) << endl;
        
        thread clientthread(processrequest, clientsocket);
        clientthread.detach();
    }
    
    close(fd);
    return 0;
}