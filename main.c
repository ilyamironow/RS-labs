#include <unistd.h>
#include <stdio.h>
#include <wait.h>
#include <malloc.h>

#include "ipc.h"
#include "common.h"
#include "pa1.h"
#include <time.h>

struct self_structure_type {
    int i; // current process's number
    int N; // number of processes
    int ****fd;
    int *p;
};

int send_multicast(void *self, const Message *msg) {
    // self should be the data structure that has current i and N
    // void * to data structure
    for (int i = 0; i < (*(struct self_structure_type *) self).N; i++) {
        if ((*(struct self_structure_type *) self).i == i)
            continue;
        printf("%d from %d to %d\n", write((*((*(struct self_structure_type *) self).fd))[(*(struct self_structure_type *) self).i][i][1], msg,
              sizeof(Message)), (*(struct self_structure_type *) self).i, i);
    }
    return 0;
}

int receive_any(void *self, Message *msg) {
    for (int i = 1; i < (*(struct self_structure_type *) self).N; i++) {
        if ((*(struct self_structure_type *) self).i == i)
            continue;
        waitpid(((*(struct self_structure_type *) self).p)[i], NULL, NULL);
        Message process_msg;

        if (read((*((*(struct self_structure_type *) self).fd))[i][(*(struct self_structure_type *) self).i][0],
                 &process_msg, sizeof(Message)) == -1)
            printf("%d process didn't send enum №%d to %d process\n", i, msg->s_header.s_type, (*(struct self_structure_type *) self).i);
        else
            printf("good\n");
        //if (process_msg->s_header.s_type == msg->s_header.s_type)
            //printf("%d process send enum №%d to %d process\n", i, msg->s_header.s_type, (*(struct self_structure_type *) self).i);

            //только для родительского
            //два раза enum 0
    }
    return 0;
}

int main() {
    FILE *f;
    f = fopen(events_log, "a+"); // a+ (create + append) option will allow appending which is useful in a log file
    int N;
    printf("Input N from 1 to 10: ");
    scanf("%d", &N);
    N++;

    // fd['from']['to'][0/1]
    int ***fd = (int ***) malloc(N * sizeof(int **));
    for (int i = 0; i < N; i++) {
        fd[i] = (int **) malloc(N * sizeof(int *));

        for (int j = 0; j < N; j++)
            fd[i][j] = (int *) malloc(2 * sizeof(int));
    }

    for (int i = 1; i < N; i++)
        for (int j = 0; j < i; j++)
            pipe(fd[i][j]);
    for (int j = 1; j < N; j++)
        for (int i = 0; i < j; i++)
            pipe(fd[i][j]);

   /* for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            fstat(fd[i][j][0]);
        }*/
// логи открытых изначально дескрипторах в терминал и в файл логов -- доделать


    /*
     * We create the pipe before forking the processes, so they share the pipe fds.
     * If we swap these two lines, both processes create their own pipe,
     * and they have different values, thus they can't communicate using fd!
     */
    int p[N]; //because p[0] is a parent
    for (int i = 1; i < N; i++) {
        p[i] = fork();
        if (p[i] == 0)
            break;
    }

    /* mail cycle */
    for (int i = N - 1; i >= 0; i--)
        if (p[i] == 0 && i != 0) {
            //in child process p[i]

            fprintf(f, log_started_fmt, i, getpid(), getppid()); //* пишет в лог started

            char buffer[50];
            int len;
            len = sprintf(buffer, log_started_fmt, i, getpid(), getppid());

            struct self_structure_type self_structure = {.i = i, .N = N, .fd = &fd, .p = &p};

            for (int j = 0; j < N; j++) {
                if (j == i)
                    continue;
                close(fd[i][j][0]); // close read end for pipe direction from current process
                close(fd[j][i][1]); // close write end for pipe direction to current process
            }
            // отправляет started всем процессам
            MessageHeader s_header = {.s_magic = MESSAGE_MAGIC, .s_payload_len = len, .s_type = STARTED, .s_local_time = (int16_t) time(
                    NULL)};
            Message msg = {.s_header = s_header, .s_payload = buffer};
            send_multicast(&self_structure, &msg);

            // дождаться от других дочерних сообщения STARTED
            receive_any(&self_structure, &msg);
            fprintf(f, log_received_all_started_fmt, i); //* пишет в лог received all started

            fprintf(f, log_done_fmt, i); //* пишет в лог done
            // отправляет DONE всем
            s_header.s_type = DONE;
            len = sprintf(buffer, log_done_fmt, i, getpid(), getppid());
            s_header.s_payload_len = len;
            s_header.s_local_time = (int16_t) time(NULL);
            msg = (Message){.s_header = s_header, .s_payload = buffer};
            send_multicast(&self_structure, &msg);

            // дождаться от других дочерних сообщения done
            receive_any(&self_structure, &msg);
            fprintf(f, log_received_all_done_fmt, i); //* пишет в лог received all started
            //тут и выше пишет received даже если не received

            for (int j = 0; j < N; j++) {
                if (j == i)
                    continue;
                close(fd[i][j][1]); // close read end for pipe direction from current process
                close(fd[j][i][0]); // close write end for pipe direction to current process
            }
            break;
        } else if (i == 0) {
            //in parent process
            /* родительский
             * дождаться от других дочерних сообщения DONE - read
             * wait пока все завершатся и конец
             */
            struct self_structure_type self_structure = {.i = i, .N = N, .fd = &fd, .p = &p};

            for (int j = 1; j < N; j++) {
                close(fd[i][j][0]); // close read end for pipe direction from current process
                close(fd[j][i][1]); // close write end for pipe direction to current process
            }
            MessageHeader s_header = {.s_type = STARTED};
            Message msg = {.s_header = s_header};
            receive_any(&self_structure, &msg);
            fprintf(f, log_received_all_started_fmt, i); //* пишет в лог received all started

            s_header.s_type = DONE;
            msg.s_header = s_header;
            receive_any(&self_structure, &msg);
            fprintf(f, log_received_all_done_fmt, i); //* пишет в лог received all done

            for (int j = 1; j < N; j++) {
                close(fd[i][j][1]); // close read end for pipe direction from current process
                close(fd[j][i][0]); // close write end for pipe direction to current process
            }
            wait(NULL);
            break;
            /*
             * wait for state changes in a child of the calling process
             * state change is considered to be: the child terminated; the child was stopped by a signal;
             * or the child was resumed by a signal.S
             * if a wait is not performed, then the terminated child remains in a "zombie" state
             */
        }

    // deallocate memory
//free верно
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            free(fd[i][j]);
        }
        free(fd[i]);
    }
    free(fd);

    fclose(f);
    return 0;
}
