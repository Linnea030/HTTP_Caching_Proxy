#include <algorithm>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "proxy.h"
#include "log.h"
#include <pthread.h>
#include <thread>
#include "socket_info.h"
#include "handler.h"

#define MAX_LEN 65536
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

//static Log log;
//！！！！！！！！！！！！！记得改路径
std::ofstream file("/var/log/erss/proxy.log");

void Proxy::init_Proxy() {
    Csbuild cs;
    //Log log;
    //test!!!
    std::cout<<"in init_proxy\n";
    //build as server
    fd_accept = cs.init_server(port);
    //test!!!
    std::cout<<"after build server\n";
    if(fd_accept == -1) {
        //ID: ERROR MESSAGE
        pthread_mutex_lock(&lock1);
        std::string error = "Cannot initialize as a server";
        logError(error, file);
        pthread_mutex_unlock(&lock1);
    }
    int thread_id = 0;

    //need to add multi-thread
    while(1) {
        std::string ip;
        int fd_client = cs.accept_as_server(fd_accept, ip);
        if(fd_client == -1){
            //ID: ERROR MESSAGE
            pthread_mutex_lock(&lock1);
            std::string error = "Cannot accept remote client";
            logError(error, file, thread_id);
            pthread_mutex_unlock(&lock1);
            std::cout<<"accept client failed\n";
        	continue;
		}
        
        //test!!!
        std::cout<<"accept client in init_proxy\n";

        pthread_t new_thread;
        //get into a new thread

        pthread_mutex_lock(&lock2);
        int fd = -1;
        //save all arguments in one object
        SocketInfo * thread_info = new SocketInfo(fd_accept, fd_client, fd, thread_id, ip);
        thread_id = thread_id + 1;
        pthread_mutex_unlock(&lock2);

        //join a new thread
        pthread_create(&new_thread, NULL, process, thread_info);
        std::cout<<"\n";
    }
}

void * Proxy::process(void * thread1) {
        //Log log;
        SocketInfo *thread_info = (SocketInfo *)thread1;
        //test!!!
        std::cout<<"--------start a new process with id: "<<thread_info->id<<"------------\n";
        Csbuild cs;
        //receive request from remote client
        char request_info[MAX_LEN] = {0};
        int flag_size = recv(thread_info->fd_client, request_info, sizeof(request_info), 0);
        //判断recv的size =0 就结束这个handlereq， 小于0就400
        if(flag_size == 0) return NULL;
        else if(flag_size < 0) {
            //400 function
            std::cout<<"__________here method is 400__________\n";
            function400(thread_info->fd_client, thread_info->id);
            //ID: Responding "RESPONSE"
            pthread_mutex_lock(&lock1);
            logRes(thread_info->id, "HTTP/1.1 400 Bad Request", file);
            pthread_mutex_unlock(&lock1);
            std::cout<<"\n__________END 400__________\n";
        }

        //test!!!
        std::cout << "received request is!!!:\n" << request_info;

        std::string request_info_s(request_info);

        //get an object of request package
        PackRequest req_pack(request_info_s);

        //ID: "REQUEST*" from *IPFROM* @ *TIME
        pthread_mutex_lock(&lock1);
        logReq(thread_info->id , req_pack.request_line, thread_info->ip_client, file);
        std::string time = getTime();
        //file << thread_info->id << ": \"" << req_pack.request_line << "\" from "<< thread_info->ip_client << " @ " << time <<std::endl;
        pthread_mutex_unlock(&lock1);

        //test!!!
        std::cout << "\nparse request info is!!!:\n";
        req_pack.print_request();

        //connect to client as server
        const char * h = req_pack.hostname.c_str();
        const char * p = req_pack.port.c_str();
        //test!!!
        //std::cout<<"\n";
        //std::cout << h << ":" << p << std::endl;

        thread_info->fd_server = cs.init_client(h, p);
        //test!!!
        thread_info->info_print();
        if(thread_info->fd_server == -1){
            pthread_mutex_lock(&lock1);
            std::string error = "ERROR in connecting client";
            logError(error, file, thread_info->id);
            pthread_mutex_unlock(&lock1);
            std::cout<<"connect server failed in process\n";
        	return NULL;
		}

         //update_cs();
        if(req_pack.method == "GET"){
            std::cout<<"\n__________here method is GET__________\n";
            //get function
            Handler h;
            //initilize cache with size 50
            Cache cache(50);
            h.GETHandler(req_pack, thread_info->fd_client, thread_info->fd_server, cache, thread_info->id, file, lock1);
            std::cout<<"\n__________END GET__________\n";
        }
        else if(req_pack.method == "POST") {
            //test!!!
            std::cout<<"\n__________here method is POST__________\n";
            //post function
            post_function(thread_info->fd_client, 
                            thread_info->fd_server, thread_info->id, req_pack, thread_info);
            std::cout<<"after post function\n";
            std::cout<<"\n__________END POST__________\n";
        }
        else if(req_pack.method == "CONNECT"){
            //test!!!
            std::cout<<"\n__________here method is CONNECT__________\n";
            //connect function
            connect_function(thread_info->fd_client, 
                            thread_info->fd_server, thread_info->id, thread_info);
            std::cout<<"after connect function\n";
            //ID: Tunnel closed
            pthread_mutex_lock(&lock1);
            logTunnel(thread_info->id, file);
            pthread_mutex_unlock(&lock1);
            std::cout<<"\n__________END CONNECT__________\n";
        }
        else{
            //400 function
            std::cout<<"__________here method is 400__________\n";
            function400(thread_info->fd_client, thread_info->id);
            //ID: Responding "RESPONSE"
            pthread_mutex_lock(&lock1);
            logRes(thread_info->id, "HTTP/1.1 400 Bad Request", file);
            pthread_mutex_unlock(&lock1);
            std::cout<<"\n__________END 400__________\n";
        }
        std::cout<<"--------new process with id: "<<thread_info->id<<"is over------------\n";
        delete thread_info;
        return NULL;
}

void Proxy::connect_function(int fd_client, int fd_server, int id, SocketInfo * thread_info){
    //Log log;
    //cs.cs_print();
    //proxy_print();
    //test!!!
    std::cout<<"in connect function\n";
    //print log ID: Requesting "REQUEST" from SERVER
    std::string con_info = "HTTP/1.1 200 OK\r\n\r\n";
    int con_info_len = con_info.size();
    const char * con_info_c = con_info.c_str();
    //send 200OK to remote client
    int flag_size_s;
    flag_size_s = send(fd_client, con_info_c, con_info_len, MSG_NOSIGNAL);
    //std::cout<<"send 200OK to client with flag: "<<flag_size_s<<"\n";
    if(flag_size_s <= 0){
        std::string error = "send HTTP/1.1 200 OK failed";
        pthread_mutex_lock(&lock1);
        logError(error, file, id);
        pthread_mutex_unlock(&lock1);
    }

    //ID: Responding "RESPONSE"
    std::string response_line = "HTTP/1.1 200 OK";
    pthread_mutex_lock(&lock1);
    logRes(id ,response_line, file);
    pthread_mutex_unlock(&lock1);

    if (flag_size_s < 0) {
        //print Log(id, ": ERROR respond connect fails");
        //perror("error of send");
        std::string error = "send HTTP/1.1 200 OK failed";
        pthread_mutex_lock(&lock1);
        logError(error, file, id);
        pthread_mutex_unlock(&lock1);
        return;
    }
    
    //select and send message
    while(1) {
        fd_set readfds;
        int max_fd = std::max(fd_client, fd_server);
        FD_ZERO(&readfds);
        FD_SET(fd_server, &readfds);
        FD_SET(fd_client, &readfds);
        //std::cout<<"before select\n";
        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        char buf[MAX_LEN] = {0};
        int flag_r;
        int flag_s;
        if(FD_ISSET(fd_client, &readfds)) {
            //test!!!
            //std::cout<<"recv info from client with flag: ";
            //get info from remote client
            flag_r = recv(fd_client, &buf, sizeof(buf), MSG_NOSIGNAL);
            //std::cout<<buf;
            //test!!!
            //std::cout<<flag_r<<"\n";
            if(flag_r <= 0) {
                //perror("error of recv");
                std::string note = "recv nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                return;
            }

            //test!!!
            //std::cout<<"send info to server with flag: ";
            //send to remote server
            flag_s = send(fd_server, buf, flag_r, MSG_NOSIGNAL);
            //test!!!
            //std::cout<<flag_s<<"\n";
            if(flag_s <= 0) {
                //perror("error of send");
                std::string note = "send nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                return;
            }
        }
        if(FD_ISSET(fd_server, &readfds)) {
            //test!!!
            //std::cout<<"recv info from server with flag: ";
            //get info from remote server
            flag_r = recv(fd_server, &buf, sizeof(buf), MSG_NOSIGNAL);
            //test!!!
            //std::cout<<flag_r<<"\n";
            if(flag_r <= 0) {
                //perror("error of recv");
                std::string note = "recv nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                return;
            }

            //test!!!
            //std::cout<<"send info to client with flag: ";
            //send to remote client
            flag_s = send(fd_client, buf, flag_r, MSG_NOSIGNAL);
            //test!!!
            //std::cout<<flag_s<<"\n";
            if(flag_s <= 0) {
                //perror("error of send");
                std::string note = "send nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                return;
            }
        }
    }
}

void Proxy::post_function(int fd_client, int fd_server, int id, PackRequest & request_info, SocketInfo * thread_info) {
    //Log log;
    //test!!!
    std::cout<<"in post function\n";
    if (request_info.content_len_remain != -1) {
        int flag_size_s;
        const char * res_info_c = request_info.request.c_str();
        flag_size_s = send(fd_server, res_info_c, request_info.request.size(), MSG_NOSIGNAL);//0 in xy
        std::cout<<"send request to server with flag: "<<flag_size_s<<"\n";
        if(flag_size_s <= 0) {
                //perror("error of recv");
                std::string note = "send nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                //return;
        }

        char response[65536] = {0};
        int response_len = recv(fd_server,response,sizeof(response),MSG_WAITALL);
        //test!!!
        //std::cout<<"recv response from server with flag: "<<response_len<<"\n";
        //std::cout<<response<<"\n";
        
        if (response_len > 0) {
            PackResponse res(response);
            //ID: Received "RESPONSE" from SERVER
            pthread_mutex_lock(&lock1);
            logConServer(id, res.response_line, request_info.URI, 1, file);
            pthread_mutex_unlock(&lock1);

            int flag_s = send(fd_client, response, response_len, 0);
            std::cout<<"send request to client with flag: "<<flag_s<<"\n";
            if(flag_s <= 0) {
                //perror("error of recv");
                std::string note = "send nothing";
                pthread_mutex_lock(&lock1);
                logNote(note, file, id);
                pthread_mutex_unlock(&lock1);
                //return;
            }
            //ID: Responding "RESPONSE"
            pthread_mutex_lock(&lock1);
            logRes(id, res.response_line, file);
            pthread_mutex_unlock(&lock1);
            std::cout << "post successfully\n";
        }
        else{
            //ID: NOTE MESSAGE
            std::string note = " The connection between proxy and server is closed";
            std::cout<< note <<"\n";
            pthread_mutex_lock(&lock1);
            logNote(note, file, id);
            pthread_mutex_unlock(&lock1);
        }
    }
    else {
        //ID: ERROR MESSAGE
        std::string error = "cannot post";
        std::cout<< error << "\n";
        pthread_mutex_lock(&lock1);
        logError(error, file, id);
        pthread_mutex_unlock(&lock1);
    }

}

//handler 400
void Proxy::function400(int fd_client, int id) {
        const char * error = "HTTP/1.1 400 Bad Request";
        int flag;
        flag = send(fd_client, error, sizeof(error), MSG_NOSIGNAL);
        if(flag < 0){
            pthread_mutex_lock(&lock1);
            logError(error, file, id);
            pthread_mutex_unlock(&lock1);
        }
}
