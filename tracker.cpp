#include<stdio.h>
#include<bits/stdc++.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <openssl/sha.h>
using namespace std;
//userid, password
unordered_map<string ,string> userdetails;
//groupid,admin
unordered_map<string,string> groups;
//groupid,pendingrequests
unordered_map<string,set<string>> pendingrequests;
//groupid,members
unordered_map<string,set<string>> groupmembers;

//ip and port
unordered_map<string,pair<string,string>> ipandport;

//groupid files
unordered_map<string, vector<string>> groupfiles;
//file information
// struct fileinfo {
//     string filepath;               
//     string overallSHA;             
//     vector<string> chunkwisesha;      
// };
struct ipports{
    string ip;
    string port;
};

struct filepeerinfo{
    string filepath;
    string overallsha;
    vector<string> chunkshas;
    vector<ipports> peers;
};


// filename,(filepath,overallsha,chunkwisesha,ipports)
// map<string, fileinfo> file_map;

//  peers associated with each file
// map<string, vector<string>> peer_map;

//  peer IP and Port information
// map<string,ipports> peer_info_map;
//<group,<filoename,fileinfo>>
unordered_map<string, map<string, filepeerinfo>> groupfileinfo;
//<groupid,<filename,vector<userids>>>
// unordered_map<string,map<string,vector<string>>> groupidfileinfo;

#define BUFFER_SIZE 1024 

//mutex lock variables
mutex userdetails_mutex;
mutex groups_mutex;
mutex pendingrequests_mutex;
mutex groupmembers_mutex;
mutex ipandport_mutex;
//tracker file information datatype
struct trackeripandport{
    string ip;
    int port;
};


// information reading from tracker file
vector<trackeripandport> trackerinformation(string filename){
    int fd=open(filename.c_str(),O_RDONLY);
    if(fd<0){
        cerr<<"failed to open"<<filename<<endl;
        return {};
    }
    char buffer[1024];
    ssize_t noofbytes=read(fd,buffer,sizeof(buffer)-1);

    if (noofbytes < 0) {
    cerr << "Error reading file" << endl;
    close(fd);
    return {};
    }

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
    close(fd);
    return trackers;
}
vector<string> split( string input) {
    vector<string> arr;
    stringstream ss(input);
    string str;
    
    while (ss >> str) { 
        arr.push_back(str);
    }

    return arr;
}

string createuser(string str){

    string delimiter = " ";

    size_t pos = 0;
    string userid="";
    string password="";
    // int count=0;

    vector<string> arr=split(str);
    userid=arr[0];
    password=arr[1];

    if(arr.size()>2) return "you have wrong input format";

    lock_guard<mutex> lock(userdetails_mutex);
    
    
    if(userdetails.find(userid)==userdetails.end()){
        userdetails[userid]=password;
        string res="User with userId "+password+" is successfully completed authentication";
        return res;
    }
    else{
        return "User with userId "+userid+" is already completed authentication";
    }
    return "";

}

string loginuser(string str,string& userid){
    string delimiter = " ";

    size_t pos = 0;
    string str1="";
    string str2="";
    string str3="";
    int count=0;

    vector<string> arr=split(str);

    if(arr.size()>3) return "you have wrong input format";
    str1=arr[0];
    str2=arr[1];
    str3=arr[2];
    userid=str1;
    lock_guard<mutex> lock(userdetails_mutex); 
    if(userdetails.find(str1)!=userdetails.end()&&userdetails[str1]==str2){
        userdetails[str1]=str2;
         pos=str3.find(':');
        string ip=str3.substr(0,pos);
        string port=(str3.substr(pos+1));
        lock_guard<mutex> lock(ipandport_mutex);
        ipandport[userid]={ip,port};
        string res="user with userId "+str1+" is  successfully loggedin";
        return res;
    }
    else{
        return "you may give wrong userid and password";
    }
    return "";
}

string creategroup(string str,string& userid){
    if(userid.size()==0) return "You did not loggedin";

    string delimiter = " ";

    size_t pos = 0;
    string str1="";
    int count=0;

    vector<string> arr=split(str);

    if(arr.size()>1){
        return "You have given wrong format create_group and groupid";
    }
    str1=arr[0];
    lock_guard<mutex> lock(groups_mutex);
    
    if(groups.find(str)!=groups.end()){
        return str1+" already created by someone";
    }
    else{
        groups[str1]=userid;
        groupmembers[str1].insert(userid);
        pendingrequests[str1];
        return str1+" has created successfully";
    }
    
    return "";
}

string joingroup(string str,string& userid){

    if(userid.empty()) 
        return "You did not loggedin";

    string delimiter =" ";

    size_t pos = 0;
    string str1="";
    int count=0;

    vector<string> arr=split(str);

    if(arr.size()>1){
        return "You have given wrong format create_group and groupid";
    }
    str1=arr[0];
    {
    lock_guard<mutex> lock(groups_mutex);
    if(groups.find(str1)==groups.end()){
        return str1+" group does not exits";
    }
    }

    if(groups[str1]==userid){
        return "You are already a member in the group";
    }
    {
        lock_guard<mutex> lock2(pendingrequests_mutex);
        pendingrequests[str1].insert(userid);
        return "your request has sended successfully to the group "+str1;
    }
    return "";
}

string leavegroup(string str,string& userid){

    if(userid.size()==0) return "You did not loggedin";

    string delimiter = " ";

    size_t pos = 0;
    string groupid="";
    int count=0;

    vector<string> arr=split(str);

    if(arr.size()>1){
        return "You have given wrong format create_group and groupid";
    }
    groupid=arr[0];

    
    //group exists or not
    lock_guard<mutex> lock(groups_mutex);
    if(groups.find(groupid)==groups.end()){
        return "the group which you are going to leave doesn't exist";
    }
    //if exits
    //check that userid is admin or not
    if(groups[groupid]!=userid){
        return "you are not admin of that group to remove and groupid is "+groupid;
    }

    lock_guard<mutex> lock2(groupmembers_mutex);
    if (groupmembers[groupid].find(userid) == groupmembers[groupid].end()) {
        return "You are not a member of the group " + groupid;
    }

    // remove user from the group
    groupmembers[groupid].erase(userid);
    //check group admin
    if (groups[groupid] == userid) {
        if (groupmembers[groupid].empty()) {
            //last user need to delete the group
            groups.erase(groupid);
            pendingrequests.erase(groupid);
            groupmembers.erase(groupid);
            return "You left the group " + groupid + " The group has been deleted as it has no members.";
        } else {
            // assigning new admin which are in the griup
            string new_admin = *groupmembers[groupid].begin();
            groups[groupid] = new_admin;
            return "You left the group " + groupid + " " + new_admin + " is the new admin.";
        }
    }
    return "";
}

string listrequests(string str,string& userid){

    if(userid.size()==0) return "You did not loggedin";

    string delimiter = " ";

    size_t pos = 0;
    string groupid="";

    vector<string> arr=split(str);

    if(arr.size()>1){
        return "You have given wrong format create_group and groupid";
    }
    groupid=arr[0];
    //check the groupid exis are not
    lock_guard<mutex> lock(groups_mutex);
    if(groups.find(groupid)==groups.end()){
        return "the groupid is not created ";
    }
    if(groups[groupid]!=userid){
        return "you dont have an access to print the requests the group which is created by someone";
    }
    
    lock_guard<mutex> lock2(pendingrequests_mutex);
    if(pendingrequests[groupid].size()==0){
        return "there are no pending requests in the groupid "+groupid;
    }
    string str1="";
    for (const string &it : pendingrequests[groupid]) {
        str1+=(it)+"\n";
    }
    return "pending requests are\n"+str1;
    
    return "";
}

string acceptrequest(string str,string& userid){
    string delimiter = " ";

    size_t pos = 0;
    string groupid="";
    string otheruser="";
    int count=0;

   vector<string> arr=split(str);

    if(arr.size()>2){
        return "You have given wrong format create_group and groupid";
    }
    groupid=arr[0];
    otheruser=arr[1];

    lock_guard<mutex> lock(groups_mutex);
    if(groups.find(groupid)==groups.end()){
        return "the groupid is doesn't creates by you";
    }
    if(groups[groupid]!=userid){
        return "you are not the admin of that group";
    }
    {   lock_guard<mutex> lock2(pendingrequests_mutex);
        pendingrequests[groupid].erase(otheruser);
        lock_guard<mutex> lock3(groupmembers_mutex);
        groupmembers[groupid].insert(otheruser);
        return "user with "+otheruser+"was added successfully to the members groups";
    }
    return "";
}

string listallgroups(string& userid){
    lock_guard<mutex> lock(groups_mutex);
    if(groups.size()==0){
        return "groups are not created in the network";
    }
    string str="";
    for (const auto &it : groups) {
        str+=it.first+"\n";
    }
    return str;
}
string ipport(string str,string &userid){
    int pos=str.find(':');
    string ip=str.substr(0,pos);
    string port=(str.substr(pos+1));
    lock_guard<mutex> lock(ipandport_mutex);
    ipandport[userid]={ip,port};
    string res="client ip and port is received";
    return res;
}
string listfilesingroup(string groupid){
    if (groupfiles.find(groupid) == groupfiles.end()) {
        return "Group not found or no files shared.";
    }
    vector<string> files = groupfiles[groupid];
    if (files.empty()) {
        return "No sharable files in this group.";
    }
    string filelist = "Sharable files in group " + groupid + ":\n";
    for (const string &file :groupfiles[groupid] ) {
        cout<<file<<endl;
        filelist += file + "\n";
    }
    return filelist;
}

bool membership(const string &groupid, const string &userid) {
    
    if (groupmembers.find(groupid) == groupmembers.end()) {
        return false; 
    }

    set<string> members = groupmembers[groupid];
    return members.find(userid) != members.end(); 
}

string uploadfile(string str, const string &userid) {
    vector<string> parts = split(str);
    if (parts.size() < 4) {
        return "Invalid upload command format.";
    }

    string filepath = parts[0];
    cout<<"this is"<<filepath<<endl;
    string groupid = parts[1];
    cout<<"this is"<<groupid<<endl;
    string overallSHA = parts[2];
    cout<<"this is"<<overallSHA<<endl;
    string chunkSHAString = parts[3];
    cout<<"this is"<<chunkSHAString<<endl;

    vector<string> chunkSHAs;
    size_t pos = 0;
    string temp="";
    
    while ((pos = chunkSHAString.find(',')) != string::npos) {
        chunkSHAs.push_back(chunkSHAString.substr(0, pos));
        temp+=chunkSHAString.substr(0, pos)+"\n";
        chunkSHAString.erase(0, pos + 1);
    }
    if (!chunkSHAString.empty()) {
        chunkSHAs.push_back(chunkSHAString);
        // cout<<chunkSHAString<<endl;
        temp+=chunkSHAString+"\n";
    }

    string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    

    
    if (groupfileinfo.find(groupid) == groupfileinfo.end()) {
        groupfileinfo[groupid] = map<string, filepeerinfo>();
    }

   
    if (groupfileinfo[groupid].find(filename) != groupfileinfo[groupid].end()) {
        
        ipports newPeer = {ipandport.at(userid).first, ipandport.at(userid).second};
        groupfileinfo[groupid][filename].peers.push_back(newPeer);
    } 
    else {
        
        filepeerinfo newFileInfo;
        newFileInfo.filepath = filepath;
        newFileInfo.overallsha = overallSHA;
        newFileInfo.chunkshas = chunkSHAs;
        ipports newPeer = {ipandport.at(userid).first, ipandport.at(userid).second};
        newFileInfo.peers.push_back(newPeer);

        groupfileinfo[groupid][filename] = newFileInfo; 
        groupfiles[groupid].push_back(filename);
        cout<<filename<<endl;
    }

    // return "File uploaded successfully!";
    return temp;
}

// Handle download request function
string handleDownloadRequest(const string& str, const string& userid) {
    vector<string> parts = split(str);
    if (parts.size() != 3) {
        return "Invalid download command format.";
    }

    string groupid = parts[0];
    string filepath = parts[1];
    string destinationPath = parts[2];
    string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

    if (groupfileinfo.find(groupid) == groupfileinfo.end()) {
        return "Group not found.";
    }

    if (groupfileinfo[groupid].find(filename) == groupfileinfo[groupid].end()) {
        return "File not found in the specified group.";
    }

    if (ipandport.find(userid) == ipandport.end()) {
        return "User IP and port not found.";
    }

    // groupfileinfo[groupid][filename].peers.push_back({ipandport.at(userid).first, ipandport.at(userid).second});

    filepeerinfo& fileInfo = groupfileinfo[groupid][filename];

    string response = "start ";
    response += fileInfo.filepath + " ";
    response += fileInfo.overallsha + " ";

    for (const auto& sha : fileInfo.chunkshas) {
        response += sha + ",";
    }
    if (!fileInfo.chunkshas.empty()) {
        response.pop_back(); 
    }
    response += " ";

    for (const auto& peer : fileInfo.peers) {
        response += peer.ip+":"+peer.port + ",";
    }
    if (!fileInfo.peers.empty() && response.back() == ',') {
        response.pop_back(); 
    }

    return response;
}

void processrequest(int clientsocket) {
    
    
    string userid="";
    while (true) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesreceived = recv(clientsocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesreceived > 0) {
            buffer[bytesreceived] = '\0';  
            string str(buffer); 
            
            ssize_t pos = str.find(' ');
            string check = str.substr(0, pos);

            string res;
            
            if (check == "create_user") {
                res = createuser(str.substr(pos + 1));
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if (check == "login") {
                res = loginuser(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="create_group"){
                res = creategroup(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="join_group"){
                res = joingroup(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="leave_group"){
                res = leavegroup(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            
            }
            else if (check == "list_files") {
                res = listfilesingroup(str.substr(pos + 1));  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="list_requests"){
                res = listrequests(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="accept_request"){
                res = acceptrequest(str.substr(pos + 1),userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            
            }
            else if(check=="list_groups"){
                res = listallgroups(userid);  
                cout << res << endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="ipport"){
                res=ipport(str.substr(pos+1),userid);
                cout<<res<<endl;
                send(clientsocket, res.c_str(), res.size(), 0);
            }
            else if(check=="list_files"){
               string res=listfilesingroup(str.substr(pos+1));
                cout<<res<<endl;
                send(clientsocket, res.c_str(), res.size(), 0);
                
            }
            else if(check=="logout"){
                res=userid+" have logout successfully";
                userid="";
                cout<<res<<endl;
                send(clientsocket, res.c_str(), res.size(),0);
            }
            else if(check=="groupid"){
                bool flag=membership(str.substr(pos+1),userid);
                string res = flag ? "yes" : "no";
                send(clientsocket, res.c_str(), res.size(),0);
            }
            else if(check=="upload_file"){
               string res=uploadfile(str.substr(pos+1),userid);
               cout<<res<<endl;
               send(clientsocket, res.c_str(), res.size(),0);
            }
            else if(check =="download_file"){
                string res=handleDownloadRequest(str.substr(pos+1),userid);
                cout<<res<<endl;
                send(clientsocket, res.c_str(), res.size(),0);
            }
            else if (check == "exit") {  
                cout << "client requested to terminate the session." << endl;
                break;  
            }
            else {
                
                res = "Unknown command.";
                send(clientsocket, res.c_str(), res.size(), 0);
            }
        } 
        else if (bytesreceived == 0) {
            cout << "client disconnected." << endl;
            break;
        } 
        else {
            cerr << "Failed to receive client request." << endl;
            break;
        }
    }

    close(clientsocket);  
}


int main(int argc,char* argv[]){
    if(argc!=3){
        cerr<<"the arguments should be : ./tracker tracker_info.txt tracker id"<<endl;
        return 0;
    }

    string trackerfile=argv[1];
    int trackerid=stoi(argv[2]);

    cout<<trackerid<<endl;

    // by passing the filename and storing file information in a vector and activate according to id.
    vector<trackeripandport> trackers=trackerinformation(trackerfile);

    if(trackers.empty()||trackerid<=0||trackerid>trackers.size()){
        cerr<<"you have given the wrong id"<<endl;
        return -1;
    }

    trackeripandport currenttracker=trackers[trackerid-1];

    cout<<"current tracker ip address is :"<<currenttracker.ip<<"  port no. is :"<<currenttracker.port<<endl;

    // initialize the socket
    // AF_NET is used to connect over the newtork using tcp or udp.so the system can resolve the address properly.SOCK_STREM is connection oreinted.
    int fd=socket(AF_INET,SOCK_STREAM,0);

    if(fd<0){
        cerr<<"socket is not opened"<<endl;
        return -1;
    }
    // it contains abpout the family,port,where you are listening and ip address 
    struct sockaddr_in address;
    // initialize the environment for sockaddr structure
    int opt=1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        cerr << "set socket options error"<<endl;
        return -1;
    }

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=inet_addr(currenttracker.ip.c_str());
    address.sin_port=htons(currenttracker.port);
    
    //bind the socket to  port
    int bindvalue=bind(fd, (struct sockaddr *)&address, sizeof(address));
    if ( bindvalue< 0) {
        cerr << "bind failed"<<endl;
        return -1;
    }
    //listen the request from client(queues the requests)
    int listenval=listen(fd,10);

    if(listenval<0){
        cerr<<"listening error"<<endl;
        return -1;
    }
    //keep waiting for the new requests and proceed as per the requests
    while (true)
    {
        int clientsocket;
        struct sockaddr_in clientaddress;
        socklen_t clientaddresslength = sizeof(clientaddress);
        
        // accept incoming connections
        if ((clientsocket = accept(fd, (struct sockaddr *)&clientaddress, &clientaddresslength)) < 0) {
            cerr << "tracker not accepted" << endl;
            continue;
        }

        cout << "client connected from IP: " << inet_ntoa(clientaddress.sin_addr)<< " port: " << ntohs(clientaddress.sin_port) << endl;

        // create a thread for each client to handle the request
        thread clientthread(processrequest, clientsocket);
        // detach the thread so it runs independently
        clientthread.detach();
    }

    close(fd);
    return 0;
}