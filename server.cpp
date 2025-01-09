#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <system_error>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <thread> 
#include <chrono>
#include <regex>
#include <sys/sysinfo.h>
#define BACKLOG 10
#define MAXDATASIZE 100
using namespace std;

const int ERR_NEEDMOREPARAMS = 461;
const int ERR_ALREADYREGISTRED = 433;
const int RPL_WELCOME = 001;
const int ERR_USERSDONTMATCH = 502;
const int ERR_UMODEUNKNOWNFLAG = 501;
const int PORT_OFFSET = 1000;
const int TIMEOUT = 20;
std::vector<sockaddr_in> clientAddresses;
int clientNum;

struct Messages {
    int messageId;
    int recipient_fd;
    string message;
};

struct UserInfo
{
    string nickname;
    string username;
    string mode;
    string realname;
    string new_fd;
    int udpPort;
    string ip;
};

struct Channel
{
    string channelName;
    string topic;
    string modes;
    vector<string> users;
};

void sigchld_handler(int s);
void parseChannels(const string &filename, vector<Channel> &channels);
void writeChannelsToFile(const string &filename, const vector<Channel> &channels);
bool channelExists(string channelName, vector<Channel> channels_new);
void joinChannels(string nickname, vector<string> channels);
int removeUserFromChannel(string nickname, string channelName);
int removeChannels(string nickname, vector<string> &channels, string message);
void *get_in_addr(struct sockaddr *sa);
std::string toupper(const std::string &input);
string toLower(const string input);
void logConnection(const std::string &clientIP);
void logDisconnection(const std::string &clientIP);
void writeUserData(const string &filename, const string &nickname, const string &username, const string &realname, const string &mode, const string &new_fd, int &udpPort, string &ip);
vector<UserInfo> loadUsersFromFile(const string &fileName);
void saveUsersToFile(const string &filename, const vector<UserInfo> &users);
bool isErroneousNickname(const string &nickname);
bool isNickNameInUse(const string &nickname);
bool isUserNameInUse(vector<UserInfo> &users, const string &username, const string &nickname, const string &new_fd);
int processNickCommand(const string &message, string &nickname);
int processUserCommand(const string message, string nickname, string &username, string &realname, string &mode, const string &new_fd, int udpPort, string &ip);
void applyModes(string &currentMode, const string &modes, bool isChannel);
string getMode(vector<UserInfo> &users, const string &nickname);
string getModeChannel(vector<Channel> &channels, const string &name);
int processModeCommand(const string message, const string nickname, string &username, string &realname, string &mode);
void deleteUserByNickname(vector<UserInfo> &users, const string &nickname);
string processQuitCommand(const string &message);
vector<string> findChannelsWithUser(const string &nickname, vector<Channel> &channels);
bool isValidChannelName(const string &channelName);
int processJoinCommand(const string &params, vector<string> &channels, string &nickname);
int processPartCommand(const string &params, vector<string> &channels, string &nickname);
void changeTopic(string channel, string topic);
string viewTopic(string channel);
int processTopicCommand(const string &receivedMsg, string &view_topic, bool &viewTopicbool);
void listFunction(string &description, int flag, vector<string> listOfChannel);
int processListCommand(const string &receivedMsg, string &description);
void namesFunction(string &description, int flag, vector<string> listOfChannel);
int processNamesCommand(const string &receivedMsg, string &description);
int getFD(const vector<UserInfo> &users, const string &nickname);
void udpBroadcast(int udpSockfd);
void listenToPrivMsg();
vector<Messages> loadMessages();
void printMessages(vector<Messages> messages);
void takeCareOfMessage();
void appendToMessageDatabase(int value1, int value2, const string &text, const string &filename);
bool doesUserExist(const string &sender, const string &recipient, const string &textToSend, int &newReplyCodeIfThingsExist);
bool doesUserExistInChannel(const Channel &channel, const string &sender);
bool doesChannelExist(const string &sender, const string &recipient, const string &textToSend, int &newReplyCodeIfThingsExist);
int processPrivMsg(string &sendersName, std::string &message);
void sendUdpPacket(int udpPort, const char *message, const std::string &IP);
void parseConfigFile(const string &filename, string &tcpPort, int &heartbeatInterval, int &statsInterval);
void sendHeartbeatToUsers(int heartbeat);
void broadCast(int statsInterval);
bool getUdpInfo(const string &nickname, int &udpPort, string &ip);
void sendJoinMessage(const Channel &channel);
void partMessage(const std::string &channel, const std::string &nickname, const std::string &message);



void sigchld_handler(int s)
{
    (void)s;

    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

void parseChannels(const string &filename, vector<Channel> &channels)
{
    ifstream file(filename);

    if (!file.is_open())
    {
        cerr << "Unable to open file: " << filename << endl;
        return;
    }

    string line;
    while (getline(file, line))
    {
        stringstream ss(line);
        Channel channel;
        string usersStr;

        // Parse the channel name
        getline(ss, channel.channelName, ';');

        // Parse the topic
        getline(ss, channel.topic, ';');

        // Parse the modes
        getline(ss, channel.modes, ';');

        // Parse the users
        getline(ss, usersStr, ';');

        stringstream userStream(usersStr);
        string user;
        while (getline(userStream, user, ','))
        {
            channel.users.push_back(user);
        }
        channels.push_back(channel);
    }

    file.close();
}
void writeChannelsToFile(const string &filename, const vector<Channel> &channels)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cerr << "Unable to open file for writing: " << filename << endl;
        return;
    }

    for (const auto &channel : channels)
    {
        // Write channel name, topic, and modes
        file << channel.channelName << ";"
             << channel.topic << ";"
             << channel.modes << ";";

        // Write users separated by commas
        for (size_t i = 0; i < channel.users.size(); ++i)
        {
            file << channel.users[i];
            if (i < channel.users.size() - 1)
            {
                file << ",";
            }
        }

        // End each channel with a semicolon and newline
        file << ";" << endl;
    }

    file.close();
}

bool channelExists(string channelName, vector<Channel> channels_new)
{
    for (auto &channel : channels_new)
    {
        if (channel.channelName == channelName)
        {
            
            return true;
        }
    }
    return false;
}

void joinChannels(string nickname, vector<string> channels)
{
    string filename = "channels.txt";
    vector<Channel> channels_new;
    parseChannels(filename, channels_new);

    bool newChannel_exist = true;
    for (auto &newChannel : channels)
    {
        bool doesChannelExist = channelExists(newChannel, channels_new);
        if (doesChannelExist)
        {
            for (auto &existingChannels : channels_new)
            {
                if (existingChannels.channelName == newChannel)
                {
                    sendJoinMessage(existingChannels);
                    if(existingChannels.modes != "p" && existingChannels.modes != "s")
                    {
                    existingChannels.users.push_back(nickname);
                    }
                }
            }
        }
        else
        {
            
            Channel append;
            append.channelName = newChannel;
            append.users.push_back(nickname);
            channels_new.push_back(append);
        }
    }
    writeChannelsToFile("channels.txt", channels_new);
}

int removeUserFromChannel(string nickname, string channelName)
{
    string filename = "channels.txt";
    vector<Channel> channels_existing;

    // Parse the existing channels from the file
    parseChannels(filename, channels_existing);

    // Check if the channel exists
    bool doesChannelExist = channelExists(channelName, channels_existing);

    if (!doesChannelExist)
    {
        return 403; 
    }

    // Iterate over the channels to find the target channel
    for (auto &channel : channels_existing)
    {
        if (channel.channelName == channelName)
        {
            // Find the user in the vector and remove them
            auto user_it = find(channel.users.begin(), channel.users.end(), nickname);
            if (user_it != channel.users.end())
            {
                channel.users.erase(user_it); 

                // Write the updated channels back to the file
                writeChannelsToFile("channels.txt", channels_existing);
                return 1; // Success
            }
            else
            {
                return 404; // User not found in the channel
            }
        }
    }

    return 403; // This line will never be reached
}



int removeChannels(string nickname, vector<string> &channels, string message)
{
    if(channels.size() > 1 )
    {
    for (auto &channel : channels)
    {
        int replyCode = removeUserFromChannel(nickname, channel);
        if (replyCode == 404)
        {
            return 404;
        }
        else if (replyCode == 403)
        {
            return 403;
        }
    }
    }
    else 
    {
        int replyCode = removeUserFromChannel(nickname,channels.front());
        if (replyCode == 404)
        {
            return 404;
        }
        else if (replyCode == 403)
        {
            return 403;
        }

        else 
        {
            if(message.size()!=0)
        {
            partMessage(channels.front(), nickname, message);
        }
        }
    }
    return 1;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

std::string toupper(const std::string &input)
{
    std::string output;
    bool capitalize = true;

    for (char c : input)
    {
        if (std::isalpha(c))
        {
            if (capitalize)
            {
                output += std::toupper(c);
            }
            else
            {
                output += std::toupper(c);
            }
            capitalize = !capitalize;
        }
        else
        {
            output += c;
        }
    }
    return output;
}

string toLower(const string input)
{
    string output;
    for (char c : input)
    {
        output += tolower(c);
    }
    return output;
}

void logConnection(const std::string &clientIP)
{
    time_t now = time(nullptr);
    tm *localTime = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime);
    std::cout << "[" << timestamp << "] Connection from: " << clientIP << std::endl;
}

void logDisconnection(const std::string &clientIP)
{
    time_t now = time(nullptr);
    tm *localTime = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime);
    std::cout << "[" << timestamp << "] Client disconnected: " << clientIP << std::endl;
}

void writeUserData(const string &filename, const string &nickname, const string &username, const string &realname, const string &mode, const string &new_fd, int& udpPort, string& ip)
{
    int FD_value = stoi(new_fd);
    ofstream userFile(filename, ios::app); // Append mode
    if (userFile.is_open())
    {
        userFile << "Nickname: " << nickname << ", Username: " << username << ", Realname: " << realname << ", Mode: " << mode << ", FD: " << new_fd << ", UDP: "<<udpPort<< ", IP: "<<ip<<"\n";
        userFile.close();
    }
    else
    {
        cerr << "Unable to open user_data.txt" << endl;
    }
    
}

vector<UserInfo> loadUsersFromFile(const string &fileName)
{
    vector<UserInfo> users;
    ifstream userFile(fileName);
    string line;

    if (!userFile.is_open())
    {
        cerr << "Unable to open file: " << fileName << endl;
        return users; // Return empty vector if file can't be opened
    }

    while (getline(userFile, line))
    {
        UserInfo userInfo;
        stringstream ss(line);
        string temp;
        string udpPort;

        // Parse the "Nickname"
        getline(ss, temp, ':');
        getline(ss, userInfo.nickname, ',');

        // Parse the "Username"
        getline(ss, temp, ':');
        getline(ss, userInfo.username, ',');

        // Parse the "Realname"
        getline(ss, temp, ':');
        getline(ss, userInfo.realname, ',');

        // Parse the "Mode"
        getline(ss, temp, ':');
        getline(ss, userInfo.mode, ',');

        // Parse the "FD"
        getline(ss, temp, ':');
        getline(ss, userInfo.new_fd, ',');

        getline(ss, temp, ':');
        getline(ss, udpPort, ',');

        getline(ss,temp, ':');
        getline(ss,userInfo.ip);

        if (!userInfo.nickname.empty()) {
    userInfo.nickname = userInfo.nickname.substr(1);
}
if (!userInfo.username.empty()) {
    userInfo.username = userInfo.username.substr(1);
}
if (!userInfo.realname.empty()) {
    userInfo.realname = userInfo.realname.substr(1);
}
if (!userInfo.mode.empty()) {
    userInfo.mode = userInfo.mode.substr(1);
}
if (!userInfo.new_fd.empty()) {
    userInfo.new_fd = userInfo.new_fd.substr(1);
}
if (!userInfo.ip.empty()) {
    userInfo.ip = userInfo.ip.substr(1);
}
if (!udpPort.empty()) {
    udpPort = udpPort.substr(1);
}
        int intUdp;
        if(udpPort != "")
        {
         intUdp = stoi(udpPort);
        }
       else 
       {
         intUdp = 111111111;
       }
        userInfo.udpPort = intUdp;

        users.push_back(userInfo);
    }

    userFile.close();
    return users;
}

void saveUsersToFile(const string &filename, const vector<UserInfo> &users)
{
    ofstream userFile(filename);
    if (userFile.is_open())
    {
        for (const auto &user : users)
        {
            userFile << "Nickname: " << user.nickname << ", Username: " << user.username << ", Realname: " << user.realname << ", Mode: " << user.mode << ", FD: " << user.new_fd <<", UDP: "<<user.udpPort<< ", IP: "<<user.ip<<"\n"; 
        }
        userFile.close();
    }
}

bool isErroneousNickname(const string &nickname)
{
   regex nicknamePattern("^([a-zA-Z!@#$%^&*]){1,8}([a-zA-Z0-9!@#$%^&*\\-])$"); //regex for username defined in RFC
   return regex_match(nickname, nicknamePattern);
}

bool isNickNameInUse(const string &nickname)
{
    std::vector<UserInfo> allUsers = loadUsersFromFile("users.txt");
    for (const UserInfo &users : allUsers)
    {

        if (nickname == users.nickname)
        {
            return true;
        }
    }
    return false;
}

bool isUserNameInUse(vector<UserInfo> &users, const string &username, const string &nickname, const string &new_fd)
{
    // Loop through the list of users
    for (UserInfo &user : users)
    {
        if (user.username == username)
        {
            user.nickname = nickname;
            user.new_fd = new_fd;
            saveUsersToFile("users.txt", users);
            return true; // Username found
        }
    }
    return false; // Username not found
}

int processNickCommand(const string &message, string &nickname)
{
    if (message.length() <= 5)
    {
        return 431; // ERR_NONICKNAMEGIVEN: No nickname given
    }

    string newNickname = message.substr(5); // Extract the nickname

    if (newNickname.empty())
    {
        return 431; // ERR_NONICKNAMEGIVEN: No nickname provided
    }

    if (!isErroneousNickname(newNickname))
    {
        return 432; // ERR_ERRONEUSNICKNAME: Erroneous nickname
    }

    if (isNickNameInUse(newNickname))
    {
        return 433;
    }

    // If everything is good, register the nickname
    nickname = newNickname;
    return 200; // Success: Nickname registered
}

int processUserCommand(const string message, string nickname, string &username, string &realname, string &mode, const string &new_fd, int udpPort, string& ip)
{
    // Check if the user is already registered
    vector<UserInfo> users = loadUsersFromFile("users.txt");

    // Make sure the message starts with "USER"

    string concat = toupper(message);

    if (concat.substr(0, 5) != "USER ")
    {
        return ERR_NEEDMOREPARAMS; // Incorrect command format
    }

    size_t startPos = 5; // Skip "USER "
    size_t nextPos;

    // Extract username
    nextPos = message.find(' ', startPos);
    if (nextPos == string::npos)
        return ERR_NEEDMOREPARAMS;
    username = message.substr(startPos, nextPos - startPos);
    bool userNameInUse = isUserNameInUse(users, username, nickname, new_fd);
    if (userNameInUse == true)
    {
        return RPL_WELCOME;
    }

    // Extract mode
    startPos = nextPos + 1;
    nextPos = message.find(' ', startPos);
    if (nextPos == string::npos)
        return ERR_NEEDMOREPARAMS;

    mode = message.substr(startPos, nextPos - startPos);
    //int modeValue;

    // Check if mode is numeric and valid
    int modeValue = stoi(mode);

    if (modeValue & (1 << 2))
    {
        mode = 'w'; // Set wallops mode if bit 2 is 1
    }
    else if (modeValue & (1 << 3))
    {
        mode = 'i'; // Set invisible mode if bit 3 is 1
    }
    else 
    {
        mode = 'N';
    }
   

    // Extract realname (starts after ':')
    startPos = message.find(':', nextPos);
    if (startPos == string::npos)
        return ERR_NEEDMOREPARAMS;
    realname = message.substr(startPos + 1); // Skip ':'

    if (username.empty() || mode.empty() || realname.empty())
    {
        return ERR_NEEDMOREPARAMS;
    }

    if (users.empty())
    {
        ofstream file("users.txt");
        if (file.is_open())
        {
            file.close();
        }
    }

    writeUserData("users.txt", nickname, username, realname, mode, new_fd, udpPort, ip);
    return RPL_WELCOME;
}

void applyModes(string &currentMode, const string &modes, bool isChannel) {
    bool addingMode = true; // true means '+' (add), false means '-' (remove)

    for (char c : modes) {
        if (c == '+') {
            addingMode = true;
        } else if (c == '-') {
            addingMode = false;
        } else {
            if (isChannel) {
                // Channel mode logic: only 'a', 's', 'p' allowed with specific rules
                if (c != 'a' && c != 's' && c != 'p') continue;

                if (addingMode) {
                    if (c == 'p' && currentMode.find('s') != string::npos) {
                        // Ignore 'p' if 's' is already set as defined in RFC
                        continue;
                    }

                    if (currentMode.find(c) == string::npos) {
                        // Add mode if not present and meets constraints
                        if (c == 's' && currentMode.find('p') != string::npos) {
                            // Replace 'p' with 's' as defined in RFC
                            currentMode.erase(remove(currentMode.begin(), currentMode.end(), 'p'), currentMode.end());
                        }
                        currentMode += c;
                    }
                } else {
                    // Remove the mode if present as defined in RFC
                    currentMode.erase(remove(currentMode.begin(), currentMode.end(), c), currentMode.end());
                }

                // Ensure only allowed combinations (as or ap, otherwise single modes) Defined in RFC
                if (currentMode.find('a') != string::npos && currentMode.find('s') != string::npos) {
                    currentMode = "as";
                } else if (currentMode.find('a') != string::npos && currentMode.find('p') != string::npos) {
                    currentMode = "ap";
                } else if (currentMode.find('a') == string::npos && currentMode.find('s') != string::npos && currentMode.find('p') != string::npos) {
                    // Ensure either 's' or 'p', but not both if 'a' is absent
                    currentMode.erase(remove(currentMode.begin(), currentMode.end(), 'p'), currentMode.end());
                }
            } else {
                // User mode logic: allow 'a', 'i', 'w' in any combination Defined in the project direction file
                if (c != 'a' && c != 'i' && c != 'w') continue;

                if (addingMode) {
                    // Add mode if not already present defined in RFC
                    if (currentMode.find(c) == string::npos) {
                        currentMode += c;
                    }
                } else {
                    // Remove the mode if present defined in RFC
                    currentMode.erase(remove(currentMode.begin(), currentMode.end(), c), currentMode.end());
                }
            }
        }
    }
}

string getMode( vector<UserInfo> &users, const string &nickname)
{
    for (const auto &user : users)
    {
        if (user.nickname == nickname)
        {
            
            return (user.mode);
            break;
        }
    }
    return "N"; // Return -1 if user is not found Defined in RFC
}

string getModeChannel( vector<Channel> &channels, const string &name)
{
    for (const auto &channel : channels)
    {
        if (channel.channelName == name)
        {
            
            return (channel.modes);
            break;
        }
    }
    return "N"; // Return -1 if user is not found
}

int processModeCommand(const string message, const string nickname, string &username, string &realname, string &mode)
{
    // Check if the message starts with "MODE"
    string concat = toupper(message);
    if (concat.substr(0, 5) != "MODE ")
    {
        return ERR_NEEDMOREPARAMS;
    }

    size_t startPos = 5; // Skip "MODE "
    size_t nextPos;

    // Extract target nickname
    nextPos = message.find(' ', startPos);
    if (nextPos == string::npos)
        return ERR_NEEDMOREPARAMS;
    string targetNickname = message.substr(startPos, nextPos - startPos);

    vector<Channel> channels;
    parseChannels("channels.txt", channels);
    bool channelsExist = channelExists(targetNickname, channels);

    // Check if the target nickname matches the sender's nickname
    if (channelsExist)
    {
        startPos = nextPos + 1;
        string modes = message.substr(startPos);
        if (modes.empty())
            return ERR_NEEDMOREPARAMS;

        // Validate the mode string
        bool validMode = true;
        for (char c : modes)
        {
            if (c != '+' && c != '-' && c != 'a' && c != 's' && c != 'p')
            {
                validMode = false;
                break;
            }
        }

        if (!validMode)
        {
            return ERR_UMODEUNKNOWNFLAG;
        }
        vector<Channel> channels_new;
        parseChannels("channels.txt", channels_new);
        string newMode = mode; 
        bool channelsExist = false;
        for (auto &channel : channels_new)
        {
            if (channel.channelName == targetNickname)
            {
                applyModes(newMode, modes, true);
                channel.modes = newMode;

                break;
            }
        }
        writeChannelsToFile("channels.txt", channels_new);
        return 200;
    }


    else if (targetNickname == nickname)
    {
        // Extract modes
        startPos = nextPos + 1;
        string modes = message.substr(startPos);
        if (modes.empty())
            return ERR_NEEDMOREPARAMS;

        // Validate the mode string
        bool validMode = true;
        for (char c : modes)
        {
            if (c != '+' && c != '-' && c != 'i' && c != 'w' && c != 'a')
            {
                validMode = false;
                break;
            }
        }

        if (!validMode)
        {
            return ERR_UMODEUNKNOWNFLAG;
        }

        // Load users from the file
        vector<UserInfo> users = loadUsersFromFile("users.txt");
        string newMode = getMode(users, nickname );

        
        
        for (auto &user : users)
        {
            if (user.nickname == nickname)
            {
                string newMode = getMode(users, nickname );
                applyModes(newMode, modes, false);
                user.mode = newMode;
                break;
            }
        }

        saveUsersToFile("users.txt", users);

        return 200;
        
    }

    else if (targetNickname != nickname)
    {
        return ERR_USERSDONTMATCH;
    }

    else if (!channelsExist)
    {
        return 403;
    }
}

void deleteUserByNickname(vector<UserInfo> &users, const string &nickname)
{
    for (int i = 0; i < users.size(); i++)
    {
        if (users[i].nickname == nickname)
        {
            users[i].nickname = "";
            users[i].new_fd = "";
            users[i].ip = "";
            users[i].udpPort = 111111111;
        }
    }
}

string processQuitCommand(const string &message)
{
    // Find the quit message
    size_t quitMessagePos = message.find(':');
    string quitMessage = "Client has quit"; 
    if (quitMessagePos != string::npos)
    {
        quitMessage = message.substr(quitMessagePos + 1); // Extract the quit message
    }

    // Print the quit message (simulating sending an ERROR to the client)
    return quitMessage;

}
vector<string> findChannelsWithUser(const string &nickname, vector<Channel> &channels)
{
    vector<string> result;


    for (const auto &channel : channels)
    {
        if (find(channel.users.begin(), channel.users.end(), nickname) != channel.users.end())
        {
            result.push_back(channel.channelName); 
        }
    }

    return result;
}

bool isValidChannelName(const string& channelName) {
    // Define the regex pattern for a valid channel name
    regex channelNamePattern("^[&#+!][^ ,\\x07]{1,49}$");

    // Check if the channel name matches the pattern
    return regex_match(channelName, channelNamePattern);
}

int processJoinCommand(const string &params, vector<string> &channels, string &nickname)
{
  
    if (params.empty())
    {
        return 461; // ERR_NEEDMOREPARAMS: Missing parameters
    }

    // Check if the parameter is "0", indicating the user wants to leave all channels
    if (params == "0")
    {
        vector<Channel> Channel;
        parseChannels("channels.txt", Channel);
        vector<string> channelList = findChannelsWithUser(nickname, Channel);
        for (auto &channel : channelList)
        {
            int number = removeUserFromChannel(nickname, channel);
        }
        channels.clear(); // Clear the channel list
        return 0;         // Special reply code for leaving all channels
    }

  
    if (params.find(", ") != string::npos)
    {
        return 461; // ERR_NEEDMOREPARAMS: No spaces allowed after commas
    }

    istringstream paramStream(params);
    string channel;

    while (getline(paramStream, channel, ','))
    {
        channels.push_back(channel); // Add each channel to the vector
    }
    for (auto &channel: channels)
    {
         if(!isValidChannelName(channel))
         {
            return 476; 
         }
    }
   
    joinChannels(nickname, channels);

    return 1; // Success
}
int processPartCommand(const std::string &params, std::vector<std::string> &channels, std::string &nickname)
{
    string sendMessage;
    if (params.empty())
    {
        return 461; // Error: No parameters provided
    }

    size_t messagePos = params.find(" :");
    std::string channelParams;
    
    if (messagePos != std::string::npos)
    {
        // Extract channels and message
        channelParams = params.substr(0, messagePos);
        sendMessage = params.substr(messagePos + 2); // Store the part message
    }
    else
    {
        channelParams = params;
        sendMessage.clear(); // No message provided
    }

    // Check for invalid formatting of channels
    if (channelParams.find(", ") != std::string::npos)
    {
        return 461; // Error: Invalid formatting of channels
    }

    std::istringstream paramStream(channelParams);
    std::string channel;

    while (getline(paramStream, channel, ','))
    {
        channels.push_back(channel);
    }

    int replyCode = removeChannels(nickname, channels, sendMessage);

    return replyCode; // Success
}

void changeTopic(string channel, string topic)
{
    vector<Channel> Channel;
    parseChannels("channels.txt", Channel);
    for (auto &channel_loop : Channel)
    {
        if (channel_loop.channelName == channel)
        {
            if(channel_loop.modes != "s")
            {
            channel_loop.topic = topic;
            }
        }
    }
    writeChannelsToFile("channels.txt", Channel);
}

string viewTopic(string channel)
{
    string returnValue;
    vector<Channel> Channel;
    parseChannels("channels.txt", Channel);
    for (auto &channel_loop : Channel)
    {
        if (channel_loop.channelName == channel)
        {
            if(channel_loop.modes.find('s') == string::npos)
            {
            returnValue = channel_loop.topic;
            break;
            }
        }
    }
    return returnValue;
}
int processTopicCommand(const string &receivedMsg, string &view_topic, bool &viewTopicbool)
{

    string params = receivedMsg;
    // Extract parameters after "TOPIC "
    size_t colonPos = params.find(" :");

    string channel, topic;

    if (colonPos != string::npos)
    {
        channel = params.substr(0, colonPos); 
        topic = params.substr(colonPos + 2);  
    }
    else
    {
        // No colon means we only have the channel (view topic case)
        channel = params;
    }

    // Trim any whitespace from the channel name
    channel.erase(channel.find_last_not_of(" \n\r\t") + 1);
    vector<Channel> channels;
     parseChannels("channels.txt",channels);
    string getModeOfChannel = getModeChannel( channels, channel);

    // Handle the three cases
    if(getModeOfChannel.find("s") == string::npos)
    {
    if (!topic.empty())
    {
        // Case 1: Change the topic
        changeTopic(channel, topic);
        cout << "Changing topic for " << channel << " to: " << topic << endl;
    }
    else if (colonPos != string::npos)
    {
        // Case 2: Clear the topic
        
        cout << "Clearing the topic for " << channel << endl;
        changeTopic(channel, "");
    }
    else
    {
        // Case 3: View the topic
        
        cout << "Viewing the topic for " << channel << endl;
        view_topic = viewTopic(channel);
        viewTopicbool = true;
    
    }
    }

    else 
    {
        return 438;
    }

    return 001; // Success
}

void listFunction(string &description, int flag, vector<string> listOfChannel)
{
    vector<Channel> Channel;               
    parseChannels("channels.txt", Channel); 
    string replyValue = "\n";


    if (flag == 0)
    {
        for (auto &channel_loop : Channel)
        {
            if (channel_loop.modes.find('s') == string::npos) {
            replyValue += "322. RPL_LIST. The channel name is " + channel_loop.channelName +
                          ". The topic of the channel is: " + channel_loop.topic + ".\n";
            }
        }
    }

    // If flag is 1, list topics of specific channels in listOfChannel
    if (flag == 1)
    {
        for (auto &requested_channel : listOfChannel)
        {
            bool channelFound = false;
            for (auto &channel_loop : Channel)
            {
                // Find the requested channel in the list of all channels
                if (channel_loop.channelName == requested_channel)
                {
                    if(channel_loop.modes.find("s") == string::npos)
                    {

                    replyValue += "322. RPL_LIST. The channel name is " + channel_loop.channelName +
                                  ". The topic of the channel is: " + channel_loop.topic + ".\n";
                    channelFound = true;
                    break; // Exit the inner loop once the channel is found
                    }
                    else 
                    {
                        replyValue += "322. RPL_LIST. Couldn't find the channel.";
                    }

                }
            }
            // If the channel was not found, append the ERR_NOSUCHCHANNEL error
            if (!channelFound)
            {
                replyValue += "403. ERR_NOSUCHCHANNEL: The " + requested_channel + " does not exist.\n";
            }
        }
    }

    // Output or send the replyValue as needed
    description = replyValue;
}

int processListCommand(const string &receivedMsg, string &description)
{
    // Vector to hold the parsed channel names
    vector<string> listOfChannel;

    // Now, we need to parse the channels after "LIST" (if any)
    string concat = toupper(receivedMsg);
    if (receivedMsg.length() == 4 && concat.substr(0, 4) == "LIST")
    {
        // If the message is just "LIST", no channels specified
        listFunction(description, 0, listOfChannel); 
        return 323;                                  
    }
    else if (concat.substr(0, 5) == "LIST ")
    {
        // If there are more characters after "LIST", assume they are channels
        string channels = receivedMsg.substr(5); // Skip "LIST "

        // Clean up any potential trailing/leading spaces
        istringstream iss(channels);
        string trimmedChannels;
        getline(iss, trimmedChannels);

        // Split the trimmedChannels string by ',' and store in listOfChannel
        istringstream channelStream(trimmedChannels);
        string channel;
        while (getline(channelStream, channel, ','))
        {
            // Trim any spaces around channel names
            channel.erase(0, channel.find_first_not_of(" \t"));
            channel.erase(channel.find_last_not_of(" \t") + 1);

            if (!channel.empty())
            {
                listOfChannel.push_back(channel);
            }
        }

        // Call listFunction with the o
        listFunction(description, 1, listOfChannel);
        return 323; // Success, indicates LIST with specified channels
    }

    return -1; // Invalid command, return an error code
}

void namesFunction(string &description, int flag, vector<string> listOfChannel)
{
    vector<Channel> Channel;                
    parseChannels("channels.txt", Channel);
    vector<UserInfo> users = loadUsersFromFile("users.txt");

    vector<string> previousUsers;  // Stores usernames that have already been processed
    string replyValue = "\n";

    // If flag is 0, list all channels and their users
    if (flag == 0)
    {
        for (auto &channel_loop : Channel)
        {
            replyValue += "353. RPL_NAMREPLY. The channel name is " + channel_loop.channelName;
            
            if (channel_loop.modes.find('s') != string::npos)
            {
                replyValue += ". The channel is secret. Can't show anyone in this channel.";
            }
            else 
            {
                replyValue += ". The users are : ";

                for (auto &user : channel_loop.users)
                {
                    // Check if user is already in previousUsers
                    if (find(previousUsers.begin(), previousUsers.end(), user) == previousUsers.end())
                    {
                        // Get the mode of the user and check if they are not invisible
                        string getUserMode = getMode(users, user);
                        if (getUserMode.find('i') == string::npos)  // User is not invisible
                        {
                            replyValue += user + " ";
                            previousUsers.push_back(user);  // Add user to previousUsers
                        }
                    }
                }
            }
            replyValue += "\n";
        }

        // Check for users not in any channels but are visible
        string visibleUsersNotInChannels = "These users are not in any channels but are visible: ";
        bool foundVisibleUser = false;

        for (const auto &userInfo : users)
        {
            // Check if user is not in previousUsers and is visible
            if (find(previousUsers.begin(), previousUsers.end(), userInfo.nickname) == previousUsers.end())
            {
                string getUserMode = getMode(users, userInfo.nickname);
                if (getUserMode.find('i') == string::npos)  // User is not invisible
                {
                    visibleUsersNotInChannels += userInfo.nickname + " ";
                    foundVisibleUser = true;
                }
            }
        }

        if (foundVisibleUser)
        {
            replyValue += visibleUsersNotInChannels + "\n";
        }

        // Append end of names list message
        //replyValue += "366. RPL_ENDOFNAMES. End of NAMES list.\n";
    }

    // Set the description to the reply value

    else if (flag == 1)
    {
        for (auto &requested_channel : listOfChannel)
        {
            bool channelFound = false;
            for (auto &channel_loop : Channel)
            {
                // Find the requested channel in the list of all channels
                if (channel_loop.channelName == requested_channel)
                {
                    if(channel_loop.modes.find('s') == string::npos)
                    {
                    replyValue += "353. RPL_NAMEREPLY. The channel name is " + channel_loop.channelName +
                                  ". The users are: ";
                    for (auto &user : channel_loop.users)
                    {
                        string getUserMode = getMode(users, user);
                        if(getUserMode.find('i') == string::npos)
                        {
                            replyValue += user + " ";
                        }
                        
                    }
                    replyValue += "\n";
                    channelFound = true;
                    break; // Exit the inner loop once the channel is found
                    }

                    else 
                    {
                        replyValue += "The channel is secret. Can't show anyone in this channel.";
                    }

                }
            }
            // If the channel was not found, still return RPL_ENDOFNAMES (no error in NAMES command)
            if (!channelFound)
            {
                replyValue += "403. ERR_NOSUCHCHANNEL. The channel name " + requested_channel +
                              " does not exist.\n";
            }
        }
        // replyValue += "366. RPL_ENDOFNAMES. End of NAMES list.\n";
    }

    // Output or send the replyValue as needed
    description = replyValue;
}

int processNamesCommand(const string &receivedMsg, string &description)
{
    // Vector to hold the parsed channel names
    vector<string> listOfChannel;
    string concat = toupper(receivedMsg);

    // Now, we need to parse the channels after "NAMES" (if any)
    if (receivedMsg.length() == 5 && concat.substr(0, 5) == "NAMES")
    {
        // If the message is just "NAMES", no channels specified
        namesFunction(description, 0, listOfChannel); // Call names function with the untouched channelList
        return 323;                                   // Success, indicates NAMES for all channels
    }
    else if (concat.substr(0, 6) == "NAMES ")
    {
        // If there are more characters after "NAMES", assume they are channels
        string channels = receivedMsg.substr(6); // Skip "NAMES "

        // Clean up any potential trailing/leading spaces
        istringstream iss(channels);
        string trimmedChannels;
        getline(iss, trimmedChannels);

        // Split the trimmedChannels string by ',' and store in listOfChannel
        istringstream channelStream(trimmedChannels);
        string channel;
        while (getline(channelStream, channel, ','))
        {
            // Trim any spaces around channel names
            channel.erase(0, channel.find_first_not_of(" \t"));
            channel.erase(channel.find_last_not_of(" \t") + 1);

            if (!channel.empty())
            {
                listOfChannel.push_back(channel);
            }
        }

        // Call namesFunction with the channel list
        namesFunction(description, 1, listOfChannel);
        return 323; // Success, indicates NAMES with specified channels
    }

    return -1; // Invalid command, return an error code
}

int getFD(const vector<UserInfo> &users, const string &nickname)
{
    for (const auto &user : users)
    {
        if (user.nickname == nickname)
        {
            cout << user.new_fd;
            return stoi(user.new_fd);
            break;
        }
    }
    return -1; // Return -1 if user is not found
}



void udpBroadcast(int udpSockfd)
{
    while (true)
    {
        for (const auto &clientAddr : clientAddresses)
        {
            std::string message = "HELLO";
            cout << "From UDP socket: " << udpSockfd << endl;
            if (sendto(udpSockfd, message.c_str(), message.size(), 0,
                       (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == -1)
            {
                perror("server: sendto");
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}
void listenToPrivMsg() {
    vector<UserInfo> currentUser = loadUsersFromFile("users.txt");

    // Run this loop indefinitely to keep listening for messages
    while (true) {
        for (auto &user : currentUser) {
            char buf[MAXDATASIZE];
            int numbytes;

            // Try to receive data from the user's fd
            if ((numbytes = recv(stoi(user.new_fd), buf, MAXDATASIZE - 1, 0)) == -1) {
                perror("recv");
                continue;
            }

            if (numbytes > 0) {
                buf[numbytes] = '\0'; // Null-terminate the message
                string receivedMsg(buf);

                if (receivedMsg == "YES") {
                    cout << user.nickname << " has sent out a YES. Their FD is " << user.new_fd << endl;
                }
            }
        }

        // Add a small delay to avoid high CPU usage
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

vector<Messages>loadMessages() {
    string filename = "messages.txt";
    vector<Messages> queue;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Could not open file: " << filename << endl;
        return queue;
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string id, fd, msg;

        if (getline(iss, id, ';') && getline(iss, fd, ';') && getline(iss, msg)) {
            Messages message;
            message.messageId = stoi(id);
            message.recipient_fd = stoi(fd);
            message.message = msg;
            queue.push_back(message);
        }
    }

    file.close();
    return queue;
}

void printMessages(vector <Messages> messages) {
    for (const auto& msg : messages) {
        cout << "Message ID: " << msg.messageId 
             << ", Recipient FD: " << msg.recipient_fd 
             << ", Message: " << msg.message << endl;
    }
}

void takeCareOfMessage()
{
    while(true)
    {
    vector<Messages> messages = loadMessages();
    ofstream file("messages.txt", ios::trunc);
    file.close();
    if(messages.size() !=0)
    {
    for( auto& msg: messages)
    {
        string toSend = msg.message;
        if (send(msg.recipient_fd, toSend.c_str(), toSend.size(), 0) == -1)
        {
           perror("send");
        }
    }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

}
void appendToMessageDatabase(int value1, int value2, const string& text, const string& filename) {
    ofstream file(filename, ios::app);
    if (!file.is_open()) {
        cerr << "Could not open file: " << filename << endl;
        return;
    }
    file << value1 << ";" << value2 << ";" << text << endl;
    file.close();
}

bool doesUserExist(const string &sender, const string &recipient, const string &textToSend, int& newReplyCodeIfThingsExist)
{
  
    vector<UserInfo> users = loadUsersFromFile("users.txt");

    if (users.empty()) {
        cerr << "Error: No users loaded from file." << endl;
        return false;
    }

    // Loop through each user to find the recipient

    
    for (const auto &user : users)
    {
        if (user.nickname == recipient)
        {
            
            if (user.mode.find('a') != string::npos)
            {
                newReplyCodeIfThingsExist = 301; 
                return true; 
            }
            string toSend = "PRIVMSG " + sender + ": " + textToSend;

            try {
                int FD = stoi(user.new_fd);
                appendToMessageDatabase(1, FD,toSend,"messages.txt");
                newReplyCodeIfThingsExist = 1;
                return true; // Message was successfully sent
            }
            catch (const exception &e) {
                cerr << "Invalid file descriptor: " << user.new_fd << " - " << e.what() << endl;
                return false;
            }
         }
                        
        }
        newReplyCodeIfThingsExist = 401;
        return false; 
    }


bool doesUserExistInChannel(const Channel& channel, const string& sender) {
   cout << "Users in the channel:" << endl;
    for (const auto& user : channel.users) { // Adjust 'names' to match your actual struct
        if(user == sender)
        {
            return true; 
        }
    }
    return false; 
}

bool doesChannelExist(const string &sender, const string &recipient, const string &textToSend, int& newReplyCodeIfThingsExist)
{
    vector<UserInfo> users = loadUsersFromFile("users.txt"); // Load all users from file
    vector<Channel> channels;
    parseChannels("channels.txt", channels); // Load all channels from file
    string toSend;


    // Check if the channel exists
    if (channelExists(recipient, channels))
    {
        for (const auto &channel : channels)
        {
            if (channel.channelName == recipient)
            {
                //cout<<"The channel exists in this loop as: "<<channel.channelName<<endl;
                if(!doesUserExistInChannel(channel, sender))
                {
                  // cout<< "404. code happened here. "<<sender<<endl;
                    newReplyCodeIfThingsExist = 404; 
                    return true; 
                }

                if (channel.modes.find('a') != std::string::npos)
                {
                     toSend = "PRIVMSG anonymous!anonymous@anonymous :" + textToSend;
                }
                else 
                {
                toSend = "PRIVMSG " + sender + "from channel: " + recipient + ": " + textToSend;
                }
                for (const auto &userNickname : channel.users)
                {
                    string getModes = getMode(users,userNickname);
                    if(getModes.find('a') != string::npos)
                    {
                        int self_FD = getFD(users,sender);
                        string message = "301. RPL_AWAY. The recipient " + userNickname + " is away.";
                        appendToMessageDatabase(1,self_FD, message, "messages.txt");
                        continue; 
                    }
                    else 
                    {

                    int FD = getFD(users, userNickname); // Get FD of each user in the channel
                    appendToMessageDatabase(1,FD,toSend,"messages.txt");
                    }
                }
                newReplyCodeIfThingsExist = 1; 
                return true; // Return true after sending to all users in the channel
            }
        }
    }
    newReplyCodeIfThingsExist = 403; 
    return false; // Return false if the channel does not exist
}



int processPrivMsg(string &sendersName, std::string &message)
{
    // Find the position of the space after the PRIVMSG command
    size_t firstSpacePos = message.find(' ');
    if (firstSpacePos == std::string::npos)
    {
        std::cout << "Error: Invalid PRIVMSG format." << std::endl;
        return 461;
    }

    // Extract recipient
    size_t secondSpacePos = message.find(' ', firstSpacePos + 1);
    if (secondSpacePos == std::string::npos)
    {
        std::cout << "Error: Missing message text." << std::endl;
        return 461;
    }

    std::string recipient = message.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1);

    // Look for ':' to find the start of the actual message text
    size_t colonPos = message.find(':', secondSpacePos + 1);
    if (colonPos == std::string::npos)
    {
        std::cout << "Error: Missing message text after recipient." << std::endl;
        return 461;
    }

    // Extract the actual message content
   string finalMessage = message.substr(colonPos + 1);


    // Check if the recipient is a user or a channel
    bool isAChannel = false;
    bool isAUser = false;
    
    int newReplyCodeIfThingsExist;
    int codeForUser = 0; 

    bool idk;
     if(isValidChannelName(recipient))
        {
            isAChannel = true; 
            idk = doesChannelExist(sendersName, recipient, finalMessage, newReplyCodeIfThingsExist);
        }
    if(!idk)
       { 
      //  cout<<"This portion"<<endl;
        isAUser = true; 
        idk = doesUserExist(sendersName, recipient, finalMessage, newReplyCodeIfThingsExist);

        
       }
       
    return newReplyCodeIfThingsExist; 
    
}

void sendUdpPacket(int udpPort, const char * message, const std::string& IP) {
    // Create a UDP socket
    int udpSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSockfd == -1) {
        perror("socket");
        return;
    }

    // Set up the UDP address structure
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(udpPort);

    // Convert the IP address from string to binary format
    if (inet_pton(AF_INET, IP.c_str(), &udp_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(udpSockfd);
        return;
    }

    // Send the UDP packet
    if (sendto(udpSockfd, message, strlen(message), 0, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) == -1) {
        perror("sendto");
    } else {
        cout << "Sent UDP message to IP " << IP << " on port " << udpPort << endl;
    }

    // Close the socket
    close(udpSockfd);
}

void parseConfigFile(const string &filename, string &tcpPort, int &heartbeatInterval, int &statsInterval) {
    ifstream configFile(filename);
    if (!configFile.is_open()) {
        cerr << "Unable to open config file: " << filename << endl;
        return;
    }

    string line;
    while (getline(configFile, line)) {
        if (line.substr(0, 9) == "TCP_PORT=") {
            tcpPort = line.substr(9);
        } else if (line.substr(0, 19) == "HEARTBEAT_INTERVAL=") {
            heartbeatInterval = stoi(line.substr(19));
        } else if (line.substr(0, 15) == "STATS_INTERVAL=") {
            statsInterval = stoi(line.substr(15));
        }
    }
    
    configFile.close();
}

void sendHeartbeatToUsers(int heartbeat) {
   // cout<<"heartbeat in seconds: "<<heartbeat<<endl;
    while(true)
    {
    vector<UserInfo> users = loadUsersFromFile("users.txt");
     for (const auto &user : users) {
            if(user.udpPort != 111111111)
            {
            sendUdpPacket(user.udpPort, "Heartbeat", user.ip);
            }
        }
    this_thread::sleep_for(chrono::seconds(heartbeat));
    }
    }

void broadCast(int statsInterval) {
    while (true) {
        
        vector<UserInfo> users = loadUsersFromFile("users.txt");

  
        int userCount = 0;
        for (const auto &user : users) {
            if (!user.nickname.empty()) {
                userCount++;
            }
        }

    
        vector<Channel> channels;
        parseChannels("channels.txt", channels);

        // Count active channels (those with non-zero users)
        int channelCount = 0;
        for (const auto &channel : channels) {
            if (!channel.users.empty()) {
                channelCount++;
            }
        }

        // Get CPU usage
        double cpuUsage = 0.0;
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            cpuUsage = 100.0 * (1.0 - static_cast<double>(sys_info.freeram) / sys_info.totalram);
        }
        
        // Format stats message
       string message = "Server Stats - Total User Count: " + to_string(userCount) + 
                 ", Channel Count: " + to_string(channelCount) + 
                 ", CPU Usage: " + to_string(cpuUsage) + "%\n";
        

        // Broadcast stats to all users with valid UDP ports and IPs
        for (const auto &user : users) {
            if (!user.ip.empty()) {
                sendUdpPacket(user.udpPort, message.c_str(), user.ip);
            }
        }

        // Wait for the specified interval before broadcasting again
        this_thread::sleep_for(chrono::seconds(statsInterval));
    }

}
bool getUdpInfo(const string &nickname, int &udpPort, string &ip) {
    vector<UserInfo> users = loadUsersFromFile("users.txt");

    for (const UserInfo &user : users) {
        if (user.nickname == nickname) {
            udpPort = user.udpPort;
            ip = user.ip;
            return true;
        }
    }
    return false;  // Return false if nickname not found
}

void sendJoinMessage(const Channel &channel) {
    string message = "A new user has joined the channel " + channel.channelName;

    
    for (const string &nickname : channel.users) {
        int udpPort;
        string ip;

        if (getUdpInfo(nickname, udpPort, ip)) {
            sendUdpPacket(udpPort, message.c_str(), ip);
        } else {
            cout << "Failed to get UDP info for user: " << nickname << endl;
        }
    }
}
void partMessage(const std::string &channel, const std::string &nickname, const std::string &message)
{
    // Search for the matching channel by name
    vector<Channel> channels; 
    parseChannels("channels.txt", channels);
    for (const Channel &ch : channels)
    {
        if (ch.channelName == channel) // Match found
        {
            // Construct the part message
            std::string partMessage = "User " + nickname + " leaving channel " + ch.channelName + " with the message: " + message;

            // Loop through each user in the channel and send the part message
            for (const std::string &userNickname : ch.users)
            {
                int udpPort;
                std::string ip;
                if (getUdpInfo(userNickname, udpPort, ip))
                {
                    sendUdpPacket(udpPort, partMessage.c_str(), ip);
                }
                else
                {
                    std::cout << "Failed to get UDP info for user: " << userNickname << std::endl;
                }
            }
            break; // Exit loop after finding and processing the correct channel
        }
    }
}


int main(int argc, char *argv[])
{

    int sockfd, new_fd, udpSockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    std::vector<UserInfo> users = loadUsersFromFile("users.txt");
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::string configFileName = argv[1];

    string port;
    int heartbeatInterval;
    int statsInterval;

    parseConfigFile("server.conf", port, heartbeatInterval, statsInterval);


    if (port.empty())
    {
        std::cerr << "Port number not found in configuration file!" << std::endl;
        return 1;
    }

    if ((rv = getaddrinfo(nullptr, port.c_str(), &hints, &servinfo)) != 0)
    {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    for (p = servinfo; p != nullptr; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            std::perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "setsockopt");
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            std::perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr)
    {
        std::cerr << "server: failed to bind" << std::endl;
        return 1;
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "listen");
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "sigaction");
    }

    std::cout << "server: waiting for connections..." << std::endl;

    udpSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSockfd == -1)
    {
        perror("server: UDP socket");
        return 1;
    }

    while (true)
    {
     
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        clientNum = clientNum + 1; 
        if (new_fd == -1)
        {
            std::perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        // Log the connection
        logConnection(s);
        struct sockaddr_in *clientAddr = (struct sockaddr_in *)&their_addr;
        int clientPort = ntohs(clientAddr->sin_port);
        int udpPort = clientPort + 1000;
        struct in_addr clientIP = clientAddr->sin_addr; // Unique client IP address
        std::string clientIPStr = inet_ntoa(clientIP);

        if(!fork())
        {
        close(sockfd);
        sockaddr_in clientAddr;
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
        clientAddr.sin_addr = ((struct sockaddr_in *)&their_addr)->sin_addr;


            
            char buf[MAXDATASIZE];
            int numbytes;
            bool nickNameRegistered = false;
            bool userRegistered = false;
            string nickname, username, realname, mode, FD;

            while (true)
            {
                if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1)
                {
                    perror("recv");
                    exit(1);
                }
                else if (numbytes == 0)
                { // Client disconnected
                    if (userRegistered)
                    {
                        std::vector<UserInfo> users = loadUsersFromFile("users.txt");
                        deleteUserByNickname(users, nickname);
                        if (users.size() == 0)
                        {
                            remove("users.txt");
                        }
                        else
                        {
                            saveUsersToFile("users.txt", users);
                        }
                    }

                    logDisconnection(s);
                    break;
                }

                buf[numbytes] = '\0';
                std::string receivedMsg(buf);
                UserInfo userInfo;

                string commandConcat = toupper(receivedMsg);
                if (commandConcat.substr(0, 5) == "NICK ")
                {
                    string replyCode;
                    int result = processNickCommand(receivedMsg, nickname);

                    switch (result)
                    {
                    case 200:
                        replyCode = "200: Nickname registered. Use the USER command next to access the server.";
                        nickNameRegistered = true;
                        break;
                    case 431:
                        replyCode = "431: ERR_NONICKNAMEGIVEN: No nickname given.";
                        break;
                    case 432:
                        replyCode = "432: ERR_ERRONEUSNICKNAME: Erroneous nickname.";
                        break;
                    case 433:
                        replyCode = "433: ERR_NICKNAMEINUSE: Nickname is already in use.";
                        break;
                    default:
                        replyCode = "401: ERR_NOSUCHNICK: No such nickname exists.";
                        break;
                    }
                    // Testing block to test the new_fd issue
                    
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                else if (commandConcat.substr(0, 5) == "USER ")
                {
                    string replyCode;
                    int result;
                    if (nickNameRegistered)
                    {
                        FD = to_string(new_fd);
                        result = processUserCommand(receivedMsg, nickname, username, realname, mode, FD, udpPort, clientIPStr);
                    }
                    else
                    {
                        result = 431;
                    }
                    switch (result)
                    {
                    case 431:
                        replyCode = "431. No Nickname given. Use the NICK command before using the USER command";
                        break;
                    case RPL_WELCOME:
                        replyCode = "001. RPL_WELCOME. Welcome to the IRC. You have succesfully been registered.";
                        userRegistered = true;
                        clientNum = clientNum + 1; 
                        break;
                    case ERR_NEEDMOREPARAMS:
                        replyCode = "461. ERR_NEEDMOREPARAMS. Not enough parameters provided for command.";
                        break;
                    case ERR_ALREADYREGISTRED:
                        replyCode = "433. ERR_NICKNAMEINUSE. The user has already been registered to the system. And can access the server.";
                        break;
                    default:
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                else if (commandConcat.substr(0, 4) == "MODE")
                {
                    string replyCode;
                    int result;
                    if (nickNameRegistered && userRegistered)
                    {

                        result = processModeCommand(receivedMsg, nickname, username, realname, mode);
                    }
                    else
                    {
                        result = 431;
                    }
                    switch (result)
                    {
                    case 431:
                        replyCode = "431. Use the NICK and USER command before using the MODE command. Register yourself in the server before accessing other commands.";
                        break;
                    case ERR_UMODEUNKNOWNFLAG:
                        replyCode = "501. ERR_UMODEUNKNOWNFLAG. Unknown user mode flag.";
                        break;
                    case ERR_NEEDMOREPARAMS:
                        replyCode = "461. ERR_NEEDMOREPARAMS. Not enough parameters provided for command.";
                        break;
                    case ERR_USERSDONTMATCH:
                        replyCode = "502. ERR_USERSDONTMATCH. Cannot change mode for other users.";
                        break;
                    default:
                        break;
                    }

                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                else if (commandConcat.substr(0, 5) == "JOIN ")
                {
                    string replyCode;
                    int result;
                    if (nickNameRegistered && userRegistered)
                    {
                        vector<string> channelNames;
                        result = processJoinCommand(receivedMsg.substr(5), channelNames, nickname);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 461:
                        replyCode = "461. ERR_NEEDMOREPARAMS.";
                        break;
                    case 0:
                        replyCode = "000. Got out of all the channels.";
                        break;
                    case 1:
                        replyCode = "001. Succesfully joined the channels.";
                        break;
                    case 476:
                        replyCode = "ERR_BADCHANMASK. Provide the proper starting channel mask.";
                        break; 
                    default:
                        replyCode = "431.  Register yourself in the server before accessing other commands.";
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }
                else if (commandConcat.substr(0, 5) == "PART ")
                {
                    string replyCode;
                    int result;
                    if (nickNameRegistered && userRegistered)
                    {
                        vector<string> channelNames;
                        result = processPartCommand(receivedMsg.substr(5), channelNames, nickname);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 461:
                        replyCode = "461. ERR_NEEDMOREPARAMS. Parameters";
                        break;
                    case 403:
                        replyCode = "403. Atleast one of the channels provided doesn't exist.";
                        break;
                    case 1:
                        replyCode = "001. Succesfully parted away from all the channels.";
                        break;
                    case 404:
                        replyCode = "404. User not Found in this Channel.";
                    default:
                        replyCode = "431.  Register yourself in the channel before accessing other commands.";
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }
                else if (commandConcat.substr(0, 6) == "TOPIC ")
                {
                    string replyCode;
                    int result;
                    string topic;
                    bool viewTopic = false;
                    if (nickNameRegistered && userRegistered)
                    {

                        result = processTopicCommand(receivedMsg.substr(6), topic, viewTopic);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 461:
                        replyCode = "461. ERR_NEEDMOREPARAMS. Parameters";
                        break;
                    case 1:
                        replyCode = "332.";
                        if (viewTopic)
                        {
                            replyCode = replyCode + " Topic of the channel is: " + topic;
                        }
                        else
                        {
                            replyCode = replyCode + "Succesfully set the topic of the channel.";
                        }

                        break;
                    case 438:
                        replyCode = "403. No such channel exists. ";
                        break;
                    default:
                        replyCode = "431. Use the NICK and USER command before using this command. Register yourself in the server before accessing other commands.";
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                else if (commandConcat.substr(0, 4) == "LIST")
                {
                    string replyCode;
                    int result;
                    string description;
                    if (nickNameRegistered && userRegistered)
                    {

                        result = processListCommand(receivedMsg, description);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 323:
                        replyCode = description + "323. RPL_LISTEND";
                        break;
                    case 431:
                        replyCode = "431. Use the NICK and USER command before using the MODE command. Register yourself in the server before accessing other commands.";
                        break;
                    default:
                        replyCode = "404. Command not in proper format";
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                else if (commandConcat.substr(0, 5) == "NAMES")
                {
                    string replyCode;
                    int result;
                    string description;
                    if (nickNameRegistered && userRegistered)
                    {

                        result = processNamesCommand(receivedMsg, description);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 323:
                        replyCode = description += "366. RPL_ENDOFNAMES. End of NAMES list.\n";
                        break;
                    case 431:
                        replyCode = "431. Use the NICK and USER command before using this command. Register yourself in the server before accessing other commands.";
                        break;
                    default:
                        replyCode = "404. Command not in proper format";
                        break;
                    }
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }               
                else if (commandConcat.substr(0, 7) == "PRIVMSG")
                {
                    string replyCode;
                    int result;
                    string description;
                    string finalMessage;
                    int recipientFD;
                    if (nickNameRegistered && userRegistered)
                    {

                        result = processPrivMsg(nickname, receivedMsg);
                    }
                    else
                    {
                        result = 431;
                    }

                    switch (result)
                    {
                    case 1:
                        replyCode = "001. Msg sent succesfully";
                        break;
                    case 431:
                        replyCode = "431. Use the NICK and USER command before using this command. Register yourself in the server before accessing other commands.";
                        break;
                    case 461:
                        replyCode = "461. ERR_NEEDMOREPARAMS. Need more parameters.";
                        break;
                    case 401:
                        replyCode = "401. ERR_NOSUCHNICK. Receipent doesn't exist. ";
                        break;
                    case 403:
                        replyCode = "403. ERR_NOSUCHCHANNEL. The given channel does not exists";
                        break; 
                    case 404:
                        replyCode = "404. Cannot send message to channel.";
                        break;
                    case 301:
                        replyCode = "301. RPL_AWAY. The user is away.";
                        break;
                    default: 
                        replyCode = "404. Dont know whats happening.";
                        break;
                    }
                    
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }

                    
                else if (commandConcat.substr(0, 4) == "QUIT" || commandConcat.substr(0, 5) == "QUIT ")
                {

                    string quitMessage = processQuitCommand(receivedMsg);
                    if (userRegistered)
                    {
                        std::vector<UserInfo> users = loadUsersFromFile("users.txt");
                        deleteUserByNickname(users, nickname);
                        if (users.size() == 0)
                        {
                            remove("users.txt");
                        }
                        else
                        {
                            saveUsersToFile("users.txt", users);
                        }
                    }
                    logDisconnection(s);
                    close(new_fd);
                    exit(0);
                }

                else
                {
                    string replyCode = "404. Command Not Found";
                    if (send(new_fd, replyCode.c_str(), replyCode.size(), 0) == -1)
                    {
                        perror("send");
                    }
                }
            }

           
           logDisconnection(s);
           close(new_fd);
           exit(0);
        
    }



    
   
    else 
    {
    // cout<<"This shit"<<endl; 
    if(clientNum == 1)
     {
        ofstream file("messages.txt");
        file.close();

        thread thread1(takeCareOfMessage);
        thread1.detach();

        ofstream file2("users.txt");
        file2.close();

        std::thread([heartbeatInterval]() {
        sendHeartbeatToUsers(heartbeatInterval);
       }).detach();

       std::thread([statsInterval]() {
        broadCast(statsInterval);
       }).detach();
      
     }
    }
    }
    return 0;
}
