#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "meeting_request_formats.h"
#include "queue_ids.h"
#include <stdbool.h>
#include <rbtree.h>

#define INT_STR_MAXL 11

int msqid;
int msgflg = IPC_CREAT | 0666;
key_t key;
pthread_mutex_t search;

pthread_t req_threads[200];
int thread_count;

struct node* root;

typedef struct {
    meeting_request_buf mrb;
    pthread_cond_t *rdy;
    pthread_mutex_t *mut;
    meeting_response_buf *res;
} rq_arg;

void* request_receive (void *arg){
    rq_arg *qargs = (rq_arg *) arg;
    meeting_request_buf *args = &(qargs->mrb);
    //meeting_request_buf *args= (meeting_request_buf *) arg;
    //maybe lock for send???
    if((msgsnd(msqid, args, SEND_BUFFER_LENGTH, IPC_NOWAIT)) < 0) {
        int errnum = errno;
        fprintf(stderr,"%d, %ld, %d, %ld\n", msqid, args->mtype, args->request_id, SEND_BUFFER_LENGTH);
        perror("(msgsnd)");
        fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
        exit(1);
    }//message successfully sent

    if(args->request_id!=0){
        pthread_mutex_lock(qargs->mut);
        while(qargs->res==NULL){
            pthread_cond_wait(qargs->rdy,qargs->mut);
        }
        pthread_mutex_unlock(qargs->mut);
    }
    fprintf(stdin, "Meeting request %d for employee %s was rejected due to conflict (%s @ zoom starting %s for %d minutes",args->request_id, args->empId, args->description_string, args->datetime, args->duration);
    //dealloc mutex, condition variable, response buffer
}

void* response_receive(void *arg){
    int ret;
    while(1){
        meeting_response_buf *rbuf = malloc(sizeof(meeting_response_buf));
        do {
        
        ret = msgrcv(msqid, rbuf, sizeof(meeting_response_buf)-sizeof(long), 1, 0);//receive type 1 message

        int errnum = errno;
        if (ret < 0 && errno !=EINTR){
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("Error printed by perror");
            fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
        }
        } while ((ret < 0 ) && (errno == 4));

        //rbuf.request_id,rbuf.avail
        pthread_mutex_lock(&search);
        if(root==NULL) continue;
        struct node* trav=root;
        while(trav!=NULL){
            if(trav->d < rbuf->request_id){
                trav=trav->r;
            }else if(trav->d > rbuf->request_id){
                trav=trav->l;
            }else{
                break;
            }
        }
        pthread_mutex_ulock(&search);

        if(trav!=NULL){
            pthread_mutex_lock(trav->mut);
            trav->res=rbuf;
            pthread_cond_signal(trav->rdy);
            pthread_mutex_unlock(trav->mut);
        }
        
    }
    
}

int main(int argc, char *argv[]){
    root = NULL;
    pthread_mutex_init(&search, NULL);
    thread_count=0;

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
        rq_arg rarg;

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

        rarg.mrb.mtype = 2;
        rarg.mrb.request_id=atoi(delim[0]);
        strncpy(rarg.mrb.empId,delim[1],EMP_ID_MAX_LENGTH);
        strncpy(rarg.mrb.description_string,delim[2],DESCRIPTION_MAX_LENGTH);
        strncpy(rarg.mrb.location_string,delim[3],LOCATION_MAX_LENGTH);
        strncpy(rarg.mrb.datetime,delim[4],DATETIME_LENGTH);
        rarg.mrb.duration=atoi(delim[5]);
        //send last meeting before breaking

        //make lock and condition variable for storage
        pthread_mutex_t* pmutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(pmutex, NULL);
        pthread_cond_t* pcond = malloc(sizeof(pthread_cond_t));
        pthread_cond_init(pcond, NULL);

        struct node* temp = malloc(sizeof(struct node));
        temp->r = NULL;
        temp->l = NULL;
        temp->p = NULL;
        temp->c=1;

        temp->d = rarg.mrb.request_id;
        temp->res = NULL;
        temp->rdy = pcond;
        temp->mut = pmutex;

        pthread_create(&req_threads[thread_count++],NULL, request_receive, &rarg);

        pthread_mutex_lock(&search);
        root=bst(root, temp);
        fixup(root,temp);
        pthread_mutex_unlock(&search);

        if(rarg.mrb.request_id==0){
            break;
        }
    }
    

    printf("%s",argv[0]);
    free(input);
}