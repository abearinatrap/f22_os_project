#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "meeting_request_formats.h"
#include "queue_ids.h"
#include <stdbool.h>

#define INT_STR_MAXL 11

int msqid;
int msgflg = IPC_CREAT | 0666;
key_t key;

void* request_receive (void *arg){
    meeting_request_buf *args= (meeting_request_buf *) arg;
    //maybe lock for send???
    if((msgsnd(msqid, args, SEND_BUFFER_LENGTH, IPC_NOWAIT)) < 0) {
        int errnum = errno;
        fprintf(stderr,"%d, %ld, %d, %ld\n", msqid, args->mtype, args->request_id, SEND_BUFFER_LENGTH);
        perror("(msgsnd)");
        fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
        exit(1);
    }
    //message successfully sent

    //should we have one thread that is processing responses in that queue and then wakes corresponding thread?
}

int main(int argc, char *argv[]){
    meeting_request_buf rbuf;
    key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
    if (key == 0xffffffff) {
        fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
        return 1;
    }
    if ((msqid = msgget(key, msgflg)) < 0) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("(msgget)");
        fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
    }

    size_t INPUT_SZ = INT_STR_MAXL + EMP_ID_MAX_LENGTH + DESCRIPTION_MAX_LENGTH + LOCATION_MAX_LENGTH + DATETIME_LENGTH + INT_STR_MAXL + 1;
    char *input = (char *)malloc(INPUT_SZ * sizeof(char));
    if(input==NULL){
        printf("Unable to alloc for string\n");
        return 1;
    }

    while(1){
        getline(input,INPUT_SZ,stdin);
        meeting_request_buf rbuf;

        //feed input into delim array by comma
        char delim[6][40];
        int i,a=0,b=0;
        char quotechar;
        bool outquote=false;
        for(i=0;i<INPUT_SZ;++i){
            if(input[i]==',' && outquote){
                delim[a][b]=0;
                ++a;
                b=0;
                continue;
            }
            delim[a][b++]=input[i];
            if((input[i]=='"'||input[i]=='\'') && outquote){
                quotechar=input[i];
                outquote=false;
            }
            if(input[i]==quotechar && !outquote){
                outquote=true;
            }
        }

        rbuf.mtype = 2;
        rbuf.request_id=atoi(delim[0]);
        strncpy(rbuf.empId,delim[1],EMP_ID_MAX_LENGTH);
        strncpy(rbuf.description_string,delim[2],DESCRIPTION_MAX_LENGTH);
        strncpy(rbuf.location_string,delim[3],LOCATION_MAX_LENGTH);
        strncpy(rbuf.datetime,delim[4],DATETIME_LENGTH);
        rbuf.duration=atoi(delim[5]);
        //send last meeting before breaking

        if(rbuf.request_id==0){
            break;
        }
    }
    

    printf("%s",argv[0]);
    free(input);
}