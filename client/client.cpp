#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/sha.h>
#define PORT 8080
#define myid
#define debug(x) cout << #x << " : " << x << endl;
#define CHUNKS 524288
#define chotaCHUNK 16384
// #define TRACKERIP "192.168.148.168"
#define TRACKERIP "127.0.0.1"

/*

I want a file - I am *REC*
I am being reached - I am *SEN*

Peer to Peer Code:
REC->SEN : 'a' - REC send hash of the required file to SEN
SEN->REC : 'b' - SEN sends posetive ack for the requested hash, and an intimation for upcoming chunkPresent array
REC->REC : 'c' - REC sends the chunk no to receive the chunk






*/

using namespace std;

string convertToString(char* a);
void* callCon();
void* startListening(void* ptr);
int connectTracker();
string listenTracker();
void sendTracker(string m);
string convertSHAToString(char* a);
string getFullSHA(string path);
vector<string> getChunkSHA(string path);
struct fileMeta generateFileMeta(string path,string gro);
void sendMetaToTracker(struct fileMeta fm);

struct fileMeta{
	string fname;
	string filesize;
	string group;
	string ip;
	int port;
	long long chunk;
	// string concatSHA;
	string totalSHA;
};

struct fileMetaMyCopy{
	string fname;
	string path;
	string filesize;
	string group;
	// string ip;
	// int port;
	long long chunk;
	vector<string> chunkSHA;
	string totalSHA;
	int seeding; // 0 - not seeding, 1 - seeding
	vector<int> chunkPresent;
    bool isDownloaded;
};

struct fileBigMeta{ // receiving data from Tracker, used for downloading
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

int myListeningPort,connectingPort,trackerPort;
int tracker_fd,trackerSoc; // conection descriptor to tracker
string myIP = "127.0.0.1"; // client ip address
// string fakeSHA = "00000000000000000000";
map<string,struct fileMetaMyCopy> fileData;
vector<vector<string>> fileResult;
string fileLoc;

void* handleDownload(void* x){
	int i=*((int *)x);
	string path=fileLoc;
	// cout<<"I was here";
	string m="1";
	char buffer[1024];
	// m=listenTracker();	// dummy ack
	// m=fileResult[i][1];	
	// sendTracker(m);	// sending hash of required file
	// m=listenTracker();	// boolean ack
	if(m=="1"){ 
		struct fileBigMeta fbm;
		sendTracker(m); // sending same boolean ack back to Tracker
		m=listenTracker(); // receive file name
		fbm.fname=m;
		sendTracker("1"); // sending ack
        m=listenTracker(); // receiving group
        fbm.group=m;
        sendTracker("1"); // sending ack
		m=listenTracker(); // receiving file size
		fbm.filesize=m;
		sendTracker("1"); // sending ack
		m=listenTracker();// receiving chunk count
		fbm.chunk=stoi(m);
		sendTracker("1"); // sending ack
		m=listenTracker(); // receiving totalSHA
		fbm.totalSHA=m;
		sendTracker("1"); // sending ack
		m=listenTracker(); // receiving size of ipPort vector
		sendTracker("1"); // sending ack
		int n=stoi(m);
		vector<vector<string>> v1;
		for(int i=0;i<n;i++){
			vector<string> v2;
			m=listenTracker(); // receiving ipaddr
			v2.push_back(m); 
			sendTracker("1"); // sending ack
			m=listenTracker(); // receiving port
			v2.push_back(m);
			sendTracker("1"); // sending ack
			v1.push_back(v2);
		}
		m=listenTracker();
		fbm.ipPort=v1;
		
		// received ip address and port of peers
		// connecting to the peer to get the file

		vector<vector<vector<string>>> vec(fbm.chunk); // peers list
        vector<string> vec2(fbm.chunk);
        struct fileMetaMyCopy mycopy;
        mycopy.fname=fbm.fname;
        mycopy.chunk=fbm.chunk;
        mycopy.filesize=fbm.filesize;
        mycopy.totalSHA=fbm.totalSHA;
        mycopy.chunkPresent=vector<int>(fbm.chunk,0);
        mycopy.chunkSHA=vec2;
        mycopy.seeding=0;
        mycopy.isDownloaded=true;
        mycopy.group=fbm.group;
		mycopy.path=path+"/"+fbm.fname;
        fileData[mycopy.totalSHA]=mycopy; // new entry for the file has been added in the File Database
        
        

        for(int i=0;i<fbm.ipPort.size();i++){ // connecting to all peers and getting availability
            int sock = 0, valread;
            struct sockaddr_in serv_addr;
            char buffer[1024] = { 0 };
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printf("\n Socket creation error \n");
                return 0;
            }
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(stoi(fbm.ipPort[i][1]));
            if (inet_pton(AF_INET, fbm.ipPort[i][0].c_str(), &serv_addr.sin_addr)
                <= 0) {
                printf("\nInvalid address/ Address not supported \n");
                return 0;
            }
            if ((tracker_fd
                = connect(sock, (struct sockaddr*)&serv_addr,
                sizeof(serv_addr)))
                < 0) {
                printf("\nConnection Failed \n");
                continue;
            }

            /*
            // map of open socket connections to all peers created to be used further
            string newtemp1=fbm.ipPort[i][0]+":"+fbm.ipPort[i][1];
            if(IPtoSocket.find(newtemp1)==IPtoSocket.end()){
                IPtoSocket[newtemp1]=sock;
            }
            */

            //connected to a single peer
            string message="a";
            ::send(sock, message.c_str(),message.length(), 0);
            valread = read(sock, buffer, 1024);
            message=fbm.totalSHA; // sending req Hash
            ::send(sock, message.c_str(),message.length(), 0);
            bzero(buffer,1024);
            valread = read(sock, buffer, 1024); // reading ack for reqHash ('b')
            message=convertToString(buffer);
            if(message=="b"){
                ::send(sock, message.c_str(),message.length(), 0);
                
                for(int j=0;j<fbm.chunk;j++){ // receiving chunk availability
                    bzero(buffer,1024);
                    valread = read(sock, buffer, 1024);
                    message=convertToString(buffer);
                    if(message=="1"){
                        vec[j].push_back(fbm.ipPort[i]);
                    }
                    message="1";
                    ::send(sock, message.c_str(),message.length(), 0);
                }

            }
            message="exit";
            ::send(sock, message.c_str(),message.length(), 0);
            close(sock);
        }
        // collected all peer availability info
        // creating socket connection 

        // sort(vec.begin(),vec.end(),comp()); // sorted in order of availability
        path=path+"/"+fbm.fname;
	    int check=open(path.c_str(),O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );

        int flag=0;

		for(long long i=0;i<fbm.chunk;i++){
            flag=0;
			for(int j=0;j<vec[i].size();j++){
				string message;
				int sock = 0, valread;
				struct sockaddr_in serv_addr;
				char buffer[1024] = { 0 };
				if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					printf("\n Socket creation error \n");
					 continue;
				}
				serv_addr.sin_family = AF_INET;
				string x,y;
				x=vec[i][j][1];
				y=vec[i][j][0];
				serv_addr.sin_port = htons(stoi(x));
				if (inet_pton(AF_INET, y.c_str(), &serv_addr.sin_addr)
					<= 0) {
					printf("\nInvalid address/ Address not supported \n");
					continue;
				}
				if ((tracker_fd
					= connect(sock, (struct sockaddr*)&serv_addr,
					sizeof(serv_addr)))
					< 0) {
					printf("\nConnection Failed to %s \n",x);
					continue;
				}
				else{
					// cout<<"connected to "<<x<<endl;
				}


				message="d";
				::send(sock, message.c_str(),message.length(), 0);
				valread = read(sock, buffer, 1024);
				message=fbm.totalSHA;
				::send(sock, message.c_str(),message.length(), 0); // sending file SHA
				bzero(buffer,1024);
				valread = read(sock, buffer, 1024);
				message=convertToString(buffer);
				if(message=="1"){ // peer has the file
					message=to_string(i);
					::send(sock, message.c_str(),message.length(), 0); // sending chunk no
					bzero(buffer,1024);
					valread = read(sock, buffer, 1024);
					message=convertToString(buffer);
					if(message=="1"){ // peer has the chunk
                        flag=1;
						::send(sock, message.c_str(),message.length(), 0);
						// start receiving the chunk
						char filebuffer[chotaCHUNK];
						long long u=0;
						string chunkData="";
						valread = read(sock, filebuffer,CHUNKS);
						message="1";
						SHA_CTX ctx;
						SHA1_Init(&ctx);
						char sha2[SHA_DIGEST_LENGTH];
						while(valread>1){
							SHA1_Update(&ctx,filebuffer,valread);
							pwrite(check,filebuffer,valread,(i*1ll*CHUNKS)+( u++*1ll*chotaCHUNK));
							// chunkData+=convertToString(filebuffer);
							::send(sock, message.c_str(),message.length(), 0);
							bzero(filebuffer,chotaCHUNK);
							valread = read(sock, filebuffer, CHUNKS);
						}
						SHA1_Final((unsigned char*)sha2,&ctx);
						for(int p=0;p<20;p++){
							if(sha2[p]=='\0'||sha2[p]=='\n'){
								sha2[p]='_';
							}
						}
						string strsha2=convertToString(sha2);
						// chunk received
						::send(sock, message.c_str(),message.length(), 0);
						//receive SHA of chunk
						bzero(buffer,1024);
						valread = read(sock, buffer, 1024); // received chunk SHA
						string sha1=convertToString(buffer);
                        fileData[mycopy.totalSHA].chunkSHA[i]=sha1;
						message="exit";
						::send(sock, message.c_str(),message.length(), 0);
						close(sock);
						if(sha1==strsha2){
							// cout<<"chunk "<<i<<" matched with "<<x<<endl;
							break;
						}
						else{
							// cout<<"chunk "<<i<<" did not match with "<<x<<endl;
						}

					}
                    else{
                        message="exit";
                        ::send(sock, message.c_str(),message.length(), 0);
                        close(sock);
                    }
				}
				else{
					message="exit";
					::send(sock, message.c_str(),message.length(), 0);
					close(sock);
				}
			}
            if(flag==0){
                cout<<"Error Downloading!"<<endl;
                break;
            }
            else{
                fileData[fbm.totalSHA].chunkPresent[i]=1;
            }
            if(i==0){
                string message;
                message="updateme";
                sendTracker(message);
                message=listenTracker();
                message=fbm.totalSHA;
                sendTracker(message); // sending totalSHA
                message=listenTracker();
                message=myIP;
                sendTracker(message); // sending IP
                message=listenTracker();
                message=to_string(myListeningPort);
                sendTracker(message); // sending port
                message=listenTracker();
            }
		}

	}
	
}

string getFullSHA(string path){
	int readCount;
	char buff[524288],sha[SHA_DIGEST_LENGTH];
	int fd = open(path.c_str(), O_RDONLY);
	if(fd<0){
		return "-1";
	}
	else{
		SHA_CTX ctx;
    	SHA1_Init(&ctx);
		    while ((readCount = read(fd, buff, 524288)) > 0){
				SHA1_Update(&ctx,buff,readCount);
			}
		SHA1_Final((unsigned char*)sha,&ctx);
		// string strSha = convertSHAToString(sha);
		string s="";
		for(int i=0;i<20;i++){
			if(sha[i]!='\0'&&sha[i]!='\n'){
				s+=sha[i];
			}
			else{
				s+=' ';
			}
		}
		// cout<<strSha;
		return s;
	}
}

vector<string> getChunkSHA(string path){
	vector<string> cs;
	int readCount;
	char buff[524288],sha[SHA_DIGEST_LENGTH];
	int fd = open(path.c_str(), O_RDONLY);
	if(fd<0){
		return cs;
	}
	else{
		SHA_CTX ctx;
		    while ((readCount = read(fd, buff, 524288)) > 0){
				SHA1_Init(&ctx);
				SHA1_Update(&ctx,buff,readCount);
				SHA1_Final((unsigned char*)sha,&ctx);
				// string strSha = convertSHAToString(sha);
				string s="";
				for(int i=0;i<20;i++){
					if(sha[i]!='\0'&&sha[i]!='\n'){
						s+=sha[i];
					}
					else{
						s+=' ';
					}
				}
				cs.push_back(s);
			}
	}
	return cs;
}


struct fileMeta generateFileMeta(string path,string gro){
	struct fileMeta fm;
	struct fileMetaMyCopy fm2;
	struct stat st;
	if(stat(path.c_str(), &st) == 0){
		int i;
		for(i=path.length()-1;i>=0;i--){
			if(path[i]=='/'){
				break;
			}
		}
		fm.fname=path.substr(i+1,path.length()-i);
		fm2.fname=path.substr(i+1,path.length()-i);
		fm2.path=path;
		fm.filesize=to_string(st.st_size);
		fm2.filesize=to_string(st.st_size);
		fm.group=gro;
		// fm2.group=gro;
		fm.ip=myIP;
		// fm2.ip=myIP;
		fm.port=myListeningPort;
		// fm2.port=myListeningPort;
		fm.chunk=1+ (st.st_size-1)/(1024*512);
		fm2.chunk=1+ (st.st_size-1)/(1024*512);
		fm.totalSHA=getFullSHA(path);
		fm2.totalSHA=getFullSHA(path);
		// fm.concatSHA=fakeSHA;
		// fm.concatSHA="";
		// for(int i=0;i<fm.chunk;i++){
		// 	fm.concatSHA+=fakeSHA;
		// }
		fm2.seeding=1;
		fm2.chunkSHA=getChunkSHA(path);
		vector<int> vec(fm2.chunk,1);
		fm2.chunkPresent = vec;
		fileData[fm2.totalSHA]=fm2;
	}
	return fm;
}

void sendTracker(string m){
	string message=m;
	::send(trackerSoc, message.c_str(),message.length(), 0);
}

void sendMetaToTracker(struct fileMeta fm){
	::send(trackerSoc, &fm,sizeof(fm), 0);
}

string listenTracker(){
	char buffer[1024] = { 0 };
	int valread = read(trackerSoc, buffer, 1024);
	if(convertToString(buffer)=="exit"){
		close(trackerSoc);
		return NULL;
	}
	return convertToString(buffer);
}

int connectTracker(){
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = { 0 };

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return 0;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(trackerPort);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, TRACKERIP, &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return 0;
	}
	if ((tracker_fd
		= connect(sock, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return 0;
	}
	trackerSoc=sock;
	return 1;
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

string convertSHAToString(char* a){
	string s="";
	for(int i=0;i<20;i++){
		s=s+a[i];
	}
	return s;
}

void* callCon(void *ptr){
    string message;
	// binding port number as the first 4 charecter of the message
	int sock = 0, valread, client_fd;
	struct sockaddr_in serv_addr;
	char buffer[1024] = { 0 };

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return NULL;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(connectingPort);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return NULL;
	}
	if ((client_fd
		= connect(sock, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return NULL;
	}
    // cout<<"connected to peer"<<endl;
    while(1){
		// cout<<"-->";
        cin>>message;
		if(message=="exit"){
			::send(sock, message.c_str(),message.length(), 0);
			close(sock);

			return NULL;
		}
        ::send(sock, message.c_str(),message.length(), 0);
		valread = read(sock, buffer, 1024);
		if(convertToString(buffer)=="exit"){
			close(sock);
			return NULL;
		}
		// cout<<"Message from("<<sock<<"): "<<convertToString(buffer)<<endl;
		bzero(buffer,512);
    }
	
	close(sock);
	return NULL;
}

void* continueConversation(void* ptr){ // *SEN*
	int server_fd, new_socket, valread;
	new_socket = *((int *)ptr);
	string message,reqHash;
	char buffer[512] = { 0 };
	while(1){
        bzero(buffer,512);
		valread = read(new_socket, buffer, 1024);
        while(valread<1){
			valread = read(new_socket, buffer, 1024);
		}
		message=convertToString(buffer);
		// cout<<message<<endl;
		bzero(buffer,512);
		if(message=="exit"){
			// ::send(new_socket, message.c_str(),message.length(), 0);
			close(new_socket);
			return NULL;
		}
		if(message==""){
			::send(new_socket, message.c_str(),message.length(), 0);
			close(new_socket);
			return NULL;
		}
		if(message=="a"){
			message="1";
			::send(new_socket, message.c_str(),message.length(), 0);
			valread = read(new_socket, buffer, 1024); // reading hash of the requiured file
			reqHash=convertToString(buffer);
			// int i,flag=0;
			// for(i=0;i<fileData.size();i++){
			// 	if(fileData[i].totalSHA==reqHash){
			// 		flag=1;
			// 		break;
			// 	}
			// }
			if(fileData.find(reqHash)!=fileData.end()){
				map<string,struct fileMetaMyCopy>::iterator itr;
				itr=fileData.find(reqHash);
				message="b";

				::send(new_socket, message.c_str(),message.length(), 0); // ack for req Hash and intimation for chunkPresent array
				valread = read(new_socket, buffer, 1024);
				for(int j=0;j<itr->second.chunk;j++){
					message=to_string(itr->second.chunkPresent[j]);
					::send(new_socket, message.c_str(),message.length(), 0); // sending the chunkPresent array
					valread = read(new_socket, buffer, 1024);
				}
			}
		}
		if(message=="c"){
			int fileCounter=0;
			message="1";
			::send(new_socket, message.c_str(),message.length(), 0);
			bzero(buffer,1024);
			valread = read(new_socket, buffer, 1024); // receiving chunk no
			message=convertToString(buffer);
			long long n=stoi(message); // chunk no
			map<string,struct fileMetaMyCopy>::iterator itr;
			itr=fileData.find(reqHash);
			char filebuffer[CHUNKS];
			bzero(buffer,CHUNKS);
			if(itr->second.chunkPresent[n]==1){ // I have got the chunk
				message="1";
				::send(new_socket, message.c_str(),message.length(), 0);
				valread = read(new_socket, buffer, 1024);
				int check=open(itr->second.path.c_str(),O_RDONLY);
				int readCount;
				long long u=0;
				readCount=pread(check,filebuffer,chotaCHUNK,(n*CHUNKS)+ u++*chotaCHUNK);
				fileCounter+=readCount;
				while(readCount>0&&u<=32){
					::send(new_socket, filebuffer,readCount,0);	
					bzero(filebuffer,CHUNKS);
					valread = read(new_socket, buffer, 1024);
					readCount=pread(check,filebuffer,chotaCHUNK,(n*1ll*CHUNKS)+ u++*1ll*chotaCHUNK);
					if(readCount>0){
						fileCounter+=readCount;
					}
				}
				close(check);
				
			}
			// cout<<"chunk :"<<n<<" size: "<<fileCounter<<endl;
		}
        else if(message=="d"){
            message="1";
            int readCount;
            
            ::send(new_socket, message.c_str(),message.length(), 0);
            bzero(buffer,1024);
            valread = read(new_socket, buffer, 1024); // receiving SHA of file
            reqHash=convertToString(buffer);
            if(fileData.find(reqHash)!=fileData.end()){ // file is present
                int check=open(fileData[reqHash].path.c_str(),O_RDONLY);
                message="1";
                ::send	(new_socket, message.c_str(),message.length(), 0);
                bzero(buffer,1024);
                valread = read(new_socket, buffer, 1024); // receiving chunk no
                int chunkNo=stoi(convertToString(buffer));
                if(fileData[reqHash].chunkPresent[chunkNo]==1){ // chunk is present
                    message="1";
                    ::send(new_socket, message.c_str(),message.length(), 0);
                    valread = read(new_socket, buffer, 1024);
                    // start sending the chunk
                    char filebuffer[chotaCHUNK];
                    long long u=0;
                    readCount=pread(check,filebuffer,chotaCHUNK,(chunkNo*CHUNKS)+ u++*chotaCHUNK);
                    while(readCount>0&&u<=32){
                        ::send(new_socket, filebuffer,readCount,0);
                        bzero(filebuffer,CHUNKS);
                        valread = read(new_socket, buffer, 1024);
                        readCount=pread(check,filebuffer,chotaCHUNK,(chunkNo*CHUNKS)+ u++*chotaCHUNK);
                    }
                    close(check);
                    // chunk sent
                    ::send(new_socket, message.c_str(),message.length(), 0);
                    valread = read(new_socket, buffer, 1024);
                    message=fileData[reqHash].chunkSHA[chunkNo];
                    ::send(new_socket, message.c_str(),message.length(), 0); // chunk SHA sent
                    bzero(buffer,1024);
                    valread = read(new_socket, buffer, 1024);
                    if(convertToString(buffer)=="exit"){
                        close(new_socket);
                        return NULL;
                    }
                }
                else{
                    message="0";
                    ::send(new_socket, message.c_str(),message.length(), 0);
                    bzero(buffer,1024);
                    valread = read(new_socket, buffer, 1024);
                    message=convertToString(buffer);
                    if(message=="exit"){
                        close(new_socket);
                    }
                }

            }
            else{
                message="0";
                ::send(new_socket, message.c_str(),message.length(), 0);
                bzero(buffer,1024);
                valread = read(new_socket, buffer, 1024);
                message=convertToString(buffer);
                if(message=="exit"){
                    close(new_socket);
                }
            }
        }
		else{
            message="0";
            ::send(new_socket, message.c_str(),message.length(), 0);
        }
	}
}

void* startListening(void* ptr){
    int server_fd, new_socket, valread;
	struct sockaddr_in address;
	struct sockaddr_storage serverStorage;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[512] = { 0 };
	string message;

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
	address.sin_port = htons(myListeningPort);

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
    else{
        // cout<<"Listening at port:"<<myListeningPort<<endl;
    }
	while(1){
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
	// getFullSHA("/home/ardhendu/Downloads/a.zip");
    myListeningPort=stoi(convertToString(argv[1]));
    trackerPort=stoi(convertToString(argv[2]));
	connectingPort=stoi(convertToString(argv[2]));

    pthread_t ptid,ptid2;
    if(pthread_create(&ptid, NULL, startListening, NULL)<0){
		cout<<"Error";
	};
	



	if(!connectTracker()){
		cout<<"Unable to Connect to Tracker";
		return 0;
	}
    string input,temp1,temp2,temp3;
    while(1){
		// cout<<"main thread sleeping.."<<endl;
        // sleep(1000);
		cin>>input;
		if(input=="create_user"){
			cin>>temp1>>temp2; // username password
			string m="a "+temp1+" "+temp2;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="login"){
			cin>>temp1>>temp2; // username password
			string m="b "+temp1+" "+temp2;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
            map<string,struct fileMetaMyCopy>::iterator mp;
            for(mp=fileData.begin();mp!=fileData.end();mp++){
                mp->second.chunkPresent.clear();
                mp->second.chunkPresent.resize(mp->second.chunk,1);
            }
		}
		else if(input=="create_group"){
			cin>>temp1; // groupid
			string m="c "+temp1;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="join_group"){
			cin>>temp1; // groupid
			string m="d "+temp1;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="leave_group"){
			cin>>temp1;  // groupid
			string m="e "+temp1;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="list_request"){
			cin>>temp1; // groupid
			string m="f "+temp1;
			sendTracker(m);
			m=listenTracker();
			cout<<m;
		}
		else if(input=="accept_request"){
			cin>>temp1>>temp2; // groupid username
			string m="g "+temp1+" "+temp2;
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="list_groups"){
			string m ="h";
			sendTracker(m);
			m=listenTracker();
			cout<<m;
		}
		else if(input=="list_files"){
			cin>>temp1; // groupid
			string m="i "+temp1;
			sendTracker(m);

			fileResult.clear();
			char buffer[1024];
			m=listenTracker();
			sendTracker("1");
			int n=stoi(m.substr(m.length()-1,1));
			for(int i=0;i<n;i++){
				temp1=listenTracker();
                cout<<i<<" "<<temp1<<endl;
				sendTracker("1");
				temp2=listenTracker();
				sendTracker("1");
				vector<string> v;
				v.push_back(temp1);
				v.push_back(temp2);
				fileResult.push_back(v);
				// fileResult[temp1]=temp2;
			}
			// for(int i=0;i<fileResult.size();i++){
			// 	printf("%-30s %-7s\n",fileResult[i][0].c_str(),fileResult[i][1].c_str());
			// }

			m=listenTracker();
			cout<<m<<endl;
		}
		else if(input=="upload_file"){
			cin>>temp1>>temp2; // filepath groupis
			struct fileMeta fm1=generateFileMeta(temp1,temp2);
			if(fm1.fname==""){
				cout<<"Error!"<<endl;
			}
			else{
				string m="j";
				char buffer[1024];
				sendTracker(m);
				// fm1.totalSHA=fakeSHA;
				// sendMetaToTracker(fm1);
				read(trackerSoc, buffer, 1024);
				sendTracker(fm1.fname);
				read(trackerSoc, buffer, 1024);
				sendTracker(fm1.filesize);
				read(trackerSoc, buffer, 1024);
				sendTracker(fm1.group);
				read(trackerSoc, buffer, 1024);
				sendTracker(fm1.ip);
				read(trackerSoc, buffer, 1024);
				sendTracker(to_string(fm1.port));
				read(trackerSoc, buffer, 1024);
				sendTracker(to_string(fm1.chunk));
				read(trackerSoc, buffer, 1024);
				sendTracker(fm1.totalSHA);
				m=listenTracker();
				cout<<m<<endl;
			}
			
		}
		else if(input=="download_file"){
			cin>>temp1>>temp2>>temp3; // groupid filename destination_path
			string m="k";
			int i,flag=0;
			for(i=0;i<fileResult.size();i++){
				if(fileResult[i][0]==temp2){
					flag=1;
					break;
				}
			}
			if(flag==1){
				sendTracker(m); // sending "k"
				char buffer[1024];
				m=listenTracker();	// dummy ack
				m=fileResult[i][1];	
				sendTracker(m);	// sending hash of required file
				m=listenTracker();	// boolean ack
				if(m=="1"){
					// handleDownload(i);
					pthread_t ptid,ptid2;
					fileLoc=temp3;
					if(pthread_create(&ptid, NULL, handleDownload, (void *)&i)<0){
						cout<<"Error";
					};
				}
				else{
					sendTracker(m); // sending same boolean ack back to Tracker
					cout<<"Error!\n";
				}

			}
			else{
				cout<<"Wrong Info!\n";
			}

		}
        else if(input=="stop_sharing"){
            cin>>temp1>>temp2;
            map<string,struct fileMetaMyCopy>::iterator mp;
            for(mp=fileData.begin();mp!=fileData.end();mp++){
                if(mp->second.fname==temp2){
                    fileData.erase(mp);
					cout<<"sharing stopped"<<endl;
					break;
                }
            }
        }
        else if(input=="show_downloads"){
            map<string,struct fileMetaMyCopy>::iterator mp;
            for(mp=fileData.begin();mp!=fileData.end();mp++){
                if(mp->second.isDownloaded==true){
                    int flag=0;
                    for(int i=0;i<mp->second.chunk;i++){
                        if(mp->second.chunkPresent[i]==0){
                            cout<<"[D] ["<<mp->second.group<<"] "<<mp->second.fname<<endl;
                            flag=1;
                            break;
                        }
                    }
                    if(flag==0){
                        cout<<"[C] ["<<mp->second.group<<"] "<<mp->second.fname<<endl;
                    }
                }
            }
        }
		else if(input=="logout"){
			string m="m";
			sendTracker(m);
			m=listenTracker();
			cout<<m<<endl;
            map<string,struct fileMetaMyCopy>::iterator mp;
            for(mp=fileData.begin();mp!=fileData.end();mp++){
                mp->second.chunkPresent.clear();
                mp->second.chunkPresent.resize(mp->second.chunk,0);
            }
		}
		else{
			cout<<"Invalid command!!"<<endl;
		}
    }
	while(1){
		sleep(1000);
	}

	return 0;
}