# Bit-Torrent
Have developed a basic version of BitTorrent depicting the major functionalities of BitTorrent.
Similar to BitTorrent, our application does not maintain load of File data at the Server, rather the file data and downloads will be shared accross multiple Peers(Users) who have already downloaded the file.

By following this approach, we are able to solve major problems.
1. Load balance at Server is greatly reduced as Server just keeps File metadata and and shares it with Users who request for File Download. As           Server does not keep File Data, there is very little load on Server whos responsibility is to keep track of File info and Seeders (Having the file)
2. Download Time - As the same file or chunks (peices) of the file are available with multiple users, we can parallelly download different chunks (peices) of the file from different users and hence download time is greatly reduced.

Naming conventions discussed in the file:-
- Tracker - Server
- Peer - Client/User
- Seeder - User having the complete File
- Chunk - A piece of File data


Few Keypoints about the Application:
- Use of User-Group mechanism which depicts modern day approach
- Divide load at Server by distributing File Download work among many Peers
- Great reduction in Download time
- Multi-Threading to perform parallel tasks
- Even if a Peer has only few chunks (part) of file, he will be able to contribute as a Seeder (Can share the chunk he has to other user without it)
- Use of SHA1 to validate the downloaded data


# Workflow of downloading a file
1. User can get registered by providing Username and Password and use them for login.
2. User can create a new Group or join a Group maintained by different User (requireves approval from Group owner)
3. User can upload a new File he/she has into Group so that it can be downloaded by other members from the group.
4. User belonging to a group can view available Files and can initiate File Download.
5. A User can wish to stop sharing a file if he no longer want to act as a Seeder.
6. User can quit a group if he no longer wants to be part of the group.

# Architecture/Internal Working of the Application
- Tracker maintains various data structures to track information of User credentials, Group Information, File information, Files within a Group, etc.,
- Tracker maintains the File metadata i.e., Information of file like FileName, FileSize, NumOfChunks, ChunkSHA1, ChunkUser (Particular Chunk is available with which User)
- Upon a Peer's request to download a File, Tracker just sends the File Metadata it has to the Peer
- The Bit-Torrent Peer after receiving the FileMetadata from tracker, will initiate many Parallel connection requests with multiple Peers requesting to send particular Chunk data to it. (Parallel working is acheived by use of Multi-Threading)
- As the Peer-Peer connection is parallel, various chunk data can be downloaded in-parallel at same time. This helps in great reduction of Download Time.
- After a particular chunk is downloaded, the Thread validates the downloaded chunk data by comparing its SHA1 with the SHA1 of chunk received from Tracker. If SHA1 is matched, it is written to the target file.
- Once all the threads have completed their job, the complete File is available with the User.
