#include <iostream>
#include <fstream>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <semaphore>
#include <mutex>
#include <semaphore.h>

using namespace std;

// mutex UserInfoMap_Mutex;
sem_t UserInfoMap_Mutex;
sem_t GroupInfoMap_Mutex;
sem_t FileInfoMap_Mutex;

struct Chunk
{
    int ChunkNo;
    string PeerName;
    string GroupName;
    string FilePathInPeer;
    string SHA1;
};

class UserInfo
{
public:
    string UserName;
    string Password;
    string IP;
    vector<string> GroupOwnership;
    int PortNo;
    bool IsAlive;

    void CreateUser(string p_Uname, string p_Pwd)
    {
        UserName = p_Uname;
        Password = p_Pwd;
        IsAlive = false;
    }
};

class GroupInfo
{
public:
    string GroupName;
    string OwnerUserName;
    vector<string> FileList;
    vector<string> UsersList;
    vector<string> JoinRequestList;

    vector<pair<string, string>> SharableFilesList;

    void CreateGroup(string p_GroupName, string p_Owner)
    {
        GroupName = p_GroupName;
        OwnerUserName = p_Owner;
    }
};

class FileInfo
{
public:
    string FileName;
    string FileSize;
    // string FilePath;
    string FileSHA1;
    int NumberOfChunks;
    vector<Chunk> ChunkInfoList;
};

class tracker
{
public:
    string ServerIP;
    int ServerPort;
    int ListenSockID;

    unordered_map<string, UserInfo> UsersInfoMap;
    unordered_map<string, GroupInfo> GroupInfoMap;
    unordered_map<string, FileInfo> FileInfoMap;

    tracker()
    {
        sem_init(&UserInfoMap_Mutex, 0, 1);
        sem_init(&GroupInfoMap_Mutex, 0, 1);
        sem_init(&FileInfoMap_Mutex, 0, 1);
    }

    ~tracker()
    {
        sem_destroy(&UserInfoMap_Mutex);
        sem_destroy(&GroupInfoMap_Mutex);
        sem_destroy(&FileInfoMap_Mutex);
    }

    void ParseArguments(string FilePath, int TrackerNumber)
    {
        ifstream fs;
        string line;
        string word;
        int trackerIndex = 0;
        vector<string> input;

        fs.open(FilePath);
        if (fs)
        {
            cerr << "Tracker Info file Opened Successfully" << endl;

            while (getline(fs, line))
            {
                istringstream iss(line);
                while (iss >> word)
                {
                    input.emplace_back(word);
                }
            }
            if (input.size() != 6)
            {
                cout << "Incorrect Tracker Info file" << endl;
                cerr << "Incorrect Tracker Info file. Number of Tokens in file:" << input.size() << endl;
                for (int i = 0; i < input.size(); i++)
                {
                    cerr << input[i] << endl;
                }
                exit(1);
            }
            if (stoi(input[trackerIndex]) != TrackerNumber)
            {
                trackerIndex = 3;
            }
            ServerIP = input[trackerIndex + 1];
            ServerPort = stoi(input[trackerIndex + 2]);

            cerr << "Server IP and Port recorded" << endl;
        }
    }

    void CreateSocketAndBind()
    {
        struct sockaddr_in trackerAdress;
        int opt;

        ListenSockID = socket(AF_INET, SOCK_STREAM, 0);
        if (ListenSockID < 0)
        {
            // error("ERROR opening socket");
            cout << "ERROR opening socket" << endl;
            cerr << "ERROR opening socket" << endl;
            exit(1);
        }
        cerr << "Socket Created" << endl;

        if (setsockopt(ListenSockID, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        bzero((char *)&trackerAdress, sizeof(trackerAdress));
        trackerAdress.sin_family = AF_INET;
        trackerAdress.sin_port = htons(ServerPort);
        if (inet_pton(AF_INET, ServerIP.c_str(), &trackerAdress.sin_addr) <= 0)
        {
            cout << "Invalid Host provided" << endl;
            cerr << "Invalid Host provided" << endl;
            exit(1);
        }

        if (bind(ListenSockID, (struct sockaddr *)&trackerAdress, sizeof(trackerAdress)) < 0)
        {
            // error("ERROR on binding");
            cout << "ERROR on binding" << endl;
            cerr << "ERROR on binding" << endl;
            exit(1);
        }
        cerr << "Socket Bind done" << endl;

        listen(ListenSockID, INT_MAX);
    }

    void ListenForClientRequests()
    {
        struct sockaddr_in clientAddress;
        int newSockFD;
        socklen_t clientLen;
        clientLen = sizeof(clientAddress);
        while (1)
        {
            cout << "Listening for Client Request" << endl;
            cerr << "Listening for Client Request" << endl;
            // newSockFD = accept(p_sockFD, (struct sockaddr*) &clientAddress, &clientLen);
            newSockFD = accept(ListenSockID, (struct sockaddr *)&clientAddress, &clientLen);
            if (newSockFD < 0)
            {
                cerr << "ERROR on accept" << endl;
                continue;
            }
            cout << "Server: Received connection from IP: " << inet_ntoa(clientAddress.sin_addr) << " PORT: " << ntohs(clientAddress.sin_port) << endl;
            cerr << "Server: Received connection from IP: " << inet_ntoa(clientAddress.sin_addr) << " PORT: " << ntohs(clientAddress.sin_port) << endl;
            // cout << "sockFD:" << ListenSockID << ",newSockFD:" << newSockFD << endl;

            thread t(&tracker::RespondToClient, this, newSockFD);
            // thread th1 = ListenerThread();
            // thread t(RespondToClient, i);

            t.detach();
        }
    }

    // thread ListenerThread()
    // {
    //     return std::thread([=]
    //                        { ListenForClientRequests(); });
    // }

    void RespondToClient(int p_newSockFD)
    {
        string ClientUserName = "";
        string msgToClient;
        vector<string> msgFromClient;
        int n;
        char buffer[BUFSIZ];
        int dataBuffSize = 8192;
        char data[dataBuffSize];
        // FILE *fp;
        int fp;
        char ch;
        int nBytes;
        int sentBytes;

        // cout << "Respond to client Triggered" << endl;
        cerr << "Responding to client" << endl;

        msgToClient = "Hello Client! Welcome to the world of Peer-To-Peer File Sharing\n";
        send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

        while (true)
        {
            msgFromClient.clear();
            bzero(buffer, BUFSIZ);

            n = read(p_newSockFD, buffer, BUFSIZ);
            cerr << "Message received from Client:" << buffer << endl;
            if (n < 0)
            {
                // cout << "ERROR reading from socket" << endl;
                cerr << "ERROR reading from socket" << endl;
                break;
            }

            ParseClientRequest(buffer, msgFromClient);

            cerr << "Parsed message. Number of Words=" << msgFromClient.size() << endl;
            if (msgFromClient.size() == 0)
            {
                continue;
            }
            else if (msgFromClient[0] == "create_user")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 3, p_newSockFD))
                {
                    continue;
                }

                msgToClient = RegisterUser(msgFromClient[1], msgFromClient[2]);
                send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);
                cerr << msgToClient << endl;

                SendStopSignal(p_newSockFD);

                // break;
            }
            else if (msgFromClient[0] == "login")
            {
                cerr << "login triggered" << endl;
                if (!ValidateCommandArguments(msgFromClient.size(), 5, p_newSockFD))
                {
                    continue;
                }

                ClientUserName = msgFromClient[1];
                msgToClient = Login(msgFromClient[1], msgFromClient[2], msgFromClient[3], stoi(msgFromClient[4]));
                cerr << msgToClient << endl;
                send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                SendStopSignal(p_newSockFD);
                // msgToClient = "";
                // send(p_newSockFD, msgToClient.c_str(), 0, 0);

                // if (msgToClient != "Login Successful")
                // {
                //     break;
                // }
            }
            else if (msgFromClient[0] == "create_group")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 2, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = CreateGroup(ClientUserName, msgFromClient[1]);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "join_group")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 2, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = JoinGroupRequest(ClientUserName, msgFromClient[1]);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "leave_group")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 2, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = LeaveGroup(ClientUserName, msgFromClient[1]);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "list_requests")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 2, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    ListGroupJoinRequests(ClientUserName, msgFromClient[1], p_newSockFD);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "accept_request")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 3, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = AcceptJoinGroupRequest(ClientUserName, msgFromClient[1], msgFromClient[2]);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "list_groups")
            {
                if (ClientUserName.length() > 0)
                {
                    ListGroups(p_newSockFD);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "list_files")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 2, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    ListFilesInGroup(ClientUserName, msgFromClient[1], p_newSockFD);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "upload_file")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 3, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = UploadFile(ClientUserName, msgFromClient[1], msgFromClient[2], p_newSockFD);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            // Custom command (Not from requirement)
            else if (msgFromClient[0] == "upload_chunk")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 7, p_newSockFD))
                {
                    continue;
                }

                ChunkUpload(msgFromClient[1], msgFromClient[2], msgFromClient[3], stoi(msgFromClient[4]), msgFromClient[5], msgFromClient[6]);
                // send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0); // No need of update for UploadChunk request

                // SendStopSignal(p_newSockFD);

                // break;
            }
            else if (msgFromClient[0] == "download_file")
            {
                cerr << "Inside download_file function" << endl;
                if (!ValidateCommandArguments(msgFromClient.size(), 3, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    DownloadTorrent(ClientUserName, msgFromClient[1], msgFromClient[2], p_newSockFD);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "logout")
            {
                if (ClientUserName.length() > 0)
                {
                    msgToClient = Logout(ClientUserName);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);

                    ClientUserName = "";
                }

                // break;
            }
            // else if (msgFromClient[0] == "show_downloads")
            // {
            // }
            else if (msgFromClient[0] == "stop_share")
            {
                if (!ValidateCommandArguments(msgFromClient.size(), 3, p_newSockFD))
                {
                    continue;
                }

                if (ClientUserName.length() > 0)
                {
                    msgToClient = StopSharing(ClientUserName, msgFromClient[1], msgFromClient[2]);
                    cerr << msgToClient << endl;
                    send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                    SendStopSignal(p_newSockFD);
                }
            }
            else if (msgFromClient[0] == "quit")
            {
                cerr << "Closing connection of client" << endl;
                break;
            }
            else
            {
                msgToClient = "Does not match list of commands acceptable";
                cerr << msgToClient << endl;
                send(p_newSockFD, msgToClient.c_str(), msgToClient.size(), 0);

                SendStopSignal(p_newSockFD);
            }
            memset(buffer, 0, BUFSIZ);
        }

        close(p_newSockFD);
        cerr << "Client Socket Closed" << endl;

        // cout << "Transfering file" << endl;
        // cerr << "Transfering file" << endl;
        // // sendFile(p_newSockFD);
        // cout << "File Tranfer completed" << endl;
        // cerr << "File Tranfer completed" << endl;

        // bzero(buffer, 255);
        // n = read(p_newSockFD, buffer, 256);
        // if (n < 0)
        // {
        //     cout << "ERROR reading from socket" << endl;
        // }
        // cout << "Here is the message from client: " << buffer << endl;
        // // log = "Here is the message from client: " + buffer.to;
        // cerr << "Here is the message from client: " << buffer << endl;

        // cerr << "Client requested Handled succesfully" << endl;

        // close(p_newSockFD);
    }

    void ParseClientRequest(char *buffer, vector<string> &p_MsgFromClient)
    {
        string word;
        istringstream iss(buffer);
        while (iss >> word)
        {
            p_MsgFromClient.emplace_back(word);
        }
    }

    string RegisterUser(string Uname, string Pwd)
    {
        sem_wait(&UserInfoMap_Mutex);

        string response;
        auto iter = UsersInfoMap.find(Uname);
        if (iter != UsersInfoMap.end())
        {
            response = "Username already used";
        }
        else
        {
            UserInfo usr;
            usr.CreateUser(Uname, Pwd);
            UsersInfoMap.insert({Uname, usr});

            response = "User registered successfully";
        }

        sem_post(&UserInfoMap_Mutex);

        return response;
    }

    string Login(string Uname, string Pwd, string IP, int PortNo)
    {
        sem_wait(&UserInfoMap_Mutex);

        string response;
        auto iter = UsersInfoMap.find(Uname);
        if (iter == UsersInfoMap.end())
        {
            response = "User not Registered";
        }
        else
        {
            UserInfo *usr = &UsersInfoMap.at(Uname);
            if (usr->Password != Pwd)
            {
                response = "Incorrect Password";
            }
            else
            {
                usr->IP = IP;
                usr->PortNo = PortNo;
                usr->IsAlive = true;
                response = "Login Successful";
            }
        }

        sem_post(&UserInfoMap_Mutex);

        return response;
    }

    string CreateGroup(string Uname, string GroupName)
    {
        sem_wait(&GroupInfoMap_Mutex);

        string response;
        auto iter = GroupInfoMap.find(GroupName);
        if (iter != GroupInfoMap.end())
        {
            response = "Group Name already exists. Please choose a different name";
        }
        else
        {
            GroupInfo grp;
            grp.CreateGroup(GroupName, Uname);
            grp.UsersList.emplace_back(Uname);
            GroupInfoMap.insert({GroupName, grp});

            UserInfo *usr = &UsersInfoMap.at(Uname);
            usr->GroupOwnership.emplace_back(GroupName);

            response = "Group created successfully";
        }

        sem_post(&GroupInfoMap_Mutex);

        return response;
    }

    string JoinGroupRequest(string Uname, string GroupName)
    {
        bool UserPartOfGroup = false;
        sem_wait(&GroupInfoMap_Mutex);
        sem_wait(&UserInfoMap_Mutex);

        string response;
        auto iter = GroupInfoMap.find(GroupName);
        if (iter == GroupInfoMap.end())
        {
            response = "Group does not exists";
        }
        else
        {
            GroupInfo *grp = &GroupInfoMap.at(GroupName);

            for (int i = 0; i < grp->UsersList.size(); i++)
            {
                if (grp->UsersList[i] == Uname)
                {
                    response = "User already part of the group";
                    UserPartOfGroup = true;
                }
            }

            if (!UserPartOfGroup)
            {
                grp->JoinRequestList.emplace_back(Uname);
                response = "Group Join request registered successfully";
            }
        }

        sem_post(&UserInfoMap_Mutex);
        sem_post(&GroupInfoMap_Mutex);

        return response;
    }

    void ListGroupJoinRequests(string Uname, string GroupName, int p_newSockFD)
    {
        bool msgSent = false;
        sem_wait(&GroupInfoMap_Mutex);
        sem_wait(&UserInfoMap_Mutex);

        cerr << "ListGroupJoinRequests function triggered" << endl;
        string response;
        bool IsOwner = false;

        UserInfo usr = UsersInfoMap.at(Uname);
        if (usr.GroupOwnership.size() == 0)
        {
            response = "No Groups under the selected user";
            send(p_newSockFD, response.c_str(), response.size(), 0);

            msgSent = true;
        }
        for (int i = 0; i < usr.GroupOwnership.size(); i++)
        {
            if (usr.GroupOwnership[i] == GroupName)
            {
                GroupInfo grp = GroupInfoMap.at(GroupName);
                cerr << "Fetched Group Info" << endl;

                cerr << "Number of Join Requests:" << grp.JoinRequestList.size() << endl;

                if (grp.JoinRequestList.size() == 0)
                {
                    response = "No Pending Join reuests";
                    send(p_newSockFD, response.c_str(), response.size(), 0);
                }

                for (int j = 0; j < grp.JoinRequestList.size(); j++)
                {
                    cerr << "j:" << j << endl;
                    cerr << "contion:" << grp.JoinRequestList.size() - 1 << endl;
                    response = grp.JoinRequestList[j];
                    send(p_newSockFD, response.c_str(), response.size(), 0);

                    usleep(100000);
                }
                msgSent = true;

                break;
            }
        }

        if (msgSent == false)
        {
            response = "Unauthorized request";
            cerr << response << endl;
            send(p_newSockFD, response.c_str(), response.size(), 0);
        }

        response = "Group -" + GroupName + " join requests succesfully sent to client";

        sem_post(&UserInfoMap_Mutex);
        sem_post(&GroupInfoMap_Mutex);

        cerr << response;
    }

    string AcceptJoinGroupRequest(string ClientUName, string GroupName, string acceptUName)
    {
        sem_wait(&GroupInfoMap_Mutex);
        sem_wait(&UserInfoMap_Mutex);

        string response;
        bool IsOwner = false;
        int UserIndex = -1;

        auto iter = UsersInfoMap.find(acceptUName);
        if (iter == UsersInfoMap.end())
        {
            response = "Given user is not registered";
        }
        else
        {
            UserInfo usr = UsersInfoMap.at(ClientUName);

            if (usr.GroupOwnership.size() == 0)
            {
                response = "No Groups under the Logged In user to process to request";
            }
            else
            {
                for (int i = 0; i < usr.GroupOwnership.size(); i++)
                {
                    if (usr.GroupOwnership[i] == GroupName)
                    {
                        IsOwner = true;
                        GroupInfo *grp = &GroupInfoMap.at(GroupName);

                        for (int j = 0; j < grp->JoinRequestList.size(); j++)
                        {
                            if (grp->JoinRequestList[j] == acceptUName)
                            {
                                UserIndex = j;
                                break;
                            }
                        }
                        if (UserIndex != -1)
                        {
                            auto iter = grp->JoinRequestList.begin() + UserIndex;
                            grp->JoinRequestList.erase(iter);
                            grp->UsersList.emplace_back(acceptUName);
                            response = "User added to the group";
                        }
                        else
                        {
                            response = "User does not have privileges to add user to a group";
                        }

                        break;
                    }
                }

                if (IsOwner == false)
                {
                    response = "Unauthorized request";
                }
            }
        }

        sem_post(&UserInfoMap_Mutex);
        sem_post(&GroupInfoMap_Mutex);

        return response;
    }

    void ListGroups(int p_NewSockFD)
    {
        sem_wait(&GroupInfoMap_Mutex);

        string response;
        for (auto i : GroupInfoMap)
        {
            response = i.first;
            send(p_NewSockFD, response.c_str(), response.size(), 0);

            usleep(100000);
        }

        if (GroupInfoMap.size() == 0)
        {
            response = "No Groups created";
            send(p_NewSockFD, response.c_str(), response.size(), 0);
        }

        sem_post(&GroupInfoMap_Mutex);

        response = "Sent Group List to Client";

        cerr << response;
    }

    void ListFilesInGroup(string Uname, string GroupName, int p_NewSockFD)
    {
        sem_wait(&GroupInfoMap_Mutex);
        sem_wait(&FileInfoMap_Mutex);
        string response;
        bool UserPartOfGroup = false;
        bool FilesExists = false;

        auto iter = GroupInfoMap.find(GroupName);
        if (iter == GroupInfoMap.end())
        {
            response = "No such group exists";
            send(p_NewSockFD, response.c_str(), response.size(), 0);
        }
        else
        {
            GroupInfo grp = GroupInfoMap.at(GroupName);

            for (int i = 0; i < grp.UsersList.size(); i++)
            {
                if (grp.UsersList[i] == Uname)
                {
                    UserPartOfGroup = true;
                    break;
                }
            }
            if (UserPartOfGroup == false)
            {
                response = "User not part of the group";
                send(p_NewSockFD, response.c_str(), response.size(), 0);
            }
            else
            {
                if (grp.SharableFilesList.size() == 0)
                {
                    response = "No Files in the group";
                    send(p_NewSockFD, response.c_str(), response.size(), 0);
                }
                else
                {
                    unordered_set<string> fileList;
                    for (int i = 0; i < grp.SharableFilesList.size(); i++)
                    {
                        // Check if User is Logged In
                        UserInfo usr = UsersInfoMap.at(grp.SharableFilesList[i].second);

                        if (usr.IsAlive == true)
                        {
                            fileList.insert(grp.SharableFilesList[i].first);
                            FilesExists = true;
                        }
                    }

                    for (auto i = fileList.begin(); i != fileList.end(); i++)
                    {
                        response = *i;
                        send(p_NewSockFD, response.c_str(), response.size(), 0);

                        usleep(100000);
                    }
                    if (FilesExists == false)
                    {
                        response = "No Files in the group";
                        send(p_NewSockFD, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        response = "File list within group sent to client"; // For Log
                    }
                }

                // if (grp.FileList.size() == 0)
                // {
                //     response = "No Files in the group";
                //     send(p_NewSockFD, response.c_str(), response.size(), 0);
                // }
                // else
                // {
                //     for (int i = 0; i < grp.FileList.size(); i++)
                //     {
                //         response = grp.FileList[i];
                //         send(p_NewSockFD, response.c_str(), response.size(), 0);

                //         usleep(100000);
                //     }
                //     response = "File list within group sent to client";
                // }
            }
        }

        sem_post(&FileInfoMap_Mutex);
        sem_post(&GroupInfoMap_Mutex);

        cerr << response;
    }

    string UploadFile(string Uname, string FileName, string GroupName, int p_NewSockFD)
    {
        string response;

        vector<string> msgFromClient;
        int NumOfChunks;

        bool UserPartOfGrp = false;

        cerr << "Upload File Triggered" << endl;

        sem_wait(&GroupInfoMap_Mutex);
        // cerr << "Passed Semaphore Lock" << endl;
        GroupInfo *grp = &GroupInfoMap.at(GroupName);
        for (int i = 0; i < grp->UsersList.size(); i++)
        {
            if (Uname == grp->UsersList[i])
            {
                UserPartOfGrp = true;
                break;
            }
        }
        cerr << "User Validation done" << endl;
        sem_post(&GroupInfoMap_Mutex);

        if (UserPartOfGrp)
        {
            FileInfo FInfo;
            FInfo.FileName = FileName;

            cerr << "File Upload:" << FileName << endl;

            // Fetch File Size
            FetchSocketMsgFromClient(p_NewSockFD, msgFromClient);
            FInfo.FileSize = msgFromClient[0];

            cerr << "File Size:" << msgFromClient[0] << endl;

            // // Fetch SHA1
            // FetchSocketMsgFromClient(p_NewSockFD, msgFromClient);
            // FInfo.FileSHA1 = msgFromClient[0];

            // cerr << "File SHA1:" << msgFromClient[0] << endl;

            // Fetch Number of Chunks
            FetchSocketMsgFromClient(p_NewSockFD, msgFromClient);
            NumOfChunks = stoi(msgFromClient[0]);
            FInfo.NumberOfChunks = NumOfChunks;

            cerr << "Num of Chunks:" << NumOfChunks << endl;

            // for (int i = 0; i <= NumOfChunks - 1; i++)
            while (true)
            {
                // Fetch Chunk Info
                if (FetchSocketMsgFromClient(p_NewSockFD, msgFromClient))
                {
                    Chunk chk;
                    chk.ChunkNo = stoi(msgFromClient[0]);
                    cerr << "Chunk No:" << stoi(msgFromClient[0]) << endl;

                    chk.FilePathInPeer = msgFromClient[1];
                    cerr << "File Path:" << msgFromClient[1] << endl;

                    chk.SHA1 = msgFromClient[2];
                    cerr << "Chunk SHA1:" << msgFromClient[2] << endl;

                    chk.PeerName = Uname;
                    cerr << "Peer Name:" << Uname << endl;

                    chk.GroupName = GroupName;
                    cerr << "Group Name:" << GroupName << endl;

                    FInfo.ChunkInfoList.emplace_back(chk);
                }
                else
                {
                    break;
                }
            }

            sem_wait(&FileInfoMap_Mutex);

            FileInfoMap.insert({FileName, FInfo});
            cerr << "Added File to FileInfoMap";

            sem_post(&FileInfoMap_Mutex);

            sem_wait(&GroupInfoMap_Mutex);

            grp->FileList.emplace_back(FileName);
            grp->SharableFilesList.emplace_back(pair(FileName, Uname));
            cerr << "Added File to Groupp";

            sem_post(&GroupInfoMap_Mutex);

            response = "File details uploaded succesfully";
        }
        else
        {
            response = "User is not part of Group. Cannot upload file";
        }

        return response;
    }

    void ChunkUpload(string FileName, string UserName, string GroupName, int ChunkNo, string FilePath, string SHA1)
    {
        sem_wait(&FileInfoMap_Mutex);

        string response;

        FileInfo *FInfo = &FileInfoMap.at(FileName);
        Chunk chk;
        chk.ChunkNo = ChunkNo;
        // cerr << "Chunk No:" << ChunkNo << endl;

        chk.FilePathInPeer = FilePath;
        // cerr << "File Path" << FilePath << endl;

        chk.SHA1 = SHA1;
        // cerr << "Chunk SHA1:" << SHA1 << endl;

        chk.PeerName = UserName;
        // cerr << "Peer Name:" << UserName << endl;

        chk.GroupName = GroupName;
        // cerr << "Group Name:" << GroupName << endl;

        FInfo->ChunkInfoList.emplace_back(chk);

        sem_post(&FileInfoMap_Mutex);

        response = "Chunk details uploaded";

        cerr << "Chunk " << ChunkNo << ": details uploaded";
    }

    void DownloadTorrent(string Uname, string GroupName, string FileName, int p_NewSockFD)
    {
        string response;
        vector<string> msgFromClient;
        bool UserPartOfGrp = false;
        bool FilePartOfGrp = false;

        cerr << "Download Torrent Triggered" << endl;

        sem_wait(&UserInfoMap_Mutex);
        sem_wait(&GroupInfoMap_Mutex);
        sem_wait(&FileInfoMap_Mutex);

        // Validate Uname, GroupName & FileName
        auto iter = GroupInfoMap.find(GroupName);
        if (iter == GroupInfoMap.end())
        {
            response = "No such group exists";
            send(p_NewSockFD, response.c_str(), response.size(), 0);
        }
        else
        {
            GroupInfo grp = GroupInfoMap.at(GroupName);
            for (int i = 0; i < grp.UsersList.size(); i++)
            {
                if (Uname == grp.UsersList[i])
                {
                    UserPartOfGrp = true;
                    break;
                }
            }
            if (!UserPartOfGrp)
            {
                response = "User does not belong to the Group. Cannot download file";
                send(p_NewSockFD, response.c_str(), response.size(), 0);
            }
            else
            {
                cerr << "Validating File Name:" << FileName << endl;
                cerr << "Group Fike List-" << endl;
                for (int i = 0; i < grp.FileList.size(); i++)
                {
                    cerr << grp.FileList[i] << endl;
                    if (FileName == grp.FileList[i])
                    {
                        FilePartOfGrp = true;
                        break;
                    }
                }
                if (!FilePartOfGrp)
                {
                    response = "Given file does not belong to Group. Cannot download file";
                    send(p_NewSockFD, response.c_str(), response.size(), 0);
                }
            }
        }

        cerr << "Validations completed" << response.length() << endl;
        cerr << response << endl;

        if (response.length() == 0)
        {
            cerr << "Validation of UserName, GroupName, FileName passed" << endl;

            FileInfo FInfo = FileInfoMap.at(FileName);

            cerr << "File Torrent being generated for File:" << FileName << endl;

            // File Size
            response = FInfo.FileSize;
            send(p_NewSockFD, response.c_str(), response.size(), 0);

            usleep(100000);

            cerr << "Sent File Size:" << FInfo.FileSize << endl;

            // // Fetch SHA1
            // response = FInfo.FileSHA1;
            // send(p_NewSockFD, response.c_str(), response.size(), 0);

            // cerr << "File SHA1:" << FInfo.FileSHA1 << endl;

            // Number of Chuks
            response = to_string(FInfo.NumberOfChunks);
            send(p_NewSockFD, response.c_str(), response.size(), 0);

            usleep(100000);

            cerr << "Sent Number of Chunks:" << FInfo.NumberOfChunks << endl;

            for (int i = 0; i < FInfo.ChunkInfoList.size(); i++)
            {
                if (ValidateUserStatusAndGroup(FInfo.ChunkInfoList[i].PeerName, FInfo.ChunkInfoList[i].GroupName))
                {
                    UserInfo usr = UsersInfoMap.at(FInfo.ChunkInfoList[i].PeerName);

                    response = to_string(FInfo.ChunkInfoList[i].ChunkNo) + " " + usr.IP + " " + to_string(usr.PortNo) + " " + FInfo.ChunkInfoList[i].FilePathInPeer + " " + FInfo.ChunkInfoList[i].SHA1;
                    send(p_NewSockFD, response.c_str(), response.size(), 0);

                    cerr << "Sent Chunk Data:" << response << endl;

                    usleep(100000);
                }
            }
            cerr << "Torrent file sent to client" << endl;
        }

        sem_post(&FileInfoMap_Mutex);
        sem_post(&GroupInfoMap_Mutex);
        sem_post(&UserInfoMap_Mutex);
    }

    string LeaveGroup(string Uname, string GroupName)
    {
        /*
            Upon LeaveGroup, UploadFile information of User still persists in the Data Structures of Files. This is done to reduce the overhead of this method.
            To handle this, we need to exclude in TORRENT from sending User information to client if User does not belong to the group.
        */

        sem_wait(&GroupInfoMap_Mutex);
        string response;
        int UserIndex = -1;
        bool deleteGroup = false;

        auto iter = GroupInfoMap.find(GroupName);
        if (iter == GroupInfoMap.end())
        {
            response = "No such group exists";
        }
        else
        {
            GroupInfo *grp = &GroupInfoMap.at(GroupName);
            cerr << "Fetched Group Obj" << endl;

            for (int i = 0; i < grp->UsersList.size(); i++)
            {
                if (grp->UsersList[i] == Uname)
                {
                    if (grp->OwnerUserName == Uname)
                    {
                        if (grp->UsersList.size() == 1)
                        {
                            deleteGroup = true;
                        }
                        else
                        {
                            // Assign New Owner to Group
                            grp->OwnerUserName = grp->UsersList[(i + 1) % (grp->UsersList.size())];
                            sem_wait(&UserInfoMap_Mutex);
                            UserInfo *usr1 = &UsersInfoMap.at(Uname);
                            for(int i=0; i< usr1->GroupOwnership.size(); i++)
                            {
                                if(usr1->GroupOwnership[i] == GroupName)
                                {
                                    usr1->GroupOwnership.erase(usr1->GroupOwnership.begin()+i);
                                    cerr << "User's Group Ownership List updated" << endl;
                                }
                            }
                            UserInfo *usr2 = &UsersInfoMap.at(grp->OwnerUserName);
                            usr2->GroupOwnership.emplace_back(GroupName);
                            sem_post(&UserInfoMap_Mutex);
                            cerr << "New Owner for Group =" << grp->OwnerUserName << endl;
                        }
                    }

                    UserIndex = i;
                    cerr << "User found in the group" << endl;
                    break;
                }
            }

            if (UserIndex != -1)
            {
                auto iter = grp->UsersList.begin() + UserIndex;
                grp->UsersList.erase(iter);

                if (deleteGroup)
                {
                    // free(grp);
                    GroupInfoMap.erase(GroupName);
                    cerr << "Group Deleted" << endl;
                }
                response = "User deleted from the group";
            }
            else
            {
                response = "User does not blong to the group";
            }
        }

        sem_post(&GroupInfoMap_Mutex);

        return response;
    }

    string StopSharing(string Uname, string GroupName, string FileName)
    {
        sem_wait(&FileInfoMap_Mutex);
        sem_wait(&GroupInfoMap_Mutex);
        string response;

        auto iter = FileInfoMap.find(FileName);
        if (iter == FileInfoMap.end())
        {
            response = "No such File exists";
        }
        else
        {
            FileInfo *file = &FileInfoMap.at(FileName);
            for (int i = 0; i < file->ChunkInfoList.size(); i++)
            {
                if (file->ChunkInfoList[i].PeerName == Uname && file->ChunkInfoList[i].GroupName == GroupName)
                {
                    auto iter = file->ChunkInfoList.begin() + i;
                    file->ChunkInfoList.erase(iter);
                }
            }

            GroupInfo *grp = &GroupInfoMap.at(GroupName);
            for (int i = 0; i < grp->SharableFilesList.size(); i++)
            {
                if (grp->SharableFilesList[i].first == FileName && grp->SharableFilesList[i].second == Uname)
                {
                    auto iter = grp->SharableFilesList.begin() + i;
                    grp->SharableFilesList.erase(iter);
                    break;
                }
            }

            response = "Recorded stop sharing status";
        }

        sem_post(&GroupInfoMap_Mutex);
        sem_post(&FileInfoMap_Mutex);

        return response;
    }

    string Logout(string Uname)
    {
        sem_wait(&UserInfoMap_Mutex);
        string response;
        UserInfo *usr = &UsersInfoMap.at(Uname);
        usr->IsAlive = false;

        response = "User logged out succesfully";
        sem_post(&UserInfoMap_Mutex);

        return response;
    }

    bool FetchSocketMsgFromClient(int p_NewSockFD, vector<string> &msgFromClient)
    {
        bool result = true;
        char buffer[BUFSIZ];
        int nBytes;

        msgFromClient.clear();
        bzero(buffer, BUFSIZ);

        nBytes = read(p_NewSockFD, buffer, BUFSIZ);
        if (nBytes <= 0 || (strcmp(buffer, "Stop") == 0))
        {
            // cout << "ERROR reading from socket" << endl;
            // cerr << "ERROR reading from socket" << endl;
            result = false;
        }
        if (result)
        {
            ParseClientRequest(buffer, msgFromClient);
        }

        return result;
    }

    bool ValidateUserStatusAndGroup(string UserName, string GroupName)
    {
        bool result = true;
        bool UserPartOfGrp = false;

        UserInfo usr = UsersInfoMap.at(UserName);
        if (usr.IsAlive == false)
        {
            result = false;
        }
        if (result)
        {
            // To handle if User left the Group
            GroupInfo grp = GroupInfoMap.at(GroupName);
            for (int i = 0; i < grp.UsersList.size(); i++)
            {
                if (UserName == grp.UsersList[i])
                {
                    UserPartOfGrp = true;
                    break;
                }
            }
            if (!UserPartOfGrp)
            {
                result = false;
            }
        }

        return result;
    }

    void SendStopSignal(int p_NewSockFD)
    {
        string Msg = "Stop";

        // sleep(0.1); // Wait for 10 ms
        usleep(100000); // Wait for 100ms
        send(p_NewSockFD, Msg.c_str(), Msg.size(), 0);
        cerr << Msg << endl;
    }

    bool ValidateCommandArguments(int ReceivedArguments, int ExpectedArguments, int p_NewSockFD)
    {
        bool result = true;
        string Msg = "Invalid command passed";
        if (ReceivedArguments < ExpectedArguments)
        {
            send(p_NewSockFD, Msg.c_str(), Msg.size(), 0);
            cerr << Msg << endl;

            SendStopSignal(p_NewSockFD);
            result = false;
        }
        return result;
    }

    // void sendFile(int sockFd)
    // {
    //     int fd1 = open("./TestPic1.jpg", O_RDONLY);
    //     if (fd1 == -1)
    //     {
    //         // print which type of error have in a code
    //         // printf("Error Number % d\n", errno);

    //         // print program detail "Success or failure"
    //         // perror("Program");
    //         cout << "erroer1" << endl;
    //         cerr << "error1" << endl;
    //         return;
    //     }
    //     char buffer[BUFSIZ];
    //     int n;
    //     while ((n = read(fd1, buffer, BUFSIZ)))
    //     {
    //         cout << "Sent " << n << " Bytes" << endl;
    //         cerr << "Sent " << n << " Bytes" << endl;
    //         if (send(sockFd, buffer, n, 0) == -1)
    //         {
    //             perror("[-]Error in sending file.");
    //             exit(1);
    //         }
    //         // cout << "in client1" << endl;
    //         // sleep(1);
    //         bzero(buffer, BUFSIZ);
    //     }
    // }
};

int main(int argc, char *argv[])
{
    freopen("Log.txt", "w", stderr);

    char Buffer[BUFSIZ];

    if (argc != 3)
    {
        cout << "Incorrect number of Arguments passed" << endl;
        cerr << "Incorrect number of Arguments passed" << endl;
        exit(1);
    }

    tracker T1;
    T1.ParseArguments(argv[1], stoi(argv[2]));

    T1.CreateSocketAndBind();

    // thread th1(T1.ListenForClientRequests);
    // thread th1 = T1.ListenerThread();

    thread th1(&tracker::ListenForClientRequests, T1);
    th1.detach();

    while (1)
    {
        cin.getline(Buffer, BUFSIZ);

        if (strcmp(Buffer, "quit") == 0)
        {
            break;
        }
    }

    return 0;
}