# Worms-game

Worms-game consists of one board shared amongs all players on which every player owns his own worm that is being controlled by him. There are 2 basic commands that any player can input: turn left or turn right. Every client is connected to game server via UDP connection and to GUI server via TCP. When GUI recives input to turn it sends given input to client in following form: 

LEFT_KEY_DOWN
LEFT_KEY_UP
RIGHT_KEY_DOWN
RIGHT_KEY_UP

client sends messages to GUI in 3 different forms:

NEW_GAME maxx maxy player_name1 player_name2 …
PIXEL x y player_name - it means that player player_name moved to new pixel
PLAYER_ELIMINATED player_name

Every client is connected to his own GUI server but all clients are connected to one main game server where all requests from clients are being dealt with and calculated.

Client sends messages to game server in the following form:

session_id: 8 byte unsigned integer
turn_direction: 1 byte unsigned integer value 0 → go straight, value 1 → turn right, value 2 → turn left
next_expected_event_no – 4 bytes unsigned integer
player_name: 0–20 ASCII characters with values from 33–126

and game server sends message to all clients in the following form:

game_id: 4 bytes unsigned integer
events: numerous records with form stated below

each event has following structure:

len: 4 bytes unsigned integer, states how many bytes all fields named event_* take
event_no: 4 byte unsigned integer, states which event this is (counting from 0)
event_type: 1 byte
event_data: depends on event type
crc32: 4 bytes unsigned integer, controll sum of all fields from len to event_data, calculated using standard CRC-32-IEEE algorythm

there are 4 possible event types:

NEW_GAME
 event_type: 0
 event_data:
  maxx: 4 bytes unsigned integer, board width in pixels
  maxy: 4 bytes unsigned integer, board hight in pixels
  list of all players containing their names with '\0' between them
 
PIXEL
 event_type: 1
 event_data:
  player_number: 1 byte
  x: 4 bytes unsigned integer
  y: 4 bytes unsigned integer
 
PLAYER_ELIMINATED
 event_type: 2
 event_data:
  player_number: 1 byte
 
GAME_OVER
 event_type: 3
 event_data: none

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
 * `game_server` – adress (IPv4 or IPv6) or game server name
 * `-n player_name` – player name
 * `-p n` – game server port (default 2021)
 * `-i gui_server` – adress (IPv4 or IPv6) or server name of server that handles user interface (default localhost)
 * `-r n` – server port of server that handles user interface (default 20210)

The following example illustrates game between two players played using GUI provided from https://students.mimuw.edu.pl/~zbyszek/sieci/gui/. In this example the game is being played on one device which hinders the movement of each player, bacause only one worm can be controlled at the time. Of course game can normally be played by multiple players on multiple devices.

https://user-images.githubusercontent.com/45102381/157502111-f5b09b27-7c1e-41a1-b7ea-701339a932d8.mp4

