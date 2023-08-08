/*
 * *
 * * Copyright (C) 2023 Advanced Micro Devices, Inc.
 * *
 * */

#include "base.hpp"
#include "computework.hpp"
#include "graphicwork.hpp"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <unordered_map>

#include <regex>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <thread>

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<errno.h>

#define RUN_TIMES 5
class Request {

    VkQueueGlobalPriorityEXT str2priority(const std::string& str) {
        static const std::unordered_map<std::string, VkQueueGlobalPriorityEXT> map = {
            {"low", VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT},
            {"medium", VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT},
            {"high", VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT},
            {"realtime", VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT}
        };

        auto it = map.find(str);
        if (it == map.end()) {
            LOG("%s is not a valid priority. Use low, medium or high\n", str.c_str());
            return VK_QUEUE_GLOBAL_PRIORITY_MAX_ENUM_EXT;
        } else {
            return it->second;
        }
    }


public:
    enum class Type {
        Graphics,
        Compute
    };

    unsigned m_commandCount;
    VkQueueGlobalPriorityEXT m_priority;
    std::chrono::microseconds m_delay = std::chrono::microseconds::zero();
    Type m_type;
    Workload* m_workload = nullptr;

    Request(char* str)
    {
        const std::regex regex_graphic("gfx=draws:([0-9]+),priority:(low|medium|high),delay:([0-9]+)");
        const std::regex regex_compute("compute=dispatch:([0-9]+),priority:(low|medium|high|realtime),delay:([0-9]+)");

        std::cmatch m;

        if (std::regex_match(str, m, regex_graphic)) {
            m_type = Type::Graphics;
            m_commandCount = stoi(m[1]);
            m_priority = str2priority(m[2]);
            m_delay = std::chrono::microseconds(std::stoi(m[3]));
        } else if (std::regex_match(str, m, regex_compute)) {
            m_type = Type::Compute;
            m_commandCount = stoi(m[1]);
            m_priority = str2priority(m[2]);
            m_delay = std::chrono::microseconds(std::stoi(m[3]));
        } else {
            LOG("Could not parse \'%s\'", str);
            exit(-1);
        }
        LOG("Request : commands %d, priority %d, delay %lld\n", m_commandCount, m_priority, m_delay.count());
    }

    ~Request() {
        if (m_workload != nullptr) {
            delete(m_workload);
        }
    }

    VkQueueFlagBits vkQueueFlag() {
        switch(m_type) {
            case Type::Graphics: return VK_QUEUE_GRAPHICS_BIT;
            case Type::Compute : return VK_QUEUE_COMPUTE_BIT;
        }
	return VK_QUEUE_FLAG_BITS_MAX_ENUM;
    }

    Workload* createWorkload(Base& base, QueueInfo queue, unsigned commandCount) {
        switch(m_type) {
            case Type::Graphics: return new GraphicsWork(base, queue, commandCount);
            case Type::Compute : return new ComputeWork(base, queue, commandCount);
        }
	return nullptr;
    }

    void init(Base& base) {
        auto queue = base.GetQueueInfo(vkQueueFlag(), m_priority);
        m_workload = createWorkload(base, queue, m_commandCount);
    }

    void queryTimestamp(uint64_t time_stamp[], int count) {
        m_workload->queryTimestamp(time_stamp, count);
    }

    void waitIdle() {
        m_workload->waitIdle();
    }

    VkFence submit(Base& base) {
        return m_workload->submit();
    }

    static std::vector<Request> rearrangeDelays(std::vector<Request> requests) {
        // Sort the vector in ascending order of delays
        std::sort(requests.begin(), requests.end(), [](Request a, Request b) { return a.m_delay < b.m_delay; });

        // The delays do not compound, so subtract the previous one from the current
        auto previous = std::chrono::microseconds::zero();
        std::cout << "Delays : " << std::endl;
        for (auto& request : requests) {
            std::cout << request.m_delay.count();
            request.m_delay -= previous;
            std::cout << " -> " << request.m_delay.count() << std::endl;
            previous = request.m_delay;
        }

        return requests;
    }
};

#define SOCKET_PATH "/tmp/mysocket"
#define IPC_KEY 0x12345678
#define TYPE_S 1
#define TYPE_C 2

struct msgbuff{
  long mtype;
  char mtext[512];
  uint64_t time_stamp[RUN_TIMES * 2];
};

struct timespec ts;
struct timespec ts1;
struct timespec ts2;
int clifd = -1;

int server()
{
    int servfd;
    int ret;

    servfd = socket(AF_LOCAL,SOCK_STREAM,0);
    if(-1 == servfd)
    {
        perror("Can not create socket");
        return -1;
    }

    struct sockaddr_un servaddr;
    bzero(&servaddr, sizeof(servaddr));
    strcpy(servaddr.sun_path+1, SOCKET_PATH);
    servaddr.sun_family = AF_LOCAL;
    socklen_t addrlen = 1 + strlen(SOCKET_PATH) + sizeof(servaddr.sun_family);

    ret = bind(servfd, (struct sockaddr *)&servaddr, addrlen);
    if(-1 == ret)
    {
        perror("bind failed");
        return -1;
    }

    ret = listen(servfd, 100);
    if(-1 == ret)
    {
        perror("listen failed");
        return -1;
    }

    pthread_t tid;
    struct sockaddr_un cliaddr;

    printf("Wait for client connect \n");
    memset(&cliaddr,0,sizeof(cliaddr));
    clifd = accept(servfd,(struct sockaddr *)&cliaddr,&addrlen);
    if(clifd == -1)
    {
        printf("accept connect failed\n");
        return -1;
    }
    printf("Accept connect success\n");

    char getData[100];
    bzero(&getData,sizeof(getData));
    ret = read(clifd,&getData,sizeof(getData));
    if(ret > 0)
    {
        printf("Receive message: %s", getData);
    }

    return 0;
}

int client()
{
    int ret;

    clifd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(-1 == clifd)
    {
        perror("socket create failed\n");
        return -1;
    }

    struct sockaddr_un cileddr;
    bzero(&cileddr, sizeof(cileddr));
    strcpy(cileddr.sun_path + 1, SOCKET_PATH);
    cileddr.sun_family = AF_LOCAL;
    socklen_t addrlen = sizeof(cileddr.sun_family) + strlen(SOCKET_PATH) + 1;

    ret = connect(clifd, (struct sockaddr *)&cileddr, addrlen);
    if(ret == -1) {
        perror("Connect fail\n");
        return -1;
    }
    const char *s = std::string("hello\n").c_str();
    send(clifd,s,strlen(s),0);
    printf("Client: send hello to server\n");
    return 0;
}

uint64_t toTime(timespec ts){
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

int gfx(std::vector<Request> &requests, bool isServer) {
    std::vector<VkQueueGlobalPriorityEXT> graphic_priorities;
    std::vector<VkQueueGlobalPriorityEXT> compute_priorities;
    Request& request = requests.back();

    switch(request.m_type) {
        case Request::Type::Graphics: graphic_priorities.push_back(request.m_priority); break;
        case Request::Type::Compute : compute_priorities.push_back(request.m_priority); break;
    }

    Base base(graphic_priorities, compute_priorities);

    printf("Waiting %lld us ... \n", request.m_delay.count());
    fflush(stdout);
    std::this_thread::sleep_for(request.m_delay);

    int msgid = -1;

    int i, j;

    struct msgbuff buf;
    memset(&buf, 0x00, sizeof(struct msgbuff));


    if (isServer)
    {
        msgid = server();
        if(msgid < 0)
        {
            perror("Server: msgget error");
            exit(-1);
        }
    }
    else {
        msgid = client();
        if(msgid < 0)
        {
            perror("Client: error , please start server first\n");
            //exit(-1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (isServer)
    {
        printf("Server: start submission time: <%ld.%ld>\n",ts.tv_sec,ts.tv_nsec);
    }
    else
    {
        printf("Client: start submission time: <%ld.%ld>\n",ts.tv_sec,ts.tv_nsec);
    }


    uint64_t time_stamp[RUN_TIMES * 2];


    std::vector<VkFence> fences;

    for (i = 0; i < RUN_TIMES; i++) {

        fences.clear();
        clock_gettime(CLOCK_MONOTONIC, &ts1);

        printf("pid %d running: %d \n", getpid(), i);
        for (j = 0; j < 2; j++) {
            request.init(base);
            fflush(stdout);
            fences.push_back(request.submit(base));
        }

        VK_CHECK_RESULT(vkWaitForFences(base.GetDevice(), fences.size(), fences.data(), VK_TRUE, UINT64_MAX));
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        time_stamp[i * 2] = toTime(ts1);
        time_stamp[i * 2 + 1] = toTime(ts2);

    }


    if (isServer)
    {
        int ret;
        memset(&buf, 0x00, sizeof(struct msgbuff));
        ret = read(clifd, &buf, sizeof(buf));
        if(ret > 0)
        {
            printf("Receive message: client %s\n", buf.mtext);
        }

        for (i = 0; i < RUN_TIMES; i++) {
            if (request.m_priority >= VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT
                    && buf.time_stamp[i * 2] < time_stamp[i * 2]
                    && buf.time_stamp[i * 2 + 1] > time_stamp[i * 2 + 1]) {
                printf("success on(%d) high:%ld low: %ld\n",i,  time_stamp[i * 2 + 1] - time_stamp[i * 2],
                    (buf.time_stamp[i * 2 + 1] - buf.time_stamp[i * 2]));
                break;
            }
        }
        if (i == RUN_TIMES) {
            printf("run again to trigger mcbp.\n");
        }
    }
    else
    {
        int ret;
        memset(&buf, 0x00, sizeof(struct msgbuff));
        memcpy(buf.time_stamp, time_stamp, sizeof(time_stamp));
        strcpy(buf.mtext, "gpu timestamp");
        send(clifd, &buf, sizeof(buf), 0);
        for (i = 0; i < RUN_TIMES; i++) {
            printf("Client: gpu timestamp %lu %lu total:%ld\n", buf.time_stamp[i * 2],
                buf.time_stamp[i * 2 + 1], (time_stamp[i * 2 + 1] - time_stamp[i * 2]));
        }
    }

    request.waitIdle();

    return 0;
}

int main(int argc, char *argv[]) {
    std::vector<Request> requests;
    // argv[1] must be used to specify client/server/ace mode
    if (strcmp(argv[1], "s") && strcmp(argv[1], "c"))
    {
        fprintf(stderr,
            "The first parameter must be specifying if it's client (c) or server (s) mode?\n");
        exit(-1);
    }

    requests.emplace_back(argv[2]);
    gfx(requests, !strcmp(argv[1], "s"));

    return 0;
}
