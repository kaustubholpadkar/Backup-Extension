+------+
|README|
+------+

-> Backup utility using ptrace. It will store the backup files in the .backup directory in the home directory of the user. For example, in our case it would be "/home/sekar" in the linux machine provided. Backup directory would be "/home/sekar/.backup".

-> Our extension detects any attempt to overwrite an existing file, e.g., opening a file for write access, opening a file with truncate flags, truncating a file, renaming a file, etc. In all of those cases, our extension copies the existing file into a backup directory with the details of timestamp, in the .backup directory within the user’s home directory.

-> In the backup directory, we will follow the directory structure of the file system while storing the backup file.
   For example, if we want to create a backup of a file located at /home/sekar/dir1/abc.txt,
   our extension will store the backup file in the /home/sekar/.backup/home/sekar/dir1 directory,
   with the filename "abc.txt.Mon_2020-04-06_12:36:04_PDT.bkp if the backup was taken at date & time Mon_2020-04-06_12:36:04_PDT

-> We follow the directory structure inside the backup directory so that if we want to restore the file, it knows where to restore the file according to the d   directory information.

-> We also use date & time related information becayse using such information will let user maintain different versions of same file with its time related information.


-> How to run:

$./backup "command to be run"

The extension takes exactly one argument as string. For example,

$./backup ls
$./backup "ls -l"
$./backup "vi abc.txt"

Note that the following command will not work:
$./backup ls -l
Because it gives more than one argument to our command. Instead run it as:
$./backup "ls -l"


-> To build:

   To build, run the 'make' command:
   $make

   It will create a executable file named 'backup'

-> To clean:

   To clean, run 'make clean' command:
   $make clean

   It will remove the executable file named 'backup'

-> Test cases:

  1) Renaming File using mv command:
  
    
    $./backup "mv abc.txt pqr.txt"
    
    This command will first take backup of abc.txt. It will print the message showing the backup file location. This command will then rename abc.txt as pqr.txt.   

  2) Editing the file using vim editor:

    $./backup "vi names.txt"

    When you open the text file for editting purpose, our command will first take the backup and then user can edit normally. It will print the message showing the backup file location.

  3) Truncating the file using truncate command:

    $./backup "truncate -s 0 trunc.txt"

    The trunc.txt contains some text earlier. We use truncate command to truncate it to size 0. Using backup extension, it will first create a backup file in     our backup directory and then truncate the file normally. It will print the message showing the backup file location.
