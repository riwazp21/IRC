# Internet Relay Chat (IRC) Application

## Overview
This project implements a simplified Internet Relay Chat (IRC) system, providing core functionality inspired by modern communication platforms like Discord or Slack. It enables real-time text-based communication between multiple users through a client-server architecture and supports both TCP and UDP protocols for efficient and robust interactions. The application adheres to RFC 2812 specifications, focusing on connection registration, channel operations, messaging, and protocol-defined response codes.

## Features

### Command Support
The IRC server implements the following core commands as defined in RFC 2812:

#### **Connection Registration (Section 3.1)**
- **NICK**: Allows a user to set or change their nickname.
- **USER**: Registers a user with the server, providing essential details.
- **MODE**: Views or changes user or channel modes, controlling permissions and behaviors.
- **QUIT**: Disconnects a user from the server gracefully.

#### **Channel Operations (Section 3.2)**
- **JOIN**: Allows a user to join a specific channel.
- **PART**: Allows a user to leave a specific channel.
- **MODE**: Views or changes channel modes, such as setting topics or restricting access.
- **TOPIC**: Views or changes the topic of a channel.
- **LIST**: Lists all channels or channels matching a given search mask.
- **NAMES**: Lists all visible users currently in a channel.

#### **Messaging (Section 3.3)**
- **PRIVMSG**: Sends a private message to a specific user or channel.

### Response Codes
The server supports a range of numeric reply codes for protocol conformance and robust error handling:
- **001 (RPL_WELCOME)**: Welcome message upon successful connection.
- **002 (RPL_YOURHOST)**: Server host and version information.
- **004 (RPL_MYINFO)**: General server information.
- **301 (RPL_AWAY)**: Indicates a user is marked as away.
- **322 (RPL_LIST)**: Provides channel information (from the LIST command).
- **323 (RPL_LISTEND)**: Indicates the end of the channel list.
- **401 (ERR_NOSUCHNICK)**: Indicates that no such nickname exists.
- **403 (ERR_NOSUCHCHANNEL)**: Indicates that no such channel exists.
- **404 (ERR_CANNOTSENDTOCHAN)**: Indicates a message cannot be sent to a channel.
- **431 (ERR_NONICKNAMEGIVEN)**: Indicates no nickname was provided.
- **432 (ERR_ERRONEUSNICKNAME)**: Indicates an invalid nickname.
- **433 (ERR_NICKNAMEINUSE)**: Indicates that the nickname is already in use.
- **461 (ERR_NEEDMOREPARAMS)**: Indicates that not enough parameters were provided.
- **501 (ERR_UMODEUNKNOWNFLAG)**: Indicates an unknown user mode flag.
- **502 (ERR_USERSDONTMATCH)**: Indicates that a user cannot change another user's mode.

### Protocols
- **TCP**: Handles reliable message delivery and command execution.
- **UDP**: Provides out-of-band communication for:
  - **Heartbeat Signals**: Server periodically checks client activity.
  - **Server Statistics**: Broadcasts server performance metrics.
  - **Real-time Notifications**: Delivers urgent alerts and announcements.

### Configuration
- **Server Configuration (`server.conf`)**:
  ```plaintext
  TCP_PORT=12345
  HEARTBEAT_INTERVAL=10
  STATS_INTERVAL=15
  ```
- **Client Configuration (`client.conf`)**:
  ```plaintext
  SERVER_IP=127.0.0.1
  SERVER_PORT=12345
  ```
### Usage
Run the following command to compile the server and client
```plaintext
make
```
### Running the server
```plaintext
./server server.conf
```
### Running the client
```plaintext
./client client.conf
```
### Development Environment
This project was developed, tested, and debugged on a Linux server using tools like vim and command-line utilities. The use of Linux utilities ensured efficient development and reliable performance. Network traffic was analyzed and validated using tools like Wireshark and tshark.

### Future Enhancement
Adding server-to-server communication for a distributed IRC network.
Enhancing the client with a graphical user interface.
Implementing a persistent database for user and channel data.

### References
RFC 2812: Internet Relay Chat: Client Protocol
chi IRC: Example IRC interactions





  

