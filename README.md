This program is a simple shell program that can run commands and has some built-in commands. 
The program has the functions to run commands in the background and foreground. 
It can handle signals. And can expand $$ to the process id of the shell.
There are soem built-in commands that are cd, status, exit and mkdir
Basically I think it can be able tp pass the test script that is provided in the instruction.



To run the program, please running the following commands in the terminal:

gcc -g --std=gnu99 -o smallsh main.c

./smallsh



If you want to run the test script, you can run this command in the terminal:
place the test script in the same directory, chmod it (chmod +x ./p3testscript) 

./p3testscript > mytestresults 2>&1