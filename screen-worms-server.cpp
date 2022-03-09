#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <vector>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <math.h>
#include <signal.h>
#include "err.h"
#include "crc.h"

bool operator < (const sockaddr_storage & left,
    const sockaddr_storage & right) {
    auto l_family = left.ss_family;
    auto r_family = right.ss_family;
    if (l_family != r_family) {
        return l_family < r_family;
    }
    if (l_family == AF_INET) {
        sockaddr_in * l = (sockaddr_in * ) & left;
        sockaddr_in * r = (sockaddr_in * ) & right;
        return l -> sin_port < r -> sin_port || (l -> sin_port == r -> sin_port && (
            l -> sin_addr.s_addr < r -> sin_addr.s_addr));
    } else {
        sockaddr_in6 * l = (sockaddr_in6 * ) & left;
        sockaddr_in6 * r = (sockaddr_in6 * ) & right;
        return l -> sin6_port < r -> sin6_port || (l -> sin6_port == r -> sin6_port && (
            memcmp(l -> sin6_addr.s6_addr, r -> sin6_addr.s6_addr, 16) < 0));
    }
}

std::vector < uint8_t > events; //<event_num, event_type>
std::set < std::string > game_start_players;
std::set < std::string > connected_players;
std::set < std::string > eliminated_players;
std::map < sockaddr_storage, std::string > connected_clients;
std::unordered_map < uint32_t, std::pair < uint8_t, std::pair < uint32_t, uint32_t >>> pixel_events; // event_num, <player_num,<x,y>>
std::unordered_map < uint32_t, uint8_t > eliminated_players_events; //event num, player_num
uint32_t game_id;
std::map < std::string, uint8_t > player_numbers;
std::map < sockaddr_storage, uint32_t > clients;
std::map < sockaddr_storage, timeval > last_message_times;

std::map < std::string, std::pair < double, double >> positions;
std::map < std::string, int32_t > directions;
std::map < std::string, bool > has_pressed;
std::map < std::string, uint8_t > turn_directions;
std::set < std::pair < uint32_t, uint32_t >> eaten_pixels;
bool in_game = false;

int size_sent = 0;
bool rdy_to_send = false;
uint8_t tab[548];

int size_sent_loop = 0;
bool rdy_to_send_loop = false;
uint8_t tab_loop[548];

std::string port_n = "2021";
int turn_speed = 6;
int rounds_sec = 50;
uint32_t seed = time(NULL) & 0xffffffff;
uint32_t width = 640;
uint32_t height = 480;
int sock;

ssize_t len, snd_len;
sockaddr_storage client_address;
socklen_t client_addrlen = (socklen_t) sizeof(client_address);

struct __attribute__((__packed__)) client_message {
    uint64_t session_id;
    uint8_t turn_direction;
    uint32_t next_expected_event_no;
    char player_name[20];
};

client_message msg;

uint32_t myrand() {
    auto ret = seed;
    uint64_t seed64 = seed;
    seed64 *= 279410273;
    seed64 %= 4294967291;
    seed = seed64;
    return ret;
}

void addToTab(uint32_t event_num) {
    uint8_t event_kind = events.at(event_num);

    int record_size;
    uint32_t len; // event_* size
    uint32_t sending_pom;

    if (size_sent == 0) {
        memcpy(tab, & game_id, sizeof(game_id));
        size_sent += sizeof(game_id);
    }

    if (event_kind == 1) { //pixel
        len = 14;
        record_size = 22;
        if (record_size + size_sent > 548) {
            rdy_to_send = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab + size_sent, & sending_pom, sizeof(len));
        size_sent += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab + size_sent, & sending_pom, sizeof(event_num));
        size_sent += sizeof(event_num);
        memcpy(tab + size_sent, & event_kind, sizeof(event_kind));
        size_sent += sizeof(event_kind);
        memcpy(tab + size_sent, & pixel_events[event_num].first, sizeof(pixel_events[event_num].first));
        size_sent += sizeof(pixel_events[event_num].first);
        sending_pom = htonl(pixel_events[event_num].second.first);
        memcpy(tab + size_sent, & sending_pom, sizeof(pixel_events[event_num].second.first));
        size_sent += sizeof(pixel_events[event_num].second.first);

        sending_pom = htonl(pixel_events[event_num].second.second);
        memcpy(tab + size_sent, & sending_pom, sizeof(pixel_events[event_num].second.second));
        size_sent += sizeof(pixel_events[event_num].second.second);
        uint32_t crc = crcSlow(tab + size_sent - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab + size_sent, & sending_pom, sizeof(crc));
        size_sent += sizeof(crc);
    } else if (event_kind == 2) { //player elminated
        len = 6;
        record_size = 14;
        if (record_size + size_sent > 548) {
            rdy_to_send = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab + size_sent, & sending_pom, sizeof(len));
        size_sent += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab + size_sent, & sending_pom, sizeof(event_num));
        size_sent += sizeof(event_num);
        memcpy(tab + size_sent, & event_kind, sizeof(event_kind));
        size_sent += sizeof(event_kind);
        memcpy(tab + size_sent, & eliminated_players_events.at(event_num), sizeof(eliminated_players_events.at(event_num)));
        size_sent += sizeof(eliminated_players_events.at(event_num));
        uint32_t crc = crcSlow(tab + size_sent - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab + size_sent, & sending_pom, sizeof(crc));
        size_sent += sizeof(crc);
    } else if (event_kind == 3) {
        len = 5;
        record_size = 13;
        if (record_size + size_sent > 548) {
            rdy_to_send = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab + size_sent, & sending_pom, sizeof(len));
        size_sent += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab + size_sent, & sending_pom, sizeof(event_num));
        size_sent += sizeof(event_num);
        memcpy(tab + size_sent, & event_kind, sizeof(event_kind));
        size_sent += sizeof(event_kind);
        uint32_t crc = crcSlow(tab + size_sent - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab + size_sent, & sending_pom, sizeof(crc));
        size_sent += sizeof(crc);
    } else { // event_num == 0
        len = 13;
        for (const auto & value: game_start_players) {
            len += value.size() + 1;
        }
        record_size = len + 8;
        if (record_size + size_sent > 548) {
            rdy_to_send = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab + size_sent, & sending_pom, sizeof(len));
        size_sent += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab + size_sent, & sending_pom, sizeof(event_num));
        size_sent += sizeof(event_num);
        memcpy(tab + size_sent, & event_kind, sizeof(event_kind));
        size_sent += sizeof(event_kind);
        sending_pom = htonl(width);
        memcpy(tab + size_sent, & sending_pom, sizeof(width));
        size_sent += sizeof(width);
        sending_pom = htonl(height);
        memcpy(tab + size_sent, & sending_pom, sizeof(height));
        size_sent += sizeof(height);
        for (const auto & value: game_start_players) {
            value.copy((char * ) tab + size_sent, value.size(), 0);
            tab[size_sent + value.size()] = '\0';
            size_sent += value.size() + 1;
        }
        uint32_t crc = crcSlow(tab + size_sent - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab + size_sent, & sending_pom, sizeof(crc));
        size_sent += sizeof(crc);
    }
}

void addToTab_loop(uint32_t event_num) {
    uint8_t event_kind = events.at(event_num);

    int record_size;
    uint32_t len; // event_* size
    uint32_t sending_pom;

    if (size_sent_loop == 0) {
        memcpy(tab_loop, & game_id, sizeof(game_id));
        size_sent_loop += sizeof(game_id);
    }

    if (event_kind == 1) { //pixel
        len = 14;
        record_size = 22;
        if (record_size + size_sent_loop > 548) {
            rdy_to_send = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(len));
        size_sent_loop += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(event_num));
        size_sent_loop += sizeof(event_num);
        memcpy(tab_loop + size_sent_loop, & event_kind, sizeof(event_kind));
        size_sent_loop += sizeof(event_kind);
        memcpy(tab_loop + size_sent_loop, & pixel_events[event_num].first, sizeof(pixel_events[event_num].first));
        size_sent_loop += sizeof(pixel_events[event_num].first);
        sending_pom = htonl(pixel_events[event_num].second.first);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(pixel_events[event_num].second.first));
        size_sent_loop += sizeof(pixel_events[event_num].second.first);

        sending_pom = htonl(pixel_events[event_num].second.second);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(pixel_events[event_num].second.second));
        size_sent_loop += sizeof(pixel_events[event_num].second.second);
        uint32_t crc = crcSlow(tab_loop + size_sent_loop - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(crc));
        size_sent_loop += sizeof(crc);
    } else if (event_kind == 2) { //player elminated
        len = 6;
        record_size = 14;
        if (record_size + size_sent_loop > 548) {
            rdy_to_send_loop = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(len));
        size_sent_loop += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(event_num));
        size_sent_loop += sizeof(event_num);
        memcpy(tab_loop + size_sent_loop, & event_kind, sizeof(event_kind));
        size_sent_loop += sizeof(event_kind);
        memcpy(tab_loop + size_sent_loop, & eliminated_players_events.at(event_num), sizeof(eliminated_players_events.at(event_num)));
        size_sent_loop += sizeof(eliminated_players_events.at(event_num));
        uint32_t crc = crcSlow(tab_loop + size_sent_loop - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(crc));
        size_sent_loop += sizeof(crc);
    } else if (event_kind == 3) {
        len = 5;
        record_size = 13;
        if (record_size + size_sent_loop > 548) {
            rdy_to_send_loop = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(len));
        size_sent_loop += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(event_num));
        size_sent_loop += sizeof(event_num);
        memcpy(tab_loop + size_sent_loop, & event_kind, sizeof(event_kind));
        size_sent_loop += sizeof(event_kind);
        uint32_t crc = crcSlow(tab_loop + size_sent_loop - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(crc));
        size_sent_loop += sizeof(crc);
    } else { // event_num == 0
        len = 13;
        for (const auto & value: game_start_players) {
            len += value.size() + 1;
        }
        record_size = len + 8;
        if (record_size + size_sent_loop > 548) {
            rdy_to_send_loop = true;
            return;
        }
        sending_pom = htonl(len);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(len));
        size_sent_loop += sizeof(len);
        sending_pom = htonl(event_num);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(event_num));
        size_sent_loop += sizeof(event_num);
        memcpy(tab_loop + size_sent_loop, & event_kind, sizeof(event_kind));
        size_sent_loop += sizeof(event_kind);
        for (const auto & value: game_start_players) {
            value.copy((char * ) tab_loop + size_sent_loop, value.size(), 0);
            tab_loop[size_sent_loop + value.size()] = '\0';
            size_sent_loop += value.size() + 1;
        }
        uint32_t crc = crcSlow(tab_loop + size_sent_loop - len - sizeof(len), len + sizeof(len));
        sending_pom = htonl(crc);
        memcpy(tab_loop + size_sent_loop, & sending_pom, sizeof(crc));
        size_sent_loop += sizeof(crc);
    }
}

void genStart() {

    game_id = myrand();
    for (auto[sadr, nam]: connected_clients) {
        if (nam.empty())
            continue;
        game_start_players.insert(nam);
        connected_players.insert(nam);
    }
    int player_num = 0;
    for (auto & nam: game_start_players) {
        player_numbers[nam] = player_num;
        player_num++;
    }
    events.push_back(0);
}

void genPixel(const std::string & name, std::pair < uint32_t, uint32_t > coords) {
    pixel_events[events.size()] = std::make_pair(player_numbers[name], coords);
    events.push_back(1);
    eaten_pixels.insert(coords);
}

void genEliminated(const std::string & name) {
    eliminated_players_events[events.size()] = player_numbers[name];
    events.push_back(2);
}

void genEndgame() {
    events.push_back(3);
}

void startGame() {

    genStart();
    addToTab(0);
    for (auto[sadr, nam]: connected_clients) {
        snd_len = sendto(sock, & tab, size_sent, 0, (struct sockaddr * ) & sadr, client_addrlen);
        if (snd_len != size_sent)
            syserr("error on sending datagram to client socket");
    }
    size_sent = 0;
    std::vector < std::string > eliminated_clients;
    for (auto & nam: game_start_players) {
        double x = (myrand() % width) + 0.5;
        double y = (myrand() % height) + 0.5;
        positions[nam] = std::make_pair(x, y);
        directions[nam] = myrand() % 360;
        if (eaten_pixels.count(std::make_pair((uint32_t) x, (uint32_t) y)) > 0) {
            genEliminated(nam);
            eliminated_players.insert(nam);
        } else {
            genPixel(nam, std::make_pair((uint32_t) x, (uint32_t) y));
        }
        addToTab(events.size() - 1);
        for (auto[sadr, nam]: connected_clients) {
            snd_len = sendto(sock, & tab, size_sent, 0, (struct sockaddr * ) & sadr, client_addrlen);
            if (snd_len != size_sent)
                syserr("error on sending datagram to client socket");
        }
        size_sent = 0;
    }

    in_game = true;
}

void endGame() {
    in_game = false;
    genEndgame();
    addToTab_loop(events.size() - 1);
    for (auto[sadr, nam]: connected_clients) {
        snd_len = sendto(sock, & tab_loop, size_sent_loop, 0, (struct sockaddr * ) & sadr, client_addrlen);
        if (snd_len != size_sent_loop)
            syserr("error on sending datagram to client socket");
    }
    size_sent_loop = 0;
    events.clear();
    game_start_players.clear();
    eliminated_players.clear();
    connected_players.clear();
    pixel_events.clear();
    eliminated_players_events.clear();
    player_numbers.clear();
    positions.clear();
    directions.clear();
    has_pressed.clear();
    turn_directions.clear();
    eaten_pixels.clear();
}

void loop_handler(int) {
    if (!in_game) {
        return;
    }
    if (game_start_players.size() - eliminated_players.size() < 2) {
        endGame();
        return;
    }
    timeval tv;
    if (gettimeofday( & tv, NULL) < 0)
        syserr("error on getting time of day");
    std::vector < sockaddr_storage > to_disconnect;
    for (auto[sadr, nam]: connected_clients) {
        if (tv.tv_sec - last_message_times[sadr].tv_sec >= 2) {
            to_disconnect.push_back(sadr);
        }
    }
    for (auto sadr: to_disconnect) {
        connected_clients.erase(sadr);
    }
    for (auto & nam: game_start_players) {
        if (eliminated_players.count(nam) > 0)
            continue;
        if (turn_directions[nam] == 1) {
            directions[nam] += turn_speed;
        }

        if (turn_directions[nam] == 2) {
            directions[nam] -= turn_speed;
        }

        double x_diff = cos(M_PI / 180 * directions[nam]);
        double y_diff = sin(M_PI / 180 * directions[nam]);
        positions[nam].first += x_diff;
        positions[nam].second += y_diff;

        if ((int) positions[nam].first == (int)(positions[nam].first - x_diff) && (int) positions[nam].second == (int)(positions[nam].second - y_diff)) {
            continue;
        }

        if ((int) positions[nam].first < 0 || (int) positions[nam].second < 0 || (uint32_t) positions[nam].first >= width || (uint32_t) positions[nam].second >= height ||
            eaten_pixels.count(std::make_pair((uint32_t) positions[nam].first, (uint32_t) positions[nam].second))) {
            genEliminated(nam);
            eliminated_players.insert(nam);
        } else {

            genPixel(nam, std::make_pair((uint32_t) positions[nam].first, (uint32_t) positions[nam].second));
        }

        addToTab_loop(events.size() - 1);

        timeval tv;
        if (gettimeofday( & tv, NULL) < 0)
            syserr("error on getting time of day");

        for (auto[sadr, nam]: connected_clients) {
            snd_len = sendto(sock, & tab_loop, size_sent_loop, 0, (struct sockaddr * ) & sadr, client_addrlen);
            if (snd_len != size_sent_loop)
                syserr("error on sending datagram to client socket");
        }
        size_sent_loop = 0;
    }
}

int main(int argc, char * argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1) {
        switch (opt) {
        case 'p':
            port_n = optarg;
            break;
        case 's':
            seed = atoi(optarg);
            break;
        case 't':
            turn_speed = atoi(optarg);
            break;
        case 'v':
            rounds_sec = atoi(optarg);
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case '?':
            break;
        }
    }

    addrinfo addr_hints;
    addrinfo * addr_result;

    memset( & addr_hints, 0, sizeof(addr_hints));
    addr_hints.ai_family = AF_INET6;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(NULL, port_n.c_str(), & addr_hints, & addr_result);

    if (err == EAI_SYSTEM) {
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) {
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    sock = socket(addr_result -> ai_family, addr_result -> ai_socktype, addr_result -> ai_protocol);
    if (sock < 0)
        syserr("socket");

    if (bind(sock, addr_result -> ai_addr, addr_result -> ai_addrlen) < 0)
        syserr("bind");

    freeaddrinfo(addr_result);

    client_addrlen = (socklen_t) sizeof(client_address);

    itimerval timer;
    if (rounds_sec == 1) {
        timer.it_interval.tv_sec = 1;
        timer.it_interval.tv_usec = 0;
    } else {
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 1000000 / rounds_sec;
    }
    timer.it_value = timer.it_interval;
    int ret = setitimer(ITIMER_REAL, & timer, NULL);
    if (ret < 0)
        syserr("error on setting timer");
    signal(SIGALRM, loop_handler);
    for (;;) {
        (void) memset( & msg, 0, 33);
        len = recvfrom(sock, & msg, 33, 0, (struct sockaddr * ) & client_address, & client_addrlen);
        timeval tv;
        if (gettimeofday( & tv, NULL) < 0)
            syserr("error on getting time of day");
        std::vector < sockaddr_storage > to_disconnect;
        for (auto[sadr, nam]: connected_clients) {
            if (tv.tv_sec - last_message_times[sadr].tv_sec >= 2) {
                to_disconnect.push_back(sadr);
            }
        }
        for (auto sadr: to_disconnect) {
            connected_clients.erase(sadr);
        }
        if (len < 0) {
            syserr("error on datagram from client socket");
        } else if (len < 13) {
            std::cerr << "received an incomplete message from client" << std::endl;
        } else {
            msg.session_id = be64toh(msg.session_id);
            msg.next_expected_event_no = ntohl(msg.next_expected_event_no);
            std::string name = std::string(msg.player_name, len - 13);
            if (connected_clients.count(client_address) > 0) {
                if (connected_players.count(name) > 0 && connected_clients[client_address] != name)
                    continue;
                if (clients[client_address] > msg.session_id)
                    continue;
                if (clients[client_address] < msg.session_id || connected_clients[client_address] != name) {
                    connected_players.erase(connected_clients[client_address]);
                }
            }
            if (clients.count(client_address) > 0) {
                if (clients[client_address] < msg.session_id || connected_clients[client_address] != name) {
                    connected_players.erase(connected_clients[client_address]);
                }
            }
            clients[client_address] = msg.session_id;
            last_message_times[client_address] = tv;
            connected_clients[client_address] = name;
            if (connected_players.count(name) > 0)
                turn_directions[name] = msg.turn_direction;
            if (msg.turn_direction != 0) {
                has_pressed[name] = true;
            }
            for (uint32_t i = msg.next_expected_event_no; i < events.size(); i++) {
                addToTab(i);
                if (rdy_to_send || i == events.size() - 1) {
                    snd_len = sendto(sock, & tab, size_sent, 0, (struct sockaddr * ) & client_address, client_addrlen);
                    if (snd_len != size_sent)
                        syserr("error on sending datagram to client socket");
                    size_sent = 0;
                    rdy_to_send = false;
                }
            }
            if (game_start_players.empty()) {
                bool can_start = true;
                int named_cnt = 0;
                for (auto[sadr, nam]: connected_clients) {
                    if (!nam.empty()) {
                        can_start &= has_pressed[nam];
                        named_cnt++;
                    }
                }
                if (can_start && named_cnt >= 2) {
                    startGame();
                }
            }
        }
    }

    if (close(sock) == -1) {
        syserr("close");
    }

    return 0;
}