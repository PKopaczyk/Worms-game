# Worms-game

Instructions to run the program are as follows:

Server:

./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]

  * `-p n` – port num (default `2021`)
  * `-s n` – random number generator seed (default value is `time(NULL)`)
  * `-t n` – Integer being a rotation speed
    (parameter `TURNING_SPEED`, default `6`)
  * `-v n` – Integer beint a game speed
    (parameter `ROUNDS_PER_SEC` default `50`)
  * `-w n` – board width in pixels (default `640`)
  * `-h n` – board hight in pixels (default `480`)

client:

    ./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]
game_server – adress (IPv4 or IPv6) or game server name
-n player_name – player name
-p n – game server port (default 2021)
-i gui_server – adress (IPv4 or IPv6) or server name of server that handles user interface (default localhost)
-r n – server port of server that handles user interface (default 20210)
