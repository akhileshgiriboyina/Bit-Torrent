#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <fcntl.h>

// 512 KB
#define CHUNKSIZE 524288

using namespace std;

struct Chunk
{
    int ChunkNo;
    string PeerIP;
    int PeerPort;
    string FilePath;
    string ChunkSHA1;
};

struct DownloadInfo
{
    bool DownloadComplete = false;
    string UserName;
    string GroupName;
    string FileName;
};

class Peer
{
public:
    string TrackerIP;
    int TrackerPort;
    struct sockaddr_in TrackerAddress;
    string PeerIP;
    int PeerPort;
    int TrackerSockID;
    int ListenSockID;
    bool DownloadWaitFlag = false;
    vector<DownloadInfo> DownloadTracker;

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
                cerr << "Incorrect Tracker Info file" << endl;
                exit(1);
            }
            if (stoi(input[trackerIndex]) != TrackerNumber)
            {
                trackerIndex = 3;
            }
            TrackerIP = input[trackerIndex + 1];
            TrackerPort = stoi(input[trackerIndex + 2]);

            bzero((char *)&TrackerAddress, sizeof(TrackerAddress));
            TrackerAddress.sin_family = AF_INET;
            TrackerAddress.sin_port = htons(TrackerPort);
            if (inet_pton(AF_INET, TrackerIP.c_str(), &TrackerAddress.sin_addr) <= 0)
            {
                cout << "Invalid Host provided" << endl;
                exit(1);
            }

            cerr << "Tracker IP and Port recorded" << endl;
        }
        else
        {
            cout << "Error in opening tracker_text.txt file" << endl;
            cerr << "Error in opening tracker_text.txt file" << endl;
        }
    }

    void OpenListeningPort()
    {
        struct sockaddr_in MyAdress;
        int opt;

        ListenSockID = socket(AF_INET, SOCK_STREAM, 0);
        if (ListenSockID < 0)
        {
            // error("ERROR opening socket");
            cout << "ERROR opening socket to listen" << endl;
            cerr << "ERROR opening socket to listen" << endl;
            exit(1);
        }
        cerr << "Listen Socket Created" << endl;

        if (setsockopt(ListenSockID, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        bzero((char *)&MyAdress, sizeof(MyAdress));
        MyAdress.sin_family = AF_INET;
        MyAdress.sin_port = htons(PeerPort);
        if (inet_pton(AF_INET, PeerIP.c_str(), &MyAdress.sin_addr) <= 0)
        {
            cout << "Invalid Peer Host provided" << endl;
            cerr << "Invalid Peer Host provided" << endl;
            exit(1);
        }

        if (bind(ListenSockID, (struct sockaddr *)&MyAdress, sizeof(MyAdress)) < 0)
        {
            // error("ERROR on binding");
            cout << "ERROR on binding Listen Socket" << endl;
            cerr << "ERROR on binding Listen Socket" << endl;
            exit(1);
        }
        cerr << "Listen Socket Bind done on Port" << PeerPort << endl;

        listen(ListenSockID, INT_MAX);
    }

    void ListenForPeerRequests()
    {
        struct sockaddr_in PeerAddress;
        int newSockFD;
        socklen_t clientLen;
        clientLen = sizeof(PeerAddress);
        while (1)
        {
            // cout << "Listening for Client Request" << endl;
            cerr << "Listening for Client Request" << endl;
            // newSockFD = accept(p_sockFD, (struct sockaddr*) &PeerAddress, &clientLen);
            newSockFD = accept(ListenSockID, (struct sockaddr *)&PeerAddress, &clientLen);
            cerr << "NewSockFD:" << newSockFD << endl;
            if (newSockFD < 0)
            {
                cerr << "Listen: ERROR on accept" << endl;
                continue;
            }
            // cout << "Listen: Received connection from IP- " << inet_ntoa(PeerAddress.sin_addr) << " PORT- " << ntohs(PeerAddress.sin_port) << endl;
            cerr << "Listen: Received connection from IP- " << inet_ntoa(PeerAddress.sin_addr) << " PORT- " << ntohs(PeerAddress.sin_port) << " , Connected on Socket " << newSockFD << endl;
            // sleep(10);
            // cout << "sockFD:" << ListenSockID << ",newSockFD:" << newSockFD << endl;

            thread t(&Peer::RespondToPeer, this, newSockFD);
            t.detach();
            // thread th1 = ListenerThread();
            // thread t(RespondToClient, i);
        }
    }

    void ConnectWithTracker()
    {
        string UserLoggedIn = "";
        char Buffer[BUFSIZ];
        vector<string> ParsedCommand;

        bool CommandSent = false;

        TrackerSockID = socket(AF_INET, SOCK_STREAM, 0);
        if (TrackerSockID < 0)
        {
            cout << "ERROR opening socket" << endl;
            cerr << "ERROR opening socket" << endl;
            exit(1);
        }

        if (connect(TrackerSockID, (struct sockaddr *)&TrackerAddress, sizeof(TrackerAddress)) < 0)
        {
            cout << "ERROR on connecting to Tracker" << endl;
            cerr << "ERROR on connecting to Tracker" << endl;
            exit(1);
        }

        cout << "Connected to Tracker" << endl;
        cerr << "Connected to Tracker" << endl;

        bzero(Buffer, BUFSIZ);
        int n = read(TrackerSockID, Buffer, BUFSIZ);
        cout << Buffer << endl;

        while (true)
        {
            ParsedCommand.clear();
            bzero(Buffer, BUFSIZ);

            cin.getline(Buffer, BUFSIZ);
            ParseConsoleMsg(Buffer, ParsedCommand);

            if (ParsedCommand[0] == "quit")
            {
                exit(1);
            }

            if (UserLoggedIn.length() == 0 && ParsedCommand[0] != "create_user" && ParsedCommand[0] != "login")
            {
                cout << "Please login to initiate a command" << endl;
                cerr << "Please login to initiate a command" << endl;

                continue;
            }

            if (ParsedCommand[0] == "login")
            {
                if (UserLoggedIn.length() > 0)
                {
                    cout << "Please logout prior to re-login" << endl;
                    cerr << "Please logout prior to re-login" << endl;

                    continue;
                }
                strcat(Buffer, " ");
                strcat(Buffer, const_cast<char *>(PeerIP.c_str()));
                strcat(Buffer, " ");
                strcat(Buffer, const_cast<char *>(to_string(PeerPort).c_str()));

                cerr << "Sent command " << Buffer << endl;
                send(TrackerSockID, Buffer, BUFSIZ, 0);
                CommandSent = true;

                UserLoggedIn = ParsedCommand[1];
            }
            else if (ParsedCommand[0] == "upload_file")
            {
                // Send extra information
                if (SendFileInfoToTracker(ParsedCommand))
                {
                    CommandSent = true;
                }
                else
                {
                    CommandSent = false;
                }
            }
            else if (ParsedCommand[0] == "show_downloads")
            {
                for (int i = 0; i < DownloadTracker.size(); i++)
                {
                    if (DownloadTracker[i].UserName == UserLoggedIn)
                    {
                        string MsgToDisplay;
                        if (DownloadTracker[i].DownloadComplete == true)
                        {
                            MsgToDisplay = "[C] ";
                        }
                        else
                        {
                            MsgToDisplay = "[D]";
                        }
                        MsgToDisplay += " ";
                        MsgToDisplay += DownloadTracker[i].GroupName;
                        MsgToDisplay += " ";
                        MsgToDisplay += DownloadTracker[i].FileName;

                        cout << MsgToDisplay << endl;
                        cerr << "Showing Download Status:" << MsgToDisplay << endl;
                    }
                }
                continue;
            }
            else
            {
                if (ParsedCommand[0] == "download_file")
                {
                    DownloadWaitFlag = true;
                }
                else if (ParsedCommand[0] == "logout")
                {
                    UserLoggedIn = "";
                }
                cerr << "Sent command " << Buffer << endl;
                send(TrackerSockID, Buffer, BUFSIZ, 0);
                CommandSent = true;
            }

            // Receive Status Information from Tracker
            cout << endl;
            cout << "-------- Message from Tracker --------" << endl;
            if (DownloadWaitFlag) // For Download Command
            {
                // New Thread to Proceed with download
                cerr << "In DownloadWaitFlag" << ParsedCommand[2] << ParsedCommand[3] << endl;
                string DestPath = ParsedCommand[3];

                // Maintain Tracker information for Download
                DownloadInfo dInfo;
                dInfo.DownloadComplete = false;
                dInfo.UserName = UserLoggedIn;
                dInfo.GroupName = ParsedCommand[1];
                dInfo.FileName = ParsedCommand[2];
                DownloadTracker.emplace_back(dInfo);

                thread t1(&Peer::DownloadFile, this, ParsedCommand[2], ParsedCommand[3], UserLoggedIn, ParsedCommand[1]);
                t1.detach();

                cerr << "Main Thread below DownloadFile" << endl;
                while (DownloadWaitFlag == true)
                {
                    cerr << "Main Thread wait" << endl;
                    sleep(1);
                }
                cerr << "Main Thread aftr DownloadFlagWait" << endl;
            }
            else // For all other Commands
            {
                while (true)
                {
                    bzero(Buffer, BUFSIZ);
                    int n = read(TrackerSockID, Buffer, BUFSIZ);
                    cerr << "Received Msg:" << Buffer << ", Bytes:" << n << endl;

                    if (ParsedCommand[0] == "login" && (strcmp(Buffer, "Incorrect Password") == 0 || strcmp(Buffer, "User not Registered") == 0))
                    {
                        UserLoggedIn = "";
                    }

                    if (n <= 0 || (strcmp(Buffer, "Stop") == 0))
                    {
                        cerr << "Break Tracker Status loop" << endl;
                        break;
                    }
                    else
                    {
                        cout << Buffer << endl;
                    }
                }
                cerr << "Msg from tracker completed. Ready to send new command" << endl;
            }
            cout << "--------------------------------------" << endl;
        }
    }

    void RespondToPeer(int p_NewSOCKID)
    {
        string FilePath;
        int ChunkNo;
        int BlkSize = 32768; // 32 KB
        char Buffer[BlkSize];
        int nBytes, TotalBytes;
        int FD;

        vector<string> MsgFromPeer;

        string MsgToPeer;

        bzero(Buffer, BlkSize);

        cerr << "Respond To Peer triggered over Socket:" << p_NewSOCKID << endl;

        MsgToPeer = "Connected";

        nBytes = send(p_NewSOCKID, MsgToPeer.c_str(), MsgToPeer.size(), 0);
        // cout << nBytes << " no of bytes sent. Data=" << Buffer << endl;
        cerr << "Sock ID: " << p_NewSOCKID << ": Msg sent to peer:" << MsgToPeer << endl;

        bzero(Buffer, BlkSize);

        nBytes = read(p_NewSOCKID, Buffer, BlkSize);
        // cout << nBytes << " no of bytes received" << endl;
        cerr << "Sock ID: " << p_NewSOCKID << ": Message received from Peer:" << Buffer << endl;

        ParseConsoleMsg(Buffer, MsgFromPeer);

        bzero(Buffer, BlkSize);

        if (MsgFromPeer[0] == "request_file")
        {
            FilePath = MsgFromPeer[1];
            ChunkNo = stoi(MsgFromPeer[2]);

            FD = open(FilePath.c_str(), O_RDONLY);
            if (FD == -1)
            {
                MsgToPeer = "Cannot open mentioned File";
                send(p_NewSOCKID, MsgToPeer.c_str(), MsgToPeer.size(), 0);
                // cout << "erroer1" << endl;
                cerr << "Sock ID: " << p_NewSOCKID << ": Cannot open mentioned File" << endl;
                return;
            }
            else
            {
                MsgToPeer = "Download Initiated";
                send(p_NewSOCKID, MsgToPeer.c_str(), MsgToPeer.size(), 0);

                usleep(100000);

                // Send Data from Peer
                nBytes = 0;
                TotalBytes = 0;
                int ChunkOffset = ((ChunkNo - 1) * CHUNKSIZE);
                // cerr << "CHUNK " << ChunkNo << ": Read " << TotalBytes << " bytes from file" << endl;
                cerr << "CHUNK " << ChunkNo << ": Choosen Chunk Offset from Chunk -" << ChunkOffset << endl;
                while (true)
                {
                    bzero(Buffer, BlkSize);

                    // nBytes = read(FD, Buffer, BlkSize);
                    ChunkOffset += nBytes;
                    nBytes = pread(FD, Buffer, BlkSize, ChunkOffset);
                    TotalBytes += nBytes;

                    // cerr << "Sock ID: " << p_NewSOCKID << ": CHUNK " << ChunkNo << ": Sent " << TotalBytes << " Bytes" << endl;
                    if (nBytes < 0)
                    {
                        break;
                    }
                    if (nBytes == 0 || TotalBytes == CHUNKSIZE)
                    {
                        // pwrite(p_NewSOCKID, Buffer, nBytes, ((ChunkNo - 1) * CHUNKSIZE) + TotalBytes);
                        nBytes = send(p_NewSOCKID, Buffer, nBytes, 0);
                        // cerr << "Sock ID: " << p_NewSOCKID << ": CHUNK " << ChunkNo << ": Sent " << TotalBytes << " Bytes" << endl;
                        break;
                    }
                    else
                    {
                        // pwrite(p_NewSOCKID, Buffer, nBytes, ((ChunkNo - 1) * CHUNKSIZE) + TotalBytes);
                        nBytes = send(p_NewSOCKID, Buffer, nBytes, 0);
                        // cerr << "Sock ID: " << p_NewSOCKID << ": CHUNK " << ChunkNo << ": Sent " << TotalBytes << " Bytes" << endl;
                    }

                    usleep(100000);
                }
                cerr << "CHUNK " << ChunkNo << ": Total Bytes sent " << TotalBytes << endl;
            }
        }

        close(p_NewSOCKID);
    }

    bool SendFileInfoToTracker(vector<string> p_ParsedCommand)
    {
        bool result = true;

        string FileName;
        string FileSize;
        string ChunkNo;
        int NumOfChunks, ChunkIndex;
        struct stat fInfo;
        string FilePath = p_ParsedCommand[1];
        string ChunkMsg;

        cerr << "Uploading File Info to Tracker" << endl;

        cerr << "File Path:" << FilePath << endl;

        if (stat(FilePath.c_str(), &fInfo) == 0)
        {
            cerr << "File Stat found" << endl;
            FileSize = to_string(fInfo.st_size);
            FileSize += " Bytes";
            NumOfChunks = fInfo.st_size / CHUNKSIZE;
            if (fInfo.st_size % CHUNKSIZE != 0)
            {
                NumOfChunks++;
            }
        }
        else
        {
            cout << "File Info not found. Cannot upload" << endl;
            cerr << "File Info not found. Cannot upload" << endl;

            return false;
        }
        cerr << "File Size:" << fInfo.st_size << endl;
        cerr << "Number of Chunks in file:" << NumOfChunks << endl;

        string FileSHA1;
        // string Chunk_SHA1[NumOfChunks + 1];
        vector<string> Chunk_SHA1;
        unsigned char chunk[CHUNKSIZE];
        int fd = open(FilePath.c_str(), O_RDONLY);
        int nBytes, i = 1;

        // while ((nBytes = read(fd, chunk, CHUNKSIZE)) > 0)
        // {
        //     cerr << "Computing SHA1 of Chunk " << i << endl;
        //     Chunk_SHA1[i] = ComputeSHA1(chunk, nBytes);
        //     FileSHA1 += Chunk_SHA1[i];

        //     cerr << Chunk_SHA1[i] << endl;

        //     i++;
        // }

        Chunk_SHA1 = cal_SHA(FilePath);


        cerr << "Computed SHA1 of all Chunks" << endl;

        // Command - upload_file <FileName> <GroupName>
        size_t found = FilePath.find_last_of('/');
        if (found == string::npos)
        {
            FileName = FilePath;
        }
        else
        {
            FileName = FilePath.substr(found + 1);
        }
        string command = p_ParsedCommand[0] + " " + FileName + " " + p_ParsedCommand[2];
        send(TrackerSockID, command.c_str(), command.size(), 0);
        usleep(100000);

        cerr << "Sent Upload Command:" << command << endl;

        // File Size
        send(TrackerSockID, FileSize.c_str(), FileSize.size(), 0);
        usleep(100000);

        cerr << "Sent File Size:" << FileSize << endl;

        // // SHA 1
        // send(TrackerSockID, FileSHA1.c_str(), FileSHA1.size(), 0);
        // usleep(1000000);

        // cerr << "Sent SHA1:" << FileSHA1 << endl;

        // Number of Chunks
        ChunkNo = to_string(NumOfChunks);
        send(TrackerSockID, ChunkNo.c_str(), ChunkNo.size(), 0);
        usleep(100000);

        cerr << "Sent NumOfChunks:" << ChunkNo << endl;

        cerr << "Sent File Information" << endl;

        // Each Chunk Information
        for (ChunkIndex = 1; ChunkIndex <= NumOfChunks; ChunkIndex++)
        {
            /*// Chunk Number
            ChunkNo = to_string(ChunkIndex);
            send(TrackerSockID, ChunkNo.c_str(), ChunkNo.size(), 0);

            cerr << "Sent Chunk No:" << ChunkNo << endl;
            usleep(100000);

            // Chunk Path
            send(TrackerSockID, FilePath.c_str(), FilePath.size(), 0);

            cerr << "Sent Chunk Path:" << FilePath << endl;
            usleep(100000);

            // Chunk SHA1
            send(TrackerSockID, Chunk_SHA1[i].c_str(), Chunk_SHA1[i].size(), 0);

            cerr << "Sent Chunk SHA1:" << Chunk_SHA1[i] << endl;
            usleep(100000);*/

            ChunkMsg = to_string(ChunkIndex) + " " + FilePath + " " + Chunk_SHA1[ChunkIndex];
            send(TrackerSockID, ChunkMsg.c_str(), ChunkMsg.size(), 0);

            cerr << "Sent Chunk Data:" << ChunkMsg << endl;
            usleep(100000);
        }
        cerr << "Sent All chunks Information" << endl;

        string Msg = "Stop";
        send(TrackerSockID, Msg.c_str(), Msg.size(), 0);
        cerr << Msg << endl;

        return result;
    }

    void DownloadFile(string FileName, string DestinationPath, string UserLoggedIn, string GroupName)
    {
        string FileSize;
        string File_SHA1;
        int NumOfChunks;

        cerr << "Downloading File Information from Tracker" << endl;
        // First Receive File Metadata from Tracker to proceed with Download
        vector<string> msgFromTracker;

        // //File Name
        // FetchSocketMsgFromTracker(msgFromTracker);

        // File Size
        FetchSocketMsgFromTracker(msgFromTracker);
        FileSize = msgFromTracker[0];
        cerr << "Received File Size:" << FileSize << endl;

        // //File SHA 1
        // FetchSocketMsgFromTracker(msgFromTracker);
        // File_SHA1 = msgFromTracker[0];
        // cerr << "Received File SHA1:" << File_SHA1 << endl;

        // Number of Chunks
        FetchSocketMsgFromTracker(msgFromTracker);
        NumOfChunks = stoi(msgFromTracker[0]);
        cerr << "Received Number of Chunks:" << NumOfChunks << endl;

        // int ChunkCountArr[NumOfChunks];
        vector<Chunk> ChunkList[NumOfChunks + 1];  // Array of Chunks containing information of availability in different users
        vector<pair<int, int>> ChunkDownloadOrder; // Maintains count of number of Users having the Chunk - pait<ChunkNo, count>

        // Initialize Vector with 0
        for (int i = 0; i <= NumOfChunks; i++)
        {
            ChunkDownloadOrder.emplace_back(make_pair(i, 0));
        }

        cerr << "Receving all Chunks Information" << endl;
        // Each Chunk Information
        while (true)
        {
            if (FetchSocketMsgFromTracker(msgFromTracker))
            {
                Chunk chnk;

                // Chunk Number
                chnk.ChunkNo = stoi(msgFromTracker[0]);

                cerr << "Chunk No:" << chnk.ChunkNo << endl;

                // Peer IP
                chnk.PeerIP = msgFromTracker[1];

                cerr << "Chunk PeerIP:" << chnk.PeerIP << endl;

                // Peer Port
                chnk.PeerPort = stoi(msgFromTracker[2]);

                cerr << "Chunk Peer Port:" << chnk.PeerPort << endl;

                // File Path
                chnk.FilePath = msgFromTracker[3];

                cerr << "Chunk File Path:" << chnk.FilePath << endl;

                // Chunk SHA1
                chnk.ChunkSHA1 = msgFromTracker[4];

                cerr << "Chunk SHA1:" << chnk.ChunkSHA1 << endl;

                ChunkList[chnk.ChunkNo].emplace_back(chnk);
                ChunkDownloadOrder[chnk.ChunkNo].second++;
            }
            else
            {
                break;
            }
        }

        cerr << "All Chunks Information Received" << endl;

        DownloadWaitFlag = false; // Reset the flag so that Peer Thread keep running parallely

        cout << "File Torrent Downloaded from Tracker. Downloading File will be performed parallely" << endl;
        cerr << "File Torrent Downloaded from Tracker" << endl;

        /* ------------Rarest Chunk First Algorithm----------- */
        DestinationPath = DestinationPath + "/" + FileName;
        int FD = open(DestinationPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (FD < 0)
        {
            cerr << "Error creating file at Destination";
            return;
        }

        cerr << "File Opened/Created at Destination path" << endl;

        // cerr << "Before RarestFirst Algo:" << endl;
        for (int i = 0; i <= NumOfChunks; i++)
        {
            cerr << "i:" << i << ", Chunk No:" << ChunkDownloadOrder[i].first << ", Chunk Count:" << ChunkDownloadOrder[i].second << endl;
        }

        sort(ChunkDownloadOrder.begin(), ChunkDownloadOrder.end(), RarestChunkFirstCompare);
        // sort(ChunkDownloadOrder.begin(), ChunkDownloadOrder.end(), bind(&Peer::RarestChunkFirstCompare, this, _1, _2));

        cerr << endl;
        cerr << "Sorted Peers based on RarestFirst Algo:" << endl;
        for (int i = 0; i <= NumOfChunks; i++)
        {
            cerr << "i:" << i << ", Chunk No:" << ChunkDownloadOrder[i].first << ", Chunk Count:" << ChunkDownloadOrder[i].second << endl;
        }

        int ChosenChunkIndex;
        int ChoesnPeerIndex;
        vector<thread> ChunkDownloadThreads;
        int ThreadCount = 0;
        cerr << "NumOfChunks:" << NumOfChunks << endl;
        srand(time(0));
        for (int i = 0; i <= NumOfChunks; i++)
        {
            if (ChunkDownloadOrder[i].second > 0)
            {
                // cerr << " Initiating Download Chunk - i:" << i << ", Chunk No:" << ChunkDownloadOrder[i].first << ", Chunk Count:" << ChunkDownloadOrder[i].second << endl;
                ChosenChunkIndex = ChunkDownloadOrder[i].first;
                ChoesnPeerIndex = rand() % ChunkList[ChosenChunkIndex].size(); // Random Peer from the available list
                Chunk chnk = ChunkList[ChosenChunkIndex][ChoesnPeerIndex];

                // thread t1(&Peer::DownloadChunk, this, chnk.ChunkNo, chnk.PeerIP, chnk.PeerPort, chnk.FilePath, chnk.ChunkSHA1, DestinationPath, FD, UserLoggedIn, GroupName);
                // t1.detach();

                // ChunkDownloadThreads.emplace_back(move(t1));

                // cerr << " Initiating Download Chunk - i:" << i << ", Chunk No:" << chnk.ChunkNo << endl;
                ChunkDownloadThreads.emplace_back(thread(&Peer::DownloadChunk, this, chnk.ChunkNo, chnk.PeerIP, chnk.PeerPort, chnk.FilePath, chnk.ChunkSHA1, DestinationPath, FD, UserLoggedIn, GroupName));
                usleep(10000);
                ThreadCount++;

                if (ThreadCount == 20)
                {
                    for (int i = 0; i < ChunkDownloadThreads.size(); i++)
                    {
                        ChunkDownloadThreads[i].join();
                        cerr << "Download thread : joined thread " << i + 1 << endl;
                    }
                    ChunkDownloadThreads.clear();
                    ThreadCount = 0;
                }
            }
        }

        // Chunk chnk = ChunkList[1][0];
        // thread t1(&Peer::DownloadChunk, this, chnk.ChunkNo, chnk.PeerIP, chnk.PeerPort, chnk.FilePath, chnk.ChunkSHA1, DestinationPath, FD, UserLoggedIn, GroupName);
        // t1.detach();
        // ChunkDownloadThreads.emplace_back(move(t1));
        // ChunkDownloadThreads.emplace_back(t1)

        // ChunkDownloadThreads.emplace_back(thread(&Peer::DownloadChunk, this, chnk.ChunkNo, chnk.PeerIP, chnk.PeerPort, chnk.FilePath, chnk.ChunkSHA1, DestinationPath, FD, UserLoggedIn, GroupName));

        cerr << "Download thread waiting for completion of ChunkDownlod Threads" << endl;
        for (int i = 0; i < ChunkDownloadThreads.size(); i++)
        {
            ChunkDownloadThreads[i].join();
            cerr << "Download thread : joined thread for CHUNK " << i + 1 << endl;
        }
        // for (std::thread &th : ChunkDownloadThreads)
        // {
        //     // If thread Object is Joinable then Join that thread.
        //     // if (th.joinable())
        //     // {
        //     //     th.join();
        //     // }
        //     th.join();
        //     ChunkDownloadThreads.erase(ChunkDownloadThreads.begin());
        // }
        // for (auto it = ChunkDownloadThreads.begin(); it != ChunkDownloadThreads.end(); it++)
        // {
        //     // if (it->joinable())
        //     // {
        //     //     // threadsJoined++;
        //     //     // cout << threadsJoined << endl;
        //     //     it->join();
        //     // }
        //     it->join();
        // }
        // while (ChunkDownloadThreads.size() > 0)
        // {
        //     ChunkDownloadThreads[0].join();
        //     ChunkDownloadThreads.erase(ChunkDownloadThreads.begin());
        // }

        cerr << "Download Thread below Join of all ChunkDownload Threads" << endl;

        // Update Download Tracker
        for (int i = 0; i < DownloadTracker.size(); i++)
        {
            if (DownloadTracker[i].UserName == UserLoggedIn && DownloadTracker[i].GroupName == GroupName && DownloadTracker[i].FileName == FileName)
            {
                DownloadTracker[i].DownloadComplete = true;
            }
        }

        close(FD);
        cerr << "Download File Completed" << endl;
    }

    static bool RarestChunkFirstCompare(const pair<int, int> &i, const pair<int, int> &j)
    {
        return i.second < j.second;
    }

    void DownloadChunk(int p_ChunkNo, string p_PeerIP, int p_PeerPort, string PeerFilePath, string ChunkSHA1, string DestinationPath, int FD, string UserLoggnedIn, string GroupName)
    {
        // Establish Connection to Peer for Requesting transmission of data
        int PeerSockID;
        sockaddr_in PeerAddress;
        int BlkSize = 32768; // 32KB
        char Buffer[BlkSize];
        int nBytes, TotalBytes;

        char ReceivedChunkData[CHUNKSIZE];
        string RecvChunk = "";

        bool CommandSent = false;

        // cerr << "CHUNK " << p_ChunkNo << ": Download Chunk Initiated, Peer IP" << p_PeerIP << ", Peer Port" << p_PeerPort << ", File Path" << PeerFilePath << endl;

        PeerSockID = socket(AF_INET, SOCK_STREAM, 0);
        // cout << "PeerSockID:" << PeerSockID << endl;
        if (PeerSockID < 0)
        {
            // cout << "ERROR opening socket" << endl;
            cerr << "CHUNK " << p_ChunkNo << ": ERROR opening socket" << endl;
            return;
        }

        bzero((char *)&PeerAddress, sizeof(PeerAddress));

        PeerAddress.sin_port = htons(p_PeerPort);
        PeerAddress.sin_family = AF_INET;
        if (inet_pton(AF_INET, p_PeerIP.c_str(), &PeerAddress.sin_addr) <= 0)
        {
            cerr << "CHUNK " << p_ChunkNo << ": Invalid Peer provided" << endl;
            return;
        }

        // cerr << "CHUNK " << p_ChunkNo << ": Trying to connect to Peer with IP: " << p_PeerIP << " and Port:" << p_PeerPort << " over Socket " << PeerSockID << endl;
        // sleep(5);

        if (connect(PeerSockID, (struct sockaddr *)&PeerAddress, sizeof(PeerAddress)) < 0)
        {
            // cout << "ERROR on connecting to Tracker" << endl;
            cerr << "CHUNK " << p_ChunkNo << ": ERROR on connecting to source Peer" << endl;
            return;
        }
        // cerr << "PeerSockID:" << PeerSockID << " Connected" << endl;

        // cout << "Connected to Tracker" << endl;
        // cerr << "CHUNK " << p_ChunkNo << ": Connected to source Peer on socket " << PeerSockID << endl;

        bzero(Buffer, BlkSize);
        // sleep(1);
        nBytes = read(PeerSockID, Buffer, BlkSize);
        // cerr << "CHUNK " << p_ChunkNo << ": Received " << nBytes << " Bytes from socket. Data=" << Buffer << endl;

        if (strcmp(Buffer, "Connected") == 0)
        {
            // cerr << "CHUNK " << p_ChunkNo << ": Inside Connected" << endl;
            string RequestMsg = "request_file " + PeerFilePath + " " + to_string(p_ChunkNo);

            send(PeerSockID, RequestMsg.c_str(), RequestMsg.size(), 0);
            // cerr << "CHUNK " << p_ChunkNo << ": Request Msg-" << RequestMsg << " on to socket " << PeerSockID << endl;

            int ChunkOffset = ((p_ChunkNo - 1) * CHUNKSIZE);
            cerr << "CHUNK " << p_ChunkNo << ": Offset choosen for Chunk -" << ChunkOffset << endl;

            bzero(Buffer, BlkSize);
            bzero(ReceivedChunkData, CHUNKSIZE);
            nBytes = read(PeerSockID, Buffer, BlkSize);
            string cnct = "";
            if (strcmp(Buffer, "Download Initiated") == 0)
            {
                // cerr << "CHUNK " << p_ChunkNo << ": Inside Download Initiated" << endl;
                // Download Data from Peer
                TotalBytes = 0;
                nBytes = 0;
                while (true)
                {
                    bzero(Buffer, BlkSize);

                    ChunkOffset += nBytes;
                    nBytes = read(PeerSockID, Buffer, BlkSize);
                    // if (nBytes <= 0 || TotalBytes == CHUNKSIZE)
                    if (nBytes <= 0)
                    {
                        // pwrite(FD, Buffer, nBytes, ChunkOffset);
                        // TotalBytes += nBytes;
                        strcat(ReceivedChunkData, Buffer);
                        // cnct +=  string(Buffer);
                        RecvChunk += Buffer;
                        break;
                    }
                    else
                    {
                        pwrite(FD, Buffer, nBytes, ChunkOffset);
                        cnct +=  string(Buffer);
                        TotalBytes += nBytes;
                        strcat(ReceivedChunkData, Buffer);
                        RecvChunk += Buffer;
                    }
                }

                string NewSHA1 = ComputeSHA1((unsigned char*)ReceivedChunkData, TotalBytes);
                cerr << "CHUNK " << p_ChunkNo << ": Old SHA:" << endl;
                cerr << ChunkSHA1 << endl;
                cerr << "CHUNK " << p_ChunkNo << ": New SHA:" << endl;
                cerr << NewSHA1 << endl;

                char tmp[CHUNKSIZE+1];
                bzero(tmp, CHUNKSIZE+1);
                // nBytes = pread(FD, tmp, CHUNKSIZE, ChunkOffset - TotalBytes);
                strcpy(tmp, cnct.c_str());
                cerr << "Block Read" << endl;
                NewSHA1 = ComputeSHA1((unsigned char*)tmp, TotalBytes);
                cerr << "CHUNK " << p_ChunkNo << ": New SHA:" << endl;
                cerr << NewSHA1 << endl;

                if (strcmp(ChunkSHA1.c_str(), NewSHA1.c_str()) == 0)
                {
                    cerr << "CHUNK " << p_ChunkNo << ": SHA1 of chunk validated" << endl;
                }
                else
                {
                    cerr << "CHUNK " << p_ChunkNo << ": SHA1 of chunk Invalid" << endl;
                }

                cerr << "CHUNK " << p_ChunkNo << ": Received " << TotalBytes << " Bytes" << endl;

                // pwrite(FD, RecvChunk.c_str(), RecvChunk.size(), ChunkOffset);

                // Update Tracker
                UpdateTracker(UserLoggnedIn, GroupName, p_ChunkNo, ChunkSHA1, DestinationPath);
            }
            else
            {
                cerr << "CHUNK " << p_ChunkNo << ": Response Msg from Peer" << Buffer << endl;
            }
        }
        else
        {
            cerr << "CHUNK " << p_ChunkNo << ": Connection not established with Peer" << endl;
        }

        close(PeerSockID);
    }

    void UpdateTracker(string UserLoggedIn, string GroupName, int p_ChunkNo, string ChunkSHA1, string DestinationPath)
    {
        // Establish Connection to Peer for Requesting transmission of data
        string FileName;
        int TrackerSockID;
        char Buffer[BUFSIZ];
        int nBytes;

        char ReceivedChunkData[CHUNKSIZE];

        bool CommandSent = false;

        TrackerSockID = socket(AF_INET, SOCK_STREAM, 0);
        if (TrackerSockID < 0)
        {
            // cout << "ERROR opening socket" << endl;
            cerr << "CHUNK " << p_ChunkNo << ": ERROR opening socket to Tracker" << endl;
            return;
        }

        if (connect(TrackerSockID, (struct sockaddr *)&TrackerAddress, sizeof(TrackerAddress)) < 0)
        {
            // cout << "ERROR on connecting to Tracker" << endl;
            cerr << "CHUNK " << p_ChunkNo << ": ERROR on connecting to Tracker" << endl;
            return;
        }

        // cout << "Connected to Tracker" << endl;
        // cerr << "CHUNK " << p_ChunkNo << ": Connected to Tracker" << endl;

        bzero(Buffer, BUFSIZ);
        nBytes = read(TrackerSockID, Buffer, BUFSIZ);
        // cout << Buffer << endl;

        size_t found = DestinationPath.find_last_of('/');
        if (found == string::npos)
        {
            FileName = DestinationPath;
        }
        else
        {
            FileName = DestinationPath.substr(found + 1);
        }
        string Msg = "upload_chunk " + FileName + " " + UserLoggedIn + " " + GroupName + " " + to_string(p_ChunkNo) + " " + DestinationPath + " " + ChunkSHA1;

        cerr << "CHUNK " << p_ChunkNo << ": Update msg sent to Traccker-" << Msg << endl;
        if (send(TrackerSockID, Msg.c_str(), Msg.size(), 0) < 0)
        {
            cerr << "CHUNK " << p_ChunkNo << ": Error on update tracker" << endl;
        }

        sleep(1);
    }

    void ParseConsoleMsg(char *buffer, vector<string> &p_Command)
    {
        string word;
        istringstream iss(buffer);
        while (iss >> word)
        {
            p_Command.emplace_back(word);
        }
    }

    string ComputeSHA1(unsigned char *p_ChunkData, int p_BytesRead)
    {
        // char SHA_Arr[SHA_DIGEST_LENGTH];
        // string ComputedSHA;
        // SHA_CTX ctx;
        // SHA1_Init(&ctx);
        // unsigned char hASH[20];
        // char p[40];

        // SHA1_Update(&ctx, p_ChunkData, p_BytesRead);

        // SHA1_Final((unsigned char *)SHA_Arr, &ctx);

        // for (int i = 0; i <= 19; i++)
        // {
        //     if (SHA_Arr[i] != '\0' && SHA_Arr[i] != '\t' && SHA_Arr[i] != '\n')
        //     {
        //         ComputedSHA += SHA_Arr[i];
        //     }
        //     else
        //     {
        //         ComputedSHA += "-";
        //     }
        // }

        string ComputedSHA;
        unsigned char hASH[20];
        char p[20];
        SHA1(p_ChunkData, p_BytesRead,hASH);
        cerr << "1.SHA" << endl;

        for (int i = 0; i < 20; i++)
        {
            sprintf(p + i, "%02x", hASH[i]);
            // sprintf(p + 2 * i, "%02x", hASH[i]);
        }
        cerr << "2.SHA" << endl;
        ComputedSHA = p;

        // cerr << ComputedSHA << endl;

        return ComputedSHA;
    }

    // get sha of that file..
    vector<string> cal_SHA(string f_path)
    {
        vector<string> result_SHA;
        result_SHA.push_back("");
        FILE *filePoiner = fopen(f_path.c_str(), "r+");
        if (filePoiner == NULL)
        {
            // cout << errorStart << "file can't be opened" << printEnd << endl;
            return result_SHA;
        }
        unsigned char buffer[CHUNKSIZE];
        unsigned char hASH[20];
        char p[20];
        string chunkString = "";
        int n;
        int countChunks = 0;
        while ((n = fread(buffer, 1, sizeof(buffer), filePoiner)) > 0)
        {
            SHA1(buffer, n, hASH);
            bzero(buffer, CHUNKSIZE);
            for (int i = 0; i < 20; i++)
                sprintf(p + i, "%02x", hASH[i]);
            result_SHA.push_back(p);
            countChunks++;
        }
        return result_SHA;
    }

    void SendStopSignal(int p_NewSockFD)
    {
        string Msg = "Stop";

        // sleep(0.1); // Wait for 10 ms
        usleep(100000); // Wait for 100ms
        send(p_NewSockFD, Msg.c_str(), Msg.size(), 0);
        cerr << Msg << endl;
    }

    bool FetchSocketMsgFromTracker(vector<string> &msgFromTracker)
    {
        bool result = true;
        char buffer[BUFSIZ];
        int nBytes;

        msgFromTracker.clear();
        bzero(buffer, BUFSIZ);

        nBytes = read(TrackerSockID, buffer, BUFSIZ);
        if (nBytes <= 0 || (strcmp(buffer, "Stop") == 0))
        {
            // cout << "ERROR reading from socket" << endl;
            // cerr << "ERROR reading from socket" << endl;
            result = false;
        }
        if (result)
        {
            cerr << "Buffer:" << buffer << endl;
            ParseConsoleMsg(buffer, msgFromTracker);
        }

        return result;
    }

    // void ParseConsoleMsg(vector<string> &msgFromClient)
    // {
    //     char buffer[BUFSIZ];
    //     int nBytes;

    //     msgFromClient.clear();
    //     bzero(buffer, BUFSIZ);

    //     nBytes = read(p_NewSockFD, buffer, BUFSIZ);
    //     if (nBytes < 0)
    //     {
    //         cout << "ERROR reading from socket" << endl;
    //         cerr << "ERROR reading from socket" << endl;
    //     }

    //     // ParseClientRequest(buffer, msgFromClient);
    // }
};

int main(int argc, char *argv[])
{
    freopen("Log2.txt", "w", stderr);

    string clientInfo;
    string tmp = "";

    if (argc != 3)
    {
        cout << "Incorrect number of Arguments passed" << endl;
        cerr << "Incorrect number of Arguments passed" << endl;
        exit(1);
    }

    Peer P1;
    P1.ParseArguments(argv[2], 1);

    clientInfo = argv[1];
    for (int i = 0; i <= clientInfo.length() - 1; i++)
    {
        if (clientInfo[i] == ':')
        {
            P1.PeerIP = tmp;
            tmp = "";
            continue;
        }
        tmp += clientInfo[i];
    }
    P1.PeerPort = stoi(tmp);

    P1.OpenListeningPort();
    thread th(&Peer::ListenForPeerRequests, P1);

    P1.ConnectWithTracker();

    return 1;
}