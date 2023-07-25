#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <fstream>

#define PORT 8080
#define debug(x) cout << #x << " : " << x << endl;


/*
message Code:
a - create_user username password
b - login username password
c - create_group groupid
d - join_group groupid
e - leave_group groupid
f - list_request groupid
g - accept_request groupid username
h - list_groups 
i - list_files groupid
j - upload_file 
k - download_file 
l - stop_sharing
m - logout





*/

using namespace std;

int currPort = PORT;
map<string,string> userPassword; // username to password map
map<string,set<string>> group;	// groupid to list of member map
map<string,string> admin;	// groupid to admin username map
map<string,set<string>> request; // group id to list of user requests to join map
map<string,struct fileMeta> fileData; // filehash to fileMeta data map
map<string,int> login; // username to login status map ( 0 : logged out, 1: logged in)


int ipCounter=0;
int clientId,curr_sock;
string message; // received message
string returnMessage; // return message
// vector<string> token;

struct fileMeta{
	string fname;
	string filesize;
	string group;
	// string ip;
	// int port;
	vector<vector<string>> ipPort;
	long long chunk;
	// string concatSHA;
	string totalSHA;
};

void* startListening(void* ptr);
string convertToString(char* a);
void createUser();
vector<string> makeToken(string a);
void* continueConversation(void* ptr);

void exportData(){
	ofstream file1;
	string loc="./";
	// exporting username Password data
	file1.open(loc+"userdata.txt");
	file1<<userPassword.size()<<endl;
	map<string,string>::iterator u;
	for(u=userPassword.begin();u!=userPassword.end();u++){
		file1<<u->first<<" "<<u->second<<endl;
	}
	file1.close();

	//exporting admin data
	file1.open(loc+"admindata.txt");
	file1<<admin.size()<<endl;
	map<string,string>::iterator ad;
	for(ad=admin.begin();ad!=admin.end();ad++){
		file1<<ad->first<<" "<<ad->second<<endl;
	}
	file1.close();

	//exporting group data
	file1.open(loc+"groupdata.txt");
	file1<<group.size()<<endl;
	map<string,set<string>>::iterator g;
	for(g=group.begin();g!=group.end();g++){
		file1<<g->first<<endl;
		file1<<g->second.size()<<endl;
		for(string se:g->second){
			file1<<se<<endl;
		}
	}
	file1.close();

	//exporting request data
	file1.open(loc+"requestdata.txt");
	file1<<request.size()<<endl;
	// map<string,set<string>>::iterator g;
	for(g=request.begin();g!=request.end();g++){
		file1<<g->first<<endl;
		file1<<g->second.size()<<endl;
		for(string se:g->second){
			file1<<se<<endl;
		}
	}
	file1.close();

	//exporting fileData
	// file1.open(loc+"fileData.txt");
	// file1<<fileData.size()<<endl;
	// map<string,struct fileMeta>::iterator fi;
	// for(fi=fileData.begin();fi!=fileData.end();fi++){
	// 	file1<<fi->second.fname<<endl;
	// 	file1<<fi->second.filesize<<endl;
	// 	file1<<fi->second.group<<endl;
	// 	file1<<fi->second.ipPort.size()<<endl;
	// 	for(int i=0;i<fi->second.ipPort.size();i++){
	// 		file1<<fi->second.ipPort[i][0]<<endl;
	// 		file1<<fi->second.ipPort[i][1]<<endl;
	// 	}
    //     file1<<fi->second.chunk<<endl;
	// 	file1<<fi->second.totalSHA<<endl;
	// }
	// file1.close();
}

void importData(){
	string loc="./";
	ifstream file1;
	string str,temp1,temp2;
	int x,y;

	// importing username Password data
	file1.open(loc+"userdata.txt");
	str="";
	file1>>str;
	if(str!=""){
		x=stoi(str);
		for(int i=0;i<x;i++){
			file1>>temp1;
			file1>>temp2;
			userPassword[temp1]=temp2;
		}
	}
	file1.close();

	//importing admin data
	file1.open(loc+"admindata.txt");
	str="";
	file1>>str;
	if(str!=""){
		x=stoi(str);
		for(int i=0;i<x;i++){
			file1>>temp1;
			file1>>temp2;
			admin[temp1]=temp2;
		}
	}
	file1.close();

	//importing group data
	file1.open(loc+"groupdata.txt");
	str="";
	file1>>str;
	if(str!=""){
		x=stoi(str);
		for(int i=0;i<x;i++){
			file1>>temp1;
			file1>>temp2;
			y=stoi(temp2);
			set<string> gr;
			for(int j=0;j<y;j++){
				file1>>temp2;
				gr.insert(temp2);
			}
			group[temp1]=gr;
		}
	}
	file1.close();

	//importing request data
	file1.open(loc+"requestdata.txt");
	str="";
	file1>>str;
	if(str!=""){
		x=stoi(str);
		for(int i=0;i<x;i++){
			file1>>temp1;
			file1>>temp2;
			y=stoi(temp2);
			set<string> gr;
			for(int j=0;j<y;j++){
				file1>>temp2;
				gr.insert(temp2);
			}
			request[temp1]=gr;
		}
	}
	file1.close();

	//importing fileData
	// file1.open(loc+"fileData.txt");
	// str="";
	// file1>>str;
	// if(str!=""){
	// 	x=stoi(str);
	// 	for(int i=0;i<x;i++){
	// 		struct fileMeta m;
	// 		int z;
	// 		file1>>temp1;
	// 		m.fname=temp1;
	// 		file1>>temp1;
	// 		m.filesize=temp1;
	// 		file1>>temp1;
	// 		m.group=temp1;
	// 		file1>>temp1;
	// 		z=stoi(temp1);
	// 		for(int j=0;j<z;j++){
	// 			vector<string> vecS;
	// 			file1>>temp1;
	// 			vecS.push_back(temp1);
	// 			file1>>temp1;
	// 			vecS.push_back(temp1);
	// 			m.ipPort.push_back(vecS);
	// 		}
    //         file1>>temp1;
    //         m.chunk=stoi(temp1);
	// 		file1>>temp1;
	// 		m.totalSHA=temp1;
	// 		fileData[m.totalSHA]=m;
	// 	}
	// }
}

vector<string> makeToken(string a){
	vector<string> token;
    token.clear();
    int start=0,end=0;
    for(int i=0;i<a.length();i++){
        if(a[i]==' '){
            end=i;
            if(a.substr(start,end-start)!=""){
                token.push_back(a.substr(start,end-start));
            }
            start=i+1;
        }
        else{
            continue;
        }
    }
    if(a.substr(start,a.length()-start)!=""){
        token.push_back(a.substr(start,a.length()-start));
    }
    return token;
}

string convertToString(char* a)
{
    int i,counter=0;
    while(a[counter]!='\0'){
        counter++;
    }
    string s = "";
    for (i = 0; i < counter; i++) {
        s = s + a[i];
    }
    return s;
}

void* continueConversation(void* ptr){
	string currUser=""; // current user status & login tracker
	int new_socket,valread;
	new_socket = *((int *)ptr);
	string message;
	char buffer[1024] = { 0 };
	while(1){
		message="";
		valread = read(new_socket, buffer, 1024);
		while(valread<1){
			valread = read(new_socket, buffer, 1024);
		}
		if(convertToString(buffer)=="exit"){
			close(new_socket);
			return NULL;
		}
		vector<string> token = makeToken(convertToString(buffer));
		if(token.size()==0){

		}
		else{
			if(token[0]=="a"){ // create user
				if(userPassword.find(token[1])==userPassword.end()){
					userPassword[token[1]]=token[2];
					message= "User Created!";
				}
				else{
					message= "Duplicated User!";
				}
				exportData();
			}
			else if(token[0]=="b"){ // login
				if(currUser!=""){
					message="Already logged in!";
				}
				else if(userPassword.find(token[1])!=userPassword.end()){
					if(userPassword[token[1]]==token[2]){
						currUser=token[1];
						login[token[1]]=1;
						message="Login Successful!";
					}
					else{
						message="Wrong Password!";
					}
				}
				else{
					message="Wrong UserName!";
				}
			}
			else if(token[0]=="c"){ // create group
				if(currUser==""){
					message="Please Login first!";
				}
				else if(group.find(token[1])==group.end()){
					group[token[1]].insert(currUser);
					admin[token[1]]=currUser;
					message="Group created successfully";
				}
				else{
					message="Group already present!";
				}
				exportData();
			}
			else if(token[0]=="d"){ // join group
				if(currUser==""){
					message="Please Login First!";
				}
				else if(group.find(token[1])==group.end()){
					message="No group is present with such name!";
				}
				else if(group[token[1]].find(currUser)!=group[token[1]].end()){
					message="Already Part of the group";
				}
				else{
					request[token[1]].insert(currUser);
					message="Requested to Join group";
				}
				exportData();
			}
			else if(token[0]=="e"){ // leave group
				if(currUser==""){
					message="Please login First!";
				}
				else if(group[token[1]].find(currUser)==group[token[1]].end()){
					message="You are not part of this group!";
				}
				else{
					group[token[1]].erase(currUser);
					if(group[token[1]].size()==0){
						group.erase(token[1]);
						admin.erase(token[1]);
					}
					else{
						for(string s: group[token[1]]){
							admin[token[1]]=s;
							break;
						}
					}
					message="Removed from group!";
				}
				exportData();
			}
			else if(token[0]=="f"){ // list request
				if(currUser==""){
					message="Please login First!";
				}
				else if(request.find(token[1])==request.end()){
					message="No group is present with such name!";
				}
				else{
					string m="";
					for(string s:request[token[1]]){
						m+=s+"\n";
					}
					message=m;
				}
			}
			else if(token[0]=="g"){ // accept request
				if(currUser==""){
					message="Please login First!";
				}
				else if(group.find(token[1])==group.end()){
					message="No group is present with such name!";
				}
				else{
					if(admin[token[1]]!=currUser){
						message="You are not the admin to accept requests!";
					}
					else if(request.find(token[1])==request.end()||request[token[1]].find(token[2])==request[token[1]].end()){
						message="No request for the User: "+token[2];
					}
					else{
						request[token[1]].erase(token[2]);
						group[token[1]].insert(token[2]);
						message=token[2]+" has been added to group";
					}
				}
				exportData();
			}
			else if(token[0]=="h"){ // list groups
				if(currUser==""){
					message="Please login First!";
				}
				else{
					map<string,set<string>>::iterator mp;
					string m="";
					for(mp=group.begin();mp!=group.end();mp++){
						m+=mp->first+"\n";
					}
					message=m;
				}
			}
			else if(token[0]=="i"){ // list files
				if(currUser==""){
					message="Please login First";
				}
				else{
					vector<vector<string>> m;
					map<string,struct fileMeta>::iterator a;
					for(a=fileData.begin();a!=fileData.end();a++){
						if(a->second.group==token[1]){
							// m[a->second.fname]=a->second.totalSHA;
							// m.insert(make_pair(a->second.fname,a->first));
							vector<string> v;
							v.push_back(a->second.fname);
							v.push_back(a->first);
							m.push_back(v);
						}
					}
					message=to_string(m.size());
					::send(new_socket, message.c_str(),message.length(), 0);
					valread = read(new_socket, buffer, 1024);
					for(int i=0;i<m.size();i++){
						message=m[i][0];
						::send(new_socket, message.c_str(),message.length(), 0);
						valread = read(new_socket, buffer, 1024);
						message=m[i][1];
						::send(new_socket, message.c_str(),message.length(), 0);
						valread = read(new_socket, buffer, 1024);
					}
					message="List of Files sent by Tracker!";
				}
			}
			else if(token[0]=="j"){ // upload file
				if(currUser==""){
					message="Please login First!";
				}
				else{
					struct fileMeta fm;
					// read(new_socket, ( void*)&fm, sizeof(fm));
					// fileData[fm.totalSHA]=fm;
					message="1";
					::send(new_socket, message.c_str(),message.length(), 0);
					valread = read(new_socket, buffer, 1024); // reading file name
					::send(new_socket, message.c_str(),message.length(), 0);
					fm.fname = convertToString(buffer);
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading file size
					::send(new_socket, message.c_str(),message.length(), 0);
					fm.filesize = convertToString(buffer);
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading group
					::send(new_socket, message.c_str(),message.length(), 0);
					fm.group = convertToString(buffer);
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading IP
					::send(new_socket, message.c_str(),message.length(), 0);
					vector<string> v;
					v.push_back(convertToString(buffer));
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading port
					::send(new_socket, message.c_str(),message.length(), 0);
					v.push_back(convertToString(buffer));
					fm.ipPort.push_back(v);
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading chunk size
					::send(new_socket, message.c_str(),message.length(), 0);
					fm.chunk = stoi(convertToString(buffer));
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // reading TotalSHA
					// ::send(new_socket, message.c_str(),message.length(), 0);
					fm.totalSHA = convertToString(buffer);
                    if(fileData.find(fm.totalSHA)!=fileData.end()){
                        fileData[fm.totalSHA].ipPort.push_back(v);
                    }
                    else{
                        fileData[fm.totalSHA]=fm;
                    }
					
					message="File Added";
				}
				exportData();
			}
			else if(token[0]=="k"){ // download file
				if(currUser==""){
					message="Please login First!";
				}
				else{
					message="1";
					::send(new_socket, message.c_str(),message.length(), 0); // dummy ack
					bzero(buffer,1024);
					valread = read(new_socket, buffer, 1024); // received hash of the required file
					string m=convertToString(buffer);
					if(fileData.find(m)!=fileData.end()){
						message="1";
						::send(new_socket, message.c_str(),message.length(), 0); // boolean ack
						valread = read(new_socket, buffer, 1024);
						message=fileData[m].fname;
						::send(new_socket, message.c_str(),message.length(), 0); // sending file name
						valread = read(new_socket, buffer, 1024);
						message=fileData[m].group;
						::send(new_socket, message.c_str(),message.length(), 0); // sending group
						valread = read(new_socket, buffer, 1024);
						message=fileData[m].filesize;
						::send(new_socket, message.c_str(),message.length(), 0); // sending file size
						valread = read(new_socket, buffer, 1024);
						message=to_string(fileData[m].chunk);
						::send(new_socket, message.c_str(),message.length(), 0); // sending chunk count
						valread = read(new_socket, buffer, 1024);
						message=fileData[m].totalSHA;
						::send(new_socket, message.c_str(),message.length(), 0); // sending totalSHA
						valread = read(new_socket, buffer, 1024);
						message=to_string(fileData[m].ipPort.size());
						::send(new_socket, message.c_str(),message.length(), 0); // sending ipPort size
						valread = read(new_socket, buffer, 1024);
						for(int i=0;i<fileData[m].ipPort.size();i++){
							message=fileData[m].ipPort[i][0];
							::send(new_socket, message.c_str(),message.length(), 0); // sending ipaddress
							valread = read(new_socket, buffer, 1024);
							message=fileData[m].ipPort[i][1];
							::send(new_socket, message.c_str(),message.length(), 0); // sending ipaddress
							valread = read(new_socket, buffer, 1024);
						}
					}
				}

			}
			else if(token[0]=="updateme"){
				string message="1";
				::send(new_socket, message.c_str(),message.length(), 0);
				bzero(buffer,1024);
				valread = read(new_socket, buffer, 1024); // receiving the totalSHA
				string reqHash=convertToString(buffer);
				message="1";
				::send(new_socket, message.c_str(),message.length(), 0);
				bzero(buffer,1024);
				valread = read(new_socket, buffer, 1024); // receiving IP
				vector<string> v;
				v.push_back(convertToString(buffer));
				message="1";
				::send(new_socket, message.c_str(),message.length(), 0);
				bzero(buffer,1024);
				valread = read(new_socket, buffer, 1024); // receiving port
				v.push_back(convertToString(buffer));
				message="1";
				::send(new_socket, message.c_str(),message.length(), 0);

				if(fileData.find(reqHash)!=fileData.end()){
					fileData[reqHash].ipPort.push_back(v);
				}
				exportData();
			}
			else if(token[0]=="m"){ // logout
				if(currUser==""){
					message="Not Logged In!";
				}
				else{
					currUser="";
					message="Logout successful";
				}
			}
		}
		
		if(message=="exit"){
			::send(new_socket, message.c_str(),message.length(), 0);
			close(new_socket);
			return NULL;
		}
		::send(new_socket, message.c_str(),message.length(), 0);
		bzero(buffer,1024);
	}
}

void* startAccepting(void* ptr){
	// pthread_detach(pthread_self());
    int server_fd, new_socket, valread;
	struct sockaddr_in address;
	struct sockaddr_storage serverStorage;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[512] = { 0 };

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 8080
	if (setsockopt(server_fd, SOL_SOCKET,
				SO_REUSEADDR
                //  | SO_REUSEPORT
                 , &opt,
				sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	// address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(currPort);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return NULL;
	}

	// Forcefully attaching socket to the port 8080
	if (::bind(server_fd, (struct sockaddr*)&address,
			sizeof(address))
		< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	while(1){
		// printf("Waiting for client to approach server at %d...\n",currPort);
		if ((new_socket = accept(server_fd, (struct sockaddr*)&serverStorage,(socklen_t*)&addrlen)) < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}
		// cout<<"Accepted Client..."<<endl;

		pthread_t conv1;
		if(pthread_create(&conv1, NULL, continueConversation, (void *)&new_socket)<0){
			cout<<"Error";
		};
	}
	shutdown(server_fd, SHUT_RDWR);
	return NULL;
}

int main(int argc, char **argv)
{	
	currPort=stoi(convertToString(argv[1]));
	importData();
	pthread_t ptid;
	// Creating a new thread
	if(pthread_create(&ptid, NULL, startAccepting, NULL)<0){
		cout<<"Error";
	};
	string input;
	while(1){
		cin>>input;
		if(input=="quit"){
			return 0;
		}
		// cout<<input<<endl;
	}
	return 0;
}
