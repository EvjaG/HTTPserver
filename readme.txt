HTTP Server Implementation
Programmer's Name: Evyatar Golan

list of files:
readme.txt - this file
threadpool.c - the file that contains the threadpool library implementation
threadpool.h - the header file that contains all the relevant declarations
server.c - the file that contains the HTTP server implementation
makefile - to build the project (Linux only)


special remarks:
if the filetype the server will receive is not in the MIME list, it will return it
with a content-type "application/octet-stream", which will make the browser D/L
it. Please make sure to check the downloads tabs when opening such files.

