#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include "read_line.h"
#include "err.h"
#include "crc.h"
#include <netinet/tcp.h>
#include <signal.h>
#include <fcntl.h>

uint32_t game_id = 0;
uint64_t session_id = time(NULL);;
uint8_t turn_direction = 0;
uint32_t next_expected_event_no = 0;
std::vector < std::string > names_vec;
std::string player_name = "";
std::string server_name = "";
std::string port_n = "2021";
std::string port_gui = "20210";
std::string gui_server = "localhost";
int sock, guisock;
sockaddr_storage server_address;
socklen_t server_addrlen;

void readFromServ(uint8_t * tab, ssize_t read_len) {
    uint32_t check_id;
    ssize_t iter = 0;
    uint32_t event_len = 0;
    uint32_t event_num = 0;
    uint8_t event_type;
    uint32_t maxx;
    uint32_t maxy;
    uint32_t x;
    uint32_t y;
    uint8_t player_num;
    uint32_t crc, crc2;
    std::string event_message;

    if ((unsigned long) read_len < sizeof(check_id) + sizeof(event_len)) {
        return;
    }
    memcpy( & check_id, tab, sizeof(check_id));
    check_id = ntohl(check_id);
    iter += sizeof(check_id);
    while (iter < read_len) {
        memcpy( & event_len, tab + iter, sizeof(event_len));
        event_len = ntohl(event_len);
        iter += sizeof(event_len);
        if ((unsigned long) read_len - iter < event_len + sizeof(crc)) {
            return;
        }
        crc = crcSlow(tab + iter - sizeof(event_len), event_len + sizeof(event_len));
        memcpy( & crc2, tab + iter + event_len, sizeof(crc2));
        crc2 = ntohl(crc2);

        if (crc != crc2) {
            return;
        }
        memcpy( & event_num, tab + iter, sizeof(event_num));
        event_num = ntohl(event_num);
        if (event_num != next_expected_event_no) {
            iter += event_len + sizeof(crc);
            continue;
        }
        iter += sizeof(event_num);
        next_expected_event_no++;
        memcpy( & event_type, tab + iter, sizeof(event_type));
        iter += sizeof(event_type);
        if (event_type == 0) {
            game_id = check_id;
            memcpy( & maxx, tab + iter, sizeof(maxx));
            maxx = ntohl(maxx);
            iter += sizeof(maxx);
            memcpy( & maxy, tab + iter, sizeof(maxy));
            maxy = ntohl(maxy);
            iter += sizeof(maxy);
            int names_len = event_len - 13;
            while (names_len > 0) {
                names_vec.emplace_back((char * ) tab + iter);
                iter += names_vec.back().size() + 1;
                names_len -= names_vec.back().size() + 1;
            }
            event_message.append("NEW_GAME ");
            event_message.append(std::to_string(maxx));
            event_message.append(" ");
            event_message.append(std::to_string(maxy));
            for (auto & name: names_vec) {
                event_message.append(" ");
                event_message.append(name);
            }
            event_message.append("\n");
        } else if (event_type == 1) {
            memcpy( & player_num, tab + iter, sizeof(player_num));
            iter += sizeof(player_num);
            memcpy( & x, tab + iter, sizeof(x));
            x = ntohl(x);
            iter += sizeof(x);
            memcpy( & y, tab + iter, sizeof(y));
            y = ntohl(y);
            iter += sizeof(y);
            event_message.append("PIXEL ");
            event_message.append(std::to_string(x));
            event_message.append(" ");
            event_message.append(std::to_string(y));
            event_message.append(" ");
            event_message.append(names_vec[player_num]);
            event_message.append("\n");
        } else if (event_type == 2) {
            memcpy( & player_num, tab + iter, sizeof(player_num));
            iter += sizeof(player_num);
            event_message.append("PLAYER_ELIMINATED ");
            event_message.append(names_vec[player_num]);
            event_message.append("\n");
        } else if (event_type == 3) {
            next_expected_event_no = 0;
            names_vec.clear();
        } else {
            next_expected_event_no--;
            iter += event_len - sizeof(event_num) - sizeof(event_type);
        }
        iter += sizeof(crc);
        if (check_id == game_id) {

            int snd_len = write(guisock, event_message.c_str(), event_message.size());
            if (snd_len < 0)
                syserr("write error");
        }
        event_message.clear();
    }
}

void loop_handler(int) {
    char tab[13 + player_name.size()];
    uint64_t sending_pom64 = htobe64(session_id);
    memcpy(tab, & sending_pom64, sizeof(sending_pom64));
    memcpy(tab + sizeof(session_id), & turn_direction, sizeof(turn_direction));
    uint32_t sending_pom32 = htonl(next_expected_event_no);
    memcpy(tab + sizeof(session_id) + sizeof(turn_direction), & sending_pom32, sizeof(sending_pom32));
    memcpy(tab + sizeof(session_id) + sizeof(turn_direction) + sizeof(next_expected_event_no), player_name.c_str(), player_name.size());
    ssize_t sent = write(sock, tab, 13 + player_name.size());
    if (sent != 13 + (ssize_t) player_name.size())
        syserr("error on sending");
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        fatal("Usage: %s address [options] ...\n", argv[0]);
    }
    server_name = argv[1];

    int opt;
    while ((opt = getopt(argc, argv, "n:p:i:r:")) != -1) {
        switch (opt) {
        case 'n':
            player_name = optarg;
            if (player_name.size() > 20) {
                std::cerr << "zbyt dluga nazwa użytkownika" << std::endl;
                exit(1);
            }
            for (auto ch: player_name) {
                if (ch < 33 || ch > 126) {
                    std::cerr << "niepoprawny znak w nazwie użytkownika" << std::endl;
                    exit(1);
                }
            }
            break;
        case 'p':
            port_n = optarg;
            break;
        case 'i':
            gui_server = optarg;
            break;
        case 'r':
            port_gui = optarg;
            break;
        case '?':
            break;
        }
    }

    addrinfo addr_hints;
    addrinfo * addr_result;
    (void) memset( & addr_hints, 0, sizeof(addrinfo));
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_flags = AI_V4MAPPED;

    int err = getaddrinfo(server_name.c_str(), port_n.c_str(), & addr_hints, & addr_result);
    if (err == EAI_SYSTEM) {
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) {
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    sock = socket(addr_result -> ai_family, addr_result -> ai_socktype, addr_result -> ai_protocol);

    memcpy( & server_address, addr_result -> ai_addr, addr_result -> ai_addrlen);
    server_addrlen = (socklen_t) addr_result -> ai_addrlen;
    if (connect(sock, addr_result -> ai_addr, addr_result -> ai_addrlen) < 0)
        syserr("connect");
    if (sock < 0)
        syserr("socket");

    freeaddrinfo(addr_result);

    (void) memset( & addr_hints, 0, sizeof(addrinfo));
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    addr_hints.ai_flags = AI_V4MAPPED;

    err = getaddrinfo(gui_server.c_str(), port_gui.c_str(), & addr_hints, & addr_result);
    if (err == EAI_SYSTEM) {
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) {
        fatal("getaddrinfo: %s", gai_strerror(err));
    }
    guisock = socket(addr_result -> ai_family, addr_result -> ai_socktype, addr_result -> ai_protocol);
    if (guisock < 0)
        syserr("socket");

    if (connect(guisock, addr_result -> ai_addr, addr_result -> ai_addrlen) < 0)
        syserr("connect");
    freeaddrinfo(addr_result);

    itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 30000;
    timer.it_value = timer.it_interval;
    int ret = setitimer(ITIMER_REAL, & timer, NULL);
    if (ret < 0)
        syserr("error on setting timer");
    signal(SIGALRM, loop_handler);

    if (setsockopt(guisock, IPPROTO_TCP, TCP_NODELAY, & opt, (socklen_t) sizeof(opt)) == -1)
        syserr("setsockopt");

    std::thread with_gui_thread([ & ] {
        char tab[15];
        while (true) {
            ssize_t read_len = readLine(guisock, tab, 16);
            if (strncmp(tab, "LEFT_KEY_DOWN\n", read_len) == 0)
                turn_direction = 2;
            if (strncmp(tab, "LEFT_KEY_UP\n", read_len) == 0)
                turn_direction = turn_direction == 2 ? 0 : turn_direction;
            if (strncmp(tab, "RIGHT_KEY_DOWN\n", read_len) == 0)
                turn_direction = 1;
            if (strncmp(tab, "RIGHT_KEY_UP\n", read_len) == 0)
                turn_direction = turn_direction == 1 ? 0 : turn_direction;
        }
    });
    std::thread with_srvr_thread([ & ] {
        while (true) {
            uint8_t tab[548];
            int rcv_len = read(sock, tab, 548);
            if (rcv_len < 0) {
                syserr("read error");
            }
            readFromServ(tab, rcv_len);
        }
    });

    with_gui_thread.join();
    with_srvr_thread.join();

    if (close(sock) == -1) {
        syserr("close");
    }

    return 0;
}