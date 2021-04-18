#include "threadpool.c"
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>


#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
/**************************
        VARIABLES
**************************/
typedef struct{
      int numOfArgs;
      int fd;
      int printSize;
      int messageSize;
      int fAlloc;
        /* errorVar's location meaning:
        * 0 - content type
        * 1 - Location (ONLY FOR HEADER 302)
        */
      char** errorVar;
      char* header; //the location of the file/folder
      char* message; // the mesage to string to the end of the headers
      char* toPrint;
      FILE* filep;
} header_cont;
// char ERRMSG[500000];

/**************************
        DECLARATIONS
**************************/

//main functions
int work(void* data);
int readParser(header_cont* head);
int parseFile(header_cont *data);
int fillHeader(int errorcode, header_cont* vars);
int writeToClient(header_cont *data);
void freeFromClient(header_cont *data);
//side/assist functions
void getTime(char* buf);
void error(char* msg,void* data);
char *get_mime_type(char *name);
char *codeWord(int code);
int strlen2(char* data);
char* convertFromSpaces(char* loc1);
char* convertToSpaces(char* loc1);
void strcpy2(char* a, char* b);
int findKnownType(char* ext);
int fileParser(char* loc1);



/**************************
*        FUNCTIONS        *
**************************/




/*************************
        MAIN OPERATIONS
**************************/


//work function for threads
int work(void* data)
{
        //first, send the fd to a read function that will read it and parse whether or not it's legal
        int code = readParser((header_cont*) data);

        //check if you need to check further into the file
        if(code==555)
                code=parseFile(data);
        if(code == -1)
                return code;
       
        //then, check the code and decide what to input into the message to be outputted to the user
        char buf[10000];
        switch (code)
        {
                case 400:
                        sprintf(buf,"<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD><BODY><H4>400 Bad request</H4>Bad Request.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
                case 404:
                        sprintf(buf,"<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD><BODY><H4>404 Not Found</H4>File not found.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
                case 302:
                        sprintf(buf,"<HTML><HEAD><TITLE>302 Found</TITLE></HEAD><BODY><H4>302 Found</H4>Directories must end with a slash.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
                case 403:
                
                        sprintf(buf,"<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD><BODY><H4>403 Forbidden</H4>Access denied.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
                case 500:
                        sprintf(buf,"<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD><BODY><H4>500 Internal Server Error</H4>Some server sideerror.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
                case 501:
                        sprintf(buf,"<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD><BODY><H4>501 Not supported</H4>Method is not supported.</BODY></HTML>");
                        ((header_cont*) data)->message = (char*)malloc(sizeof(char)*10000);
                        strcpy(((header_cont*) data)->message,buf);
                        ((header_cont*) data)->toPrint = (char*) calloc (2500,sizeof(char));
                        break;
        }

        
        
        code=fillHeader(code,data);
        if(code == -1)
                return code;
        // printf("%s\n",((header_cont*) data)->toPrint);
        ((header_cont*)data)->printSize = strlen2(((header_cont*)data)->toPrint);
        
        
        code=writeToClient(data);
        if(code == -1)
                return code;
        freeFromClient(data);
        

        return 0;
}

//reads the inputted data and parses it
int readParser(header_cont* head)
{       
        head->fAlloc=0;
        char temp[4500];
        int rc=read(head->fd,temp,2000);
        if(rc<=0)
                return -1;
        // head->header=(char*) calloc(strlen2(temp)+1,sizeof(char));
        int j=-1,k=0,h=1, slashCount=0,slashBad=0;
        for(int i=0;temp[i]!='\0';i++)
        {
                if(temp[i]=='\n'||temp[i]=='\r')
                {
                        temp[i]='\0';
                        j=i;
                        break;
                }
				if(temp[i]=='\\')
				{
					slashCount++;
					if(slashCount>1)
						slashBad=1;
				} else {
					slashCount--;
				}
                if(temp[i]==' ' && h)
                {
                        k++;
                        h--;
                } else if(temp[i]!=' ' && !h)
                {
                        h++;
                }
        }
        if(j==-1 || k!= 2 || slashBad==1)//if the server received a bad input
                return 400;


        /*** start breaking to words and checking legality ***/
        //check that the first item has a GET request
        char *token;
        token = strtok(temp, " ");
        if(strcmp(token, "GET") != 0)
                return 501;
        //copy location reuest to head->header
        token=strtok(NULL," ");
        int headerlen = strlen2(token);
        head->header=(char*)calloc(headerlen+1,sizeof(char));
        strcpy(head->header,token);
        //check that the last item is HTTP/1.0
        token = strtok(NULL, " ");
        if((strcmp(token, "HTTP/1.1") != 0) && (strcmp(token, "HTTP/1.0") != 0) )//if http isn't ver 1.1 or 1.0
                return 501;//return method not supported

        if(headerlen > 1){
                //check for two slashes in a row
                char toComp=head->header[0];
                for(int i=1;i<headerlen;i++)
                {
                        if((head->header[i]==toComp && toComp=='/') ||(head->header[i]=='/' && toComp=='.' && i=1))
                                return 400;
                        toComp=head->header[i];
                }
        }
		
		
        char* ext = get_mime_type(head->header);//check for header type
        if(ext == NULL)
        {
                char temp[] = "1.txt";
                ext=get_mime_type(((char*)&temp));
        }
        strcpy2(head->errorVar[0],ext);
        free(ext);		
		return 555;//check dir for index.html
        

        return 0;        
}

/**
 * looks up whether a file or a folder are actually available at given location, 
 * and attach them to message if exist. Return 1 if found file, 2 if folder contents,
 * otherwise, return 404 if nothing of that sort was found 
 */
int parseFile(header_cont *data)
{  
        //for header of location
        int locLen = 0;
        for(;data->header[locLen]!='\0';locLen++);
        // char loc[2500];
        char* loc = (char*)calloc(locLen+1000,sizeof(char));
        if(loc==NULL)
        {
                error("Cannot allocate location memory",data);
                return -1;
        }
        strcpy(loc,"");
        if(loc==NULL)
        {
                error("cannot allocate location data",data);
                return -1;
        }
        loc[0]='.';
        int indexExists=0;
        if(data->header!=NULL)
                strcat(loc,data->header);
        int toReturnEarly = fileParser(loc);
        if(toReturnEarly<0)
        {
                free(loc);
                return toReturnEarly*-1;
        }
        if(loc[strlen2(loc)-2]=='/')
        {//if the user is searching for a directory
                // fclose(fopen("./headIndic","w+"));
                int headIndic=0;
                char* SpacesName=convertToSpaces(loc);
                strcpy(loc,SpacesName);
                free(SpacesName);
                DIR *dir=opendir(loc);
                if(dir==NULL)//if we couldn't open a file, that means it's not there.
                {
                        free(loc);
                        switch (errno)
                        {
                        case EACCES:
                                return 403;
                        case EBADE:
                                return 404;
                        case ENOENT:
                                return 404;
                        default:
                                return 500;
                        }
                }
                int dirSize=0;
                struct dirent* dentry;
                while( (dentry=readdir(dir)) != NULL)
                {
                        if(strcmp("index.html",dentry->d_name) == 0)
                        {
                                indexExists=1;
                                break;
                        } else if(strcmp(dentry->d_name,"headIndic") == 0)
                        {
                                headIndic=1;
                        }
                        dirSize++;
                }
                if(indexExists)//if we found a file called index.html, move forward and return it
                {
                        data->header=(char*) realloc(data->header,sizeof(char)*(strlen2(loc)+20));
                        strcat(data->header,dentry->d_name);
                        closedir(dir);                        
                } else {
                        //start listing all the items in the dir
                        data->message=(char*)calloc(dirSize*(550+locLen)+2500,sizeof(char));
                        if(data->message==NULL)
                        {
                                error("can't allocate location memory",data);
                                return -1;
                        }
                        char buf[5000];
                        // char* msg = data->message;
                        if(headIndic)
                                sprintf(buf,"<HTML><HEAD><TITLE>Index of /</TITLE></HEAD><BODY><H4>Index of /</H4><table CELLSPACING=8><tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");                                
                        else
                        {
                                sprintf(buf,"<HTML><HEAD><TITLE>Index of %s</TITLE></HEAD><BODY><H4>Index of %s</H4><table CELLSPACING=8><tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n",loc,loc);
                        }
                        strcpy(data->message,buf);
                        // char loc1[20000];
                        char* loc1 = (char*)calloc(600+locLen,sizeof(char));
                        if(loc1==NULL)
                        {
                                error("can't allocate location memory",data);
                                closedir(dir);
                                return -1;
                        }
                        // if( (strcmp("",loc) == 0) )
                        char* loc2 = (char*)calloc(locLen+100,sizeof(char));
                        strcpy2(loc2,loc);
                        DIR *dir1 = opendir(loc2);
                        free(loc2);
                        // dentry1= { 0 };
                        struct stat fs;
                        char buf3[5000];
                        for(int j=0;j<dirSize;j++)
                        {
                                struct dirent* dentry1=readdir(dir1);//move onto next file in order
                                if(dentry1==NULL)
                                        continue;
                                // if(headIndic || (strcmp(dentry1->d_name,".") == 0) )
                                // {
                                //         if( (strcmp(dentry1->d_name,".") == 0) ||
                                //          (strcmp(dentry1->d_name,"..") == 0) ||
                                //           (strcmp(dentry1->d_name,"headIndic") == 0))
                                //           continue;
                                // }
                                memset(buf,0,5000);
                                memset(buf3,0,5000);
                                // memset(loc1,0,600+locLen+3);
                                if(strcmp(dentry1->d_name,"..") == 0)
                                        sprintf(loc1,"%s",dentry1->d_name);                                
                                else
                                        sprintf(loc1,"%s%s",loc,dentry1->d_name);
                                int noPermission=0;                            
                                if( (stat(loc1,&fs)) < 0)
                                {
                                        if(errno == EACCES)
                                                noPermission=1;
                                        else
                                                continue;
                                }
                                char* flname = convertFromSpaces( ((char*)(dentry1->d_name)) );
                                {
                                        time_t t = fs.st_mtime;
                                        struct tm lt;
                                        localtime_r(&t, &lt);
                                        char timebuffer[100];
                                        strftime(timebuffer, sizeof(timebuffer), RFC1123FMT, &lt);
                                        if( (S_ISDIR(fs.st_mode)) )
                                                strcat(flname,"/");
                                        sprintf(buf,"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>",flname,dentry1->d_name,timebuffer);
                                }
                                
                                strcat(buf3,buf);
                                free(flname);
                                if(!(S_ISDIR(fs.st_mode)) && !noPermission)
                                {
                                        sprintf(buf,"%ld",fs.st_size);
                                        strcat(buf3,buf);                                        
                                }
                                sprintf(buf,"</td></tr>\r\n");
                                strcat(buf3,buf);
                                // printf("%s\n*****************\n",buf3);
                                strcat(data->message,buf3);
                                memset(loc1,0,locLen+600);

                        }
                        sprintf(buf,"</table><HR><ADDRESS>webserver/1.0</ADDRESS></BODY></HTML>\r\n\r\n");
                        strcat(data->message,buf);
                        // printf("%s\n",data->message);
                        closedir(dir);
                        closedir(dir1);
                        free(loc);
                        free(loc1);
                        
                        // int* (data->messageSize = &(data->messageSize)7;
                        // closedir(dir1);

                        return 2;
                }
        }
        
        //if the system reached here, he's likely looking for a file
        int retVal=2;
        struct stat fs;
        char* preFilename = (char*) calloc (locLen+300,sizeof(char));
        if(preFilename==NULL)
        {
                error("cannot allocate filename memory",data);
                return -1;
        }
        strcpy(preFilename,loc);
        char* filename = convertToSpaces(preFilename);
        free(preFilename);
        if(indexExists)
                strcat(filename,"index.html");
        
        if(stat(filename,&fs) >= 0)
        {
                if(! (S_ISDIR(fs.st_mode)))
                {
                        //find out whether this is a known filetype
                        char *ext = strrchr(data->header, '.');
                        if(findKnownType(ext)<0)
                        {
                                sprintf(data->errorVar[0],"application/octet-stream");
                        }
                        data->messageSize=fs.st_size;
                        FILE* file = fopen(filename, "r");
                        data->filep=file;
                        data->fAlloc=1;
                } else {retVal=302;}
        } 
        else { 
                switch(errno){
                        case EACCES:
                                retVal=403;
                                break;
                        case ENOENT:
                                retVal=404;
                                break;
                        default:
                                retVal=500;
                                break;
                }
                
        }

       free(loc);
       free(filename);
       return retVal;
}


//fill the return http & the header
int fillHeader(int errorcode, header_cont* vars)
{
        //initialize the buf array
        int sizeOfMessage = strlen2(vars->message);
        if(vars->filep!=NULL)
                sizeOfMessage=vars->messageSize+1;
        if(vars->toPrint==NULL)
                vars->toPrint=(char*)calloc(5000+sizeOfMessage,sizeof(char));
        if(vars->toPrint == NULL)
        {
                error("Cannot allocate memory for printing",vars);
                return -1;
        }
        char* buf = vars->toPrint;
        // printf("%s\n",vars->message);


        char date[2000];
        char buff[1000];
        time_t now;
        now=time(NULL);
        strftime(date, sizeof(date), RFC1123FMT, gmtime(&now));
        char* codeword = codeWord(errorcode);
        sprintf(buf,"HTTP/1.1 %s\r\nServer: webserver/1.0\r\n",codeword);
        sprintf(buff,"Date: %s\r\n",date);
        strcat(buf,buff);
        if(errorcode == 302)
        {
                sprintf(buff,"Location %s\r\n",vars->errorVar[1]);
                strcat(buf,buff);
        }
        if(errorcode>3)
        {
                sprintf(vars->errorVar[0],"text/html");
        }
        sprintf(buff,"Content-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",vars->errorVar[0],sizeOfMessage);
        strcat(buf,buff);
        if(vars->filep==NULL)
        {
                strcat(buf,vars->message);
        }
        int j=0;/*0*/
        for(;buf[j]!='\0';j++);
        vars->printSize=j+1;

        free(codeword);
        
        return 0;
}

int writeToClient(header_cont *data)
{
        // printf("%s\n",data->toPrint);
        int fd=data->fd, printSize=strlen2(data->toPrint);
        int headS=0,fileS=0;
        char* tp = data->toPrint;

        char toEnd[4]="\r\n\r\n";
        // printf("%s",tp);
        if((headS = write(fd, tp, printSize-1)) < 0)
        {
                error("Couldn't send data to client",data);
                return -1;
        }
        if(data->fAlloc)
        {
                unsigned char buf[1000];
                memset(buf,0,1000);
                int read=0;
                int fileT = 0;
                while( (read = fread(buf,sizeof(char),1000,data->filep)) > 0)
                {
                        if( (fileT = write(fd,buf,read)) < 0)
                        {
                                error("Cannot write file to client!",data);
                                return -1;
                        }
                        fileS+=fileT;
                }
                if(read<0)
                {
                        error("Couldn't write to client",data);
                        return -1;
                }
        }
        if(headS > 0)
                write(fd,toEnd,4);
        // printf("Client FD:\t%d\nWrote header of size:\t%d\nWrote file of size:\t%d\n",fd,headS,fileS);

        return 0;
}

void freeFromClient(header_cont *data)
{
        for(int i=0;i<data->numOfArgs;i++)
        {
                // if(data->errorVar[i] != NULL)
                        if(data->errorVar[i]!=NULL)
                                free((data->errorVar[i]));
        }
        free(data->errorVar);
        if(data->filep != NULL)
                fclose(data->filep);
        free(data->toPrint);
        free(data->message);
        free(data->header);
        close(data->fd);


        free(data);
        return;
}
   

/*************************
    SECONDARY OPERATIONS
**************************/


//code words for HTTP header
/** codes  dictionary
 * 200 - ok
 * 302 - found response(folder found, no '/' in end of path)
 * 400 - bad request(no 3 words before linebreak)
 * 403 - forbidden (no read permission)
 * 404 - not found (no dir/file found)
 * 500 - Internal Server Error
 * 501 - Not supported (for http/1.0)
 * 555 - check whether or not directory exists
 */
char *codeWord(int code)
{
        char* buf = (char*) malloc(200*sizeof(char));
        switch(code) 
        {
        case 200 :
                sprintf(buf,"200 OK");
                break;
        case 302 :
                sprintf(buf,"302 Found");
                break;
        case 400 :// bad request
                sprintf(buf,"400 Bad Request");
                break;
        case 403 ://access denied
                sprintf(buf,"403 Forbidden");
                break;
        case 404 ://404
                sprintf(buf,"404 Not Found");
                break;
        case 500 ://internal server error
                sprintf(buf,"500 Internal Server Error");
                break;
        case 501://method not supported
                sprintf(buf,"501 Not supported");
                break;
        case 1://in case of successful file finding, before writing to buffer
                sprintf(buf,"200 OK");
                break;
        case 2://dir contents
                sprintf(buf,"200 OK");
                break;
        }
    return buf;

}
//print error message, break line and exit
void error(char* msg,void* data)
{
        char* errorMSG = (char*)calloc(strlen2(msg)+2,sizeof(char));
        sprintf(errorMSG,"%s\n",msg);
        free(errorMSG);
        if(data!=NULL)
                freeFromClient(data);
        return;
}
//return the extension of the page if one exists
char *get_mime_type(char *name)
{
        char *ext1 = strrchr(name, '.');
        if(ext1==NULL)
        {
                char* retval = (char*)calloc(100,sizeof(char));
                strcpy2(retval,"text/html");
                return retval;
        }
        char *ext = (char*)calloc(strlen2(ext1),sizeof(char));
        if(ext==NULL)
        {
                error("Cannot allocate extensdion-determining memory",NULL);
                return NULL;
        }
        strcpy(ext,ext1);
        for(int i=0;ext[i]!='\0';i++)
                ext[i]=tolower(ext[i]);

        char* retval = (char*)calloc(100,sizeof(char));
        if (!ext) strcpy2(retval,NULL);
        else if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) strcpy2(retval,"text/html");
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) strcpy2(retval,"image/jpeg");
        else if (strcmp(ext, ".gelse if") == 0) strcpy2(retval,"image/gelse if");
        else if (strcmp(ext, ".png") == 0) strcpy2(retval,"image/png");
        else if (strcmp(ext, ".css") == 0) strcpy2(retval,"text/css");
        else if (strcmp(ext, ".au") == 0) strcpy2(retval,"audio/basic");
        else if (strcmp(ext, ".wav") == 0) strcpy2(retval,"audio/wav");
        else if (strcmp(ext, ".avi") == 0) strcpy2(retval,"video/x-msvideo");
        else if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) strcpy2(retval,"video/mpeg");
        else if (strcmp(ext, ".mp3") == 0) strcpy2(retval,"audio/mpeg");
        else if (strcmp(ext, ".txt") == 0) strcpy2(retval,"text/plain");
        else strcpy2(retval,"text/html");

        free(ext);
        return retval;
}
int findKnownType(char* ext)
{
        if(ext==NULL)
                return -1;
        int retVal = -1;
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0 ||
        (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) ||
        (strcmp(ext, ".gelse if") == 0) ||(strcmp(ext, ".png") == 0) ||
        (strcmp(ext, ".css") == 0) || (strcmp(ext, ".au") == 0) ||
        (strcmp(ext, ".wav") == 0) || (strcmp(ext, ".avi") == 0) ||
        (strcmp(ext, ".mpeg") == 0) || (strcmp(ext, ".mpg") == 0) ||
        (strcmp(ext, ".mp3") == 0) || (strcmp(ext, ".txt") == 0) )
                retVal=1;

        return retVal;
}


int strlen2(char* data)
{
        if(data==NULL) return 0;
        int toReturn=0;
        for(;data[toReturn]!='\0';toReturn++);
        return toReturn+1;
}
void strcpy2(char* a, char* b)
{
        if(b==NULL)
        {
                a[0]='\0';
        } else {
                for(int i=0;b[i]!='\0';i++)
                        a[i]=b[i];
        }
        return;
}

char* convertFromSpaces(char* loc1)
{
        int numOfSpaces=0,i=0;
        for(;loc1[i]!='\0';i++)
        {
                if(loc1[i]==' ')
                        numOfSpaces++;                        
        }
        int z = i+(numOfSpaces*3)+100;
        char* toReturn = (char*)calloc(z,sizeof(char));
        
        char zeroChar = '0';
        for(int j=0,k=0;k<(z-1);j++,k++)
        {
                if(loc1[j]=='\0')
                        break;
                if(loc1[j]!=' ')
                {
                        toReturn[k]=loc1[j];
                } else {
                        toReturn[k]='%';
                        k++;
                        toReturn[k]='2';
                        k++;
                        toReturn[k]=zeroChar;
                }
        }

        return toReturn;
}

char* convertToSpaces(char* loc1)
{

        char* toReturn;
        int i=0,len=strlen2(loc1),k=0;
        toReturn=(char*)calloc(len+150,sizeof(char));

        for(;i<len;i++,k++)
        {
                if(loc1[i] == '\0')
                        continue;
                
                if((i<len-3) && ( loc1[i]=='%' ) && ( loc1[i+1]=='2' ) &&( loc1[i+2]=='0' ) )
                {
                        toReturn[k]=' ';
                        i+=2;
                }
                else
                        toReturn[k]=loc1[i];
        }


        return toReturn;
}


int fileParser(char* loc1)
{
    int toReturn = 0;
    char* loc=convertToSpaces(loc1);
    char* token = strtok(loc, "/");
    struct stat fs;
    if(( (stat(token,&fs)) < 0))
    {
        free(loc);
        switch(errno){
            case EACCES:
                return -403;
            case ENOENT:
                return -404;
        }
    }
    char* loc2 = (char*) calloc (strlen2(loc1)+10,sizeof(char));
    strcpy2(loc2,token);
    int problem = 0,noAccess=0;
    while( (token = strtok(NULL, "/")) != NULL)
    {
        strcat(loc2,"/");
        strcat(loc2,token);
        if(stat(loc2,&fs) < 0)
        {
            problem=1;
            break;
        }
        if(! (fs.st_mode & S_IXOTH))
        {
            noAccess=1;
            break;
        }
    }
    if(problem)
    {
        switch(errno){
            case EACCES:
                toReturn=-403;
                break;
            case ENOENT:
                toReturn=-404;
                break;
            default:
                toReturn=500;
                break;
        }
    } 
    else if(noAccess){
        toReturn=-403;
    }
    else if( (stat(loc2,&fs)) < 0)
    {//check if final location is accessible for us
         switch(errno){
            case EACCES:
                toReturn=-403;
                break;
            case ENOENT:
                toReturn=-404;
                break;
            default:
                toReturn=500;   
                break;
        }
    } else {
        if(!(fs.st_mode & S_IROTH))
            toReturn=-403;
    }

    

    free(loc);
    free(loc2);
    return toReturn;
}



/**************************
           MAIN
**************************/

int main(int argc, char* argv[])
{
        if(argc!=4)
        {
                printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
                return 1;
        }
        int port=atoi(argv[1]);
        int poolSize = atoi(argv[2]);
        int backlog = atoi(argv[3]);
        if(port <=0 || backlog<=0 || poolSize<=0)
        {
                printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
                return 1;
        }
		if(poolSize>MAXT_IN_POOL || poolSize<1)
		{
			printf("Error: maximum poolsize %d, minimum poolsize 1",MAXT_IN_POOL);
			return 1;
		}




        //connect server to net
        // char buff[256];
        struct sockaddr_in serv_addr={ 0 };
        struct sockaddr cli_addr= { 0 };
        socklen_t clilen=0;

        memset(&cli_addr,0,sizeof(cli_addr));




        int sockfd = socket(AF_INET,SOCK_STREAM,0);
        if(sockfd < 0)
        {
                error("ERROR opening socket",NULL);
                return 1;
        }
        serv_addr.sin_family=AF_INET;
        serv_addr.sin_addr.s_addr=INADDR_ANY;
        serv_addr.sin_port=htons(/*atoi(port)*/port);

        if( bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
                error("ERROR on binding",NULL);
                return 1;
        }
        listen(sockfd,backlog);

        threadpool* pool = create_threadpool(poolSize);
        if(pool==NULL)
        {
                printf("Cannot create threadpool\n");
                close(sockfd);
                return 1;
        }
        
        for(int i=0;i<backlog;i++)
        {
                int newsockfd=accept(sockfd,&cli_addr,&clilen);
                if(newsockfd<0)
                {
                        error("ERROR on accept",NULL);
                        return 1;
                }
                /*how to build header_cont tutorial*/
                header_cont* a = (header_cont*)calloc(1,sizeof(header_cont));
                if(a==NULL)
                {
                        printf("Cannot create thread");
                        exit(1);
                }
                a->fd=newsockfd;
                a->errorVar=(char**)calloc(2,sizeof(char*));
                a->errorVar[0]=(char*)calloc(50,sizeof(char));
                a->numOfArgs++;
                if(a->errorVar==NULL)
                {
                        printf("Cannot create thread");
                        exit(1);
                }
                dispatch(pool,work,a);
        }

        close(sockfd);
        destroy_threadpool(pool);
        


        return 0;
}