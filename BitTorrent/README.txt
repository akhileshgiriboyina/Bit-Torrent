tracker_info.txt
The file should contains list of trackers with their IP and Port
Ex:-
1 10.42.0.102 8086


Running the application:-
Tracker - ./tracker tracker_info.txt tracker_no
Client - ./client <IP>:<PORT> tracker_info.txt


COMMANDS used by the Application aliong with Syntax:-
- CREATE User Account
create_user <user_id> <password>

- LOGIN
login <user_id> <password>

- CREATE Group
create_group <group_id>

- JOIN Group
join_group <group_id>

- LEAVE Group
leave_group <group_id>

- List Pending Join Requests of a Group
list_requests<group_id>

- Accept Group Joining Request
accept_request <group_id> <user_id>

- List All Groups in Network
list_groups

- List Sharable Files in Group
list_files <group_id>

- UPLOAD File
upload_file <file_path> <group_id>

- DOWNLOAD File
download_file <group_id> <file_name> <destination_path>

- LOGOUT
logout

- Show Download status
show_downloads

- STOP Sharing
stop_share <group_id> <file_name>
