#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "meeting_request_formats.h"
#include "queue_ids.h"
#include <stdbool.h>
#include "rbtree.h"
#include <string.h>
#include <signal.h>
#include "common.h"

#define INT_STR_MAXL 11

int msqid;
int msgflg = IPC_CREAT | 0666;
key_t key;

//could easily pass in as parameters but eh
pthread_mutex_t search;
pthread_mutex_t send;
pthread_mutex_t mut_end;
pthread_cond_t cond_end;
int num_left;

//for red-black tree
struct node* root;

//request thread arguments
//could've done without the pointers. but style i guess?
typedef struct {
    meeting_request_buf mrb;
    pthread_cond_t *rdy;
    pthread_mutex_t *mut;
    meeting_response_buf *res;
} rq_arg;

// SIGINT handler
//since isn't requirement, I make it a one time use so you can still cancel program
void SIGINT_hand(int sig)
{
    int num;
    Pthread_mutex_lock(&mut_end);
    num=num_left;
    Pthread_mutex_unlock(&mut_end);
    fprintf(stdout,"Number of outstanding requests: %d\n",num);
    signal(SIGINT, SIG_DFL);
}

//Thread to send requests, and wait for receive.
void* request_receive (void *arg){
    rq_arg *qargs = (rq_arg *) arg;
    meeting_request_buf *args = &(qargs->mrb);
    
    //wait for other request threads to exit before sending 0
    if(args->request_id==0){
        Pthread_mutex_lock(&mut_end);
        while( num_left>1) {
            pthread_cond_wait(&cond_end, &mut_end);
        }
        Pthread_mutex_unlock(&mut_end);
    }

    //on bigger inputs, without while, program doesn't send because of IPC_NOWAIT flag
    //it is a while loop in a lock but if it can't send the others can't as well
    Pthread_mutex_lock(&send);
    while((msgsnd(msqid, args, SEND_BUFFER_LENGTH, IPC_NOWAIT)) < 0) {}//message successfully sent
    Pthread_mutex_unlock(&send);

    //wait response
    int a;
    Pthread_mutex_lock(qargs->mut);
    while(qargs->res==NULL){
        Pthread_cond_wait(qargs->rdy,qargs->mut);
    }
    a = qargs->res->avail;
    Pthread_mutex_unlock(qargs->mut);

    
    //print result
    if(args->request_id!=0){
        if(a==1){
            //removes quotes from description and location if exists. 
            if(args->description_string[0]=='"'){
                int i=1;
                while(args->description_string[i]!='"' && args->description_string[i]!=0){
                    args->description_string[i-1] = args->description_string[i];
                    ++i;
                }
                args->description_string[--i]=0;
            }
            if(args->location_string[0]=='"'){
                int i=1;
                while(args->location_string[i]!='"' && args->location_string[i]!=0){
                    args->location_string[i-1] = args->location_string[i];
                    ++i;
                }
                args->location_string[--i]=0;
            }
            fprintf(stdout, "Meeting request %d for employee %s was accepted (%s @ %s starting %s for %d minutes)\n",args->request_id, args->empId, args->description_string, args->location_string, args->datetime, args->duration);
        }else if (a==0){
            if(args->description_string[0]=='"'){
                int i=1;
                while(args->description_string[i]!='"' && args->description_string[i]!=0){
                    args->description_string[i-1] = args->description_string[i];
                    ++i;
                }
                args->description_string[--i]=0;
            }
            if(args->location_string[0]=='"'){
                int i=1;
                while(args->location_string[i]!='"' && args->location_string[i]!=0){
                    args->location_string[i-1] = args->location_string[i];
                    ++i;
                }
                args->location_string[--i]=0;
            }
            fprintf(stdout, "Meeting request %d for employee %s was rejected due to conflict (%s @ %s starting %s for %d minutes)\n",args->request_id, args->empId, args->description_string, args->location_string, args->datetime, args->duration);
        }else {
            fprintf(stdout, "\n");
        }
    }

    //destroy and deallocate personal thread mutexs, condition variables
    Pthread_mutex_destroy(qargs->mut);
    Pthread_cond_destroy(qargs->rdy);
    free(qargs->rdy);
    free(qargs->mut);
    free(qargs->res);
    free(qargs);

    //num left for sigint handler
    Pthread_mutex_lock(&mut_end);
    num_left--;
    Pthread_mutex_unlock(&mut_end);

    return NULL;
}

void* response_receive(void *arg){
    /* Thread to distribute responses to corresponding request thread
    */
    int ret;
    bool endThread=false;
    while(1){
        //request thread frees own response buffer
        meeting_response_buf *rbuf = malloc(sizeof(meeting_response_buf));
        if(rbuf==NULL){
            fprintf(stderr,"malloc unsuccessful");
            exit(1);
        }
        do {
        
        ret = msgrcv(msqid, rbuf, sizeof(meeting_response_buf)-sizeof(long), 1, 0);//receive type 1 message
        int errnum = errno;
        if (ret < 0 && errno !=EINTR){
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("Error printed by perror");
            fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
        }
        } while ((ret < 0 ) && (errno == 4));

        //traverse rbtree for request_id to find thread to wake.
        Pthread_mutex_lock(&search);
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
        //add data and signal
        if(trav!=NULL){
            Pthread_mutex_lock(trav->mut);
            //link response to thread.
            *trav->res=rbuf;
            //kill when 0 response
            if(rbuf->request_id==0){
                endThread=true;
            }
            Pthread_cond_signal(trav->rdy);
            Pthread_mutex_unlock(trav->mut);
        }
        Pthread_mutex_unlock(&search);

        
        if(endThread) break;
    }
    //fprintf(stderr,"exit reponse");
    return NULL;
}

int main(int argc, char *argv[]){
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

    pthread_t req_threads[200];
    root = NULL;
    Pthread_mutex_init(&search);
    Pthread_mutex_init(&send);
    Pthread_mutex_init(&mut_end);
    Pthread_cond_init(&cond_end);

    num_left=0;
    int thread_count=0;

    size_t INPUT_SZ = INT_STR_MAXL + EMP_ID_MAX_LENGTH + DESCRIPTION_MAX_LENGTH + LOCATION_MAX_LENGTH + DATETIME_LENGTH + INT_STR_MAXL + 1;
    char *input = (char *)malloc(INPUT_SZ * sizeof(char));
    if(input==NULL){
        printf("Unable to alloc string for input\n");
        return 1;
    }
    signal(SIGINT, SIGINT_hand);
    //run thread handling responses
    pthread_t responsethread;
    Pthread_create(&responsethread, NULL, response_receive, NULL);

    while(1){
        int gstop = getline(&input,&INPUT_SZ,stdin);
        rq_arg *rarg = malloc(sizeof(rq_arg));
        if(rarg==NULL){
            fprintf(stderr,"malloc unsuccessful");
        }

        //feed input into delim array by comma. allows for commas in quotes.
        //RFC 4180 
        char delim[6][40];
        int i,a=0,b=0;
        char quotechar;
        bool outquote=true;
        for(i=0;i<gstop;++i){
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
        delim[a][b]=0;

        //create argument for request thread
        rarg->mrb.mtype = 2;
        rarg->mrb.request_id=atoi(delim[0]);
        strncpy(rarg->mrb.empId,delim[1],EMP_ID_MAX_LENGTH);
        strncpy(rarg->mrb.description_string,delim[2],DESCRIPTION_MAX_LENGTH);
        strncpy(rarg->mrb.location_string,delim[3],LOCATION_MAX_LENGTH);
        strncpy(rarg->mrb.datetime,delim[4],DATETIME_LENGTH);
        rarg->mrb.duration=atoi(delim[5]);
        
        //make lock and condition variable for storage
        pthread_mutex_t* pmutex = malloc(sizeof(pthread_mutex_t));
        if(pmutex==NULL){
            fprintf(stderr,"malloc unsuccessful");
        }
        Pthread_mutex_init(pmutex);
        pthread_cond_t* pcond = malloc(sizeof(pthread_cond_t));
        if(pcond==NULL){
            fprintf(stderr,"malloc unsuccessful");
        }
        Pthread_cond_init(pcond);

        //add to argument
        rarg->mut=pmutex;
        rarg->rdy=pcond;
        rarg->res=NULL;

        //add mutex, CV, v to RB tree
        struct node* temp = malloc(sizeof(struct node));
        temp->r = NULL;
        temp->l = NULL;
        temp->p = NULL;
        temp->c=1;

        temp->d = rarg->mrb.request_id;
        temp->res = &rarg->res;
        temp->rdy = pcond;
        temp->mut = pmutex;

        Pthread_mutex_lock(&mut_end);
        Pthread_create(&req_threads[thread_count++],NULL, request_receive, rarg);
        num_left++;
        Pthread_mutex_unlock(&mut_end);

        Pthread_mutex_lock(&search);
        // if there are duplicate ids, the rbtree breaks. the input.msg given had duplicate ids but the one shown in assignment.md didn't
        if(!(root==NULL)){
            struct node* trav=root;
            while(trav!=NULL){
                if(trav->d < temp->d){
                    trav=trav->r;
                }else if(trav->d > temp->d){
                    trav=trav->l;
                }else{
                    break;
                }
            }
            if(trav!=NULL){
                printf("Duplicate ids, bad input. (please clean queue)\n");
                exit(1);
            }
        }
        root=bst(root, temp);
        fixup(root,temp);
        Pthread_mutex_unlock(&search);

        if(rarg->mrb.request_id==0){
            /*
                request_id 0 will be last input, break to close input loop;
            */
            break;
        }
    }

    //join all threads
    int i;
    for(i=0;i<thread_count;++i){
        //fprintf(stderr, "waiting %d\n",i);
        Pthread_join(req_threads[i], NULL);
        //if waiting on request_id 0, end it
        Pthread_mutex_lock(&mut_end);
        if(num_left==1){
            num_left--;
            Pthread_cond_signal(&cond_end);
        }
        Pthread_mutex_unlock(&mut_end);
    }
    Pthread_join(responsethread, NULL);

    free(input);
    Pthread_mutex_destroy(&send);
    Pthread_mutex_destroy(&search);
    Pthread_cond_destroy(&cond_end);
    Pthread_mutex_destroy(&mut_end);
}