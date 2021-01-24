/*
    g++ tokenizer.cpp -fopenmp -o a.out -std=c++0x
    ./a.out 8 keywords.txt files 

    input: keyword file name, directory name 
    output: the frequency of each keyword
    producer: read each line of files in the directory
    consumer: tokenize, calculate the frequency of each keyword 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>           // define the max 
#include <iostream>
#include <vector>
using namespace std;

#define MAX_FILES 100
#pragma GCC diagnostic ignored "-Wwrite-strings"

struct list_node
{
    char* data; 
    struct list_node* next; 
};

void enqueue(char* line, struct list_node** queue_head, struct list_node** queue_tail, int id)
{
    struct list_node* tmp = NULL; 
    tmp = (struct list_node *) malloc(sizeof(struct list_node)); 
    tmp->data = line; 
    tmp->next = NULL;               // prepare the data

#   pragma omp critical 
    {
        if (*queue_tail == NULL) {
            *queue_head = tmp;      // the queue is empty, tmp is the first element
            *queue_tail = tmp; 
        }
        else {
            (*queue_tail)->next = tmp;         
            *queue_tail = tmp; 
        }
    }
    
}

struct list_node* dequeue(struct list_node** queue_head, struct list_node** queue_tail, int id)
{
    struct list_node* tmp = NULL; 

    if (*queue_head == NULL)        // empty 
        return NULL; 

#   pragma omp critical
    {
        if (*queue_head == *queue_tail && *queue_head != NULL) 
            *queue_tail = (*queue_tail)->next;      

        if (*queue_head != NULL) {
            tmp = *queue_head; 
            *queue_head = (*queue_head)->next;      
        }
    }

    return tmp; 
}

void read_file(FILE* fp, struct list_node** queue_head, struct list_node** queue_tail, int id)
{
    char* line = (char *) malloc(sizeof(char) * CHAR_MAX); 

    // read the contents (unit: line) of the file, then push the data into the shared queue 
    while (fgets(line, CHAR_MAX, fp) != NULL)
    {
        enqueue(line, queue_head, queue_tail, id); 
        line = (char *) malloc(sizeof(char) * CHAR_MAX); 
    }

    fclose(fp); 
}

void tokenize(char* line, vector<char*> kw, int* table, int id)
{
    // use thread-safe function strtok_r() rather than strtok() 
    char* tmp = NULL; 
    tmp = (char *) malloc(sizeof(char) * CHAR_MAX);

    memcpy(tmp, line, strlen(line) + 1);
    char *token, *saveptr;

    token = strtok_r(tmp, " ", &saveptr);

    while (token != NULL)
    {
        int i;
        // compare to the keywords, if matches, add the count in the table (should be atomic)
        for (i = 0; i < kw.size(); i++) {
            if (strcmp(token, kw.at(i)) == 0) {
                #pragma omp atomic
                table[i]++;
            }
        }

        token = strtok_r(NULL, " ", &saveptr);
    }
}

void prod_cons(int prod_count, int cons_count, FILE* files[], int file_count, vector<char*> kw, int table[])
{
    // function for the producer and consumer threads 
    int thread_count = prod_count + cons_count; 
    int prod_done = 0;
    struct list_node* queue_head = NULL;        // maintain the shared queue 
    struct list_node* queue_tail = NULL; 

#   pragma omp parallel num_threads(thread_count) default (none) \
    shared(files, file_count, prod_count, cons_count, prod_done, queue_head, queue_tail, kw, table)
    {
        int i; 
        int id = omp_get_thread_num(); 

        if (id < prod_count)
        {
            // producer: read the file via the file pointer  
            for (i = id; i < file_count; i += prod_count)
                read_file(files[i], &queue_head, &queue_tail, id);

#           pragma omp atomic
            prod_done++;
        }
        else
        {
            // consumer: tokenzie and calculate the frequency of keyword 
            struct list_node* tmp = NULL; 
            while (prod_done < prod_count)                              // producer is still running 
            {
                tmp = dequeue(&queue_head, &queue_tail, id);
                if (tmp != NULL)
                    tokenize(tmp->data, kw, table, id);
            }

            while (queue_head != NULL)                                  // process the left contents 
            {
                tmp = dequeue(&queue_head, &queue_tail, id);
                if (tmp != NULL)
                    tokenize(tmp->data, kw, table, id);
            }
        }
    }
}

char* concat_1(char *s1, char *s2)
{
    char* result = (char*) malloc(strlen(s1) + strlen(s2)); 
    strcpy(result, s1); 
    strcat(result, s2); 
    return result; 
}

char* concat_2(char *s1, char *s2)
{
    char* result = (char*) malloc(strlen(s1) + strlen(s2) + 1); 
    strcpy(result, s1); 
    strcat(result, s2); 
    return result; 
}

int main(int argc, char** argv)
{
    int i, prod_count, cons_count, file_count;

    char* dirname;
    char* kw_name; 

    prod_count = 3; 
    cons_count = atoi(argv[1]) - cons_count; 
    kw_name = (char *)argv[2]; 
    dirname = (char *)argv[3]; 

    vector<char*> kw; 
    int table[100] = {0}; 

    FILE* fp = fopen(kw_name, "r"); 
    char* buf = (char *) malloc(sizeof(char) * CHAR_MAX); 
    if (fgets(buf, CHAR_MAX, fp) == NULL) {
        perror("fgets"); 
        exit(1); 
    }

    char* token = strtok(buf, " ");
    while (token) {
        kw.push_back(token); 
        token = strtok(NULL, " ");
    }

    // open the directory
    DIR* dir = opendir((const char*)dirname);
    if (dir == NULL) {
        perror("opendir");
        exit(1);
    }

    // get filenames
    struct dirent *entry;
    char** filenames[MAX_FILES] = {0}; 
    char* s1 = concat_1("./", (char *)dirname); 

    i = 0; 
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {

            char* s2 = concat_1("/", entry->d_name); 
            filenames[i] = (char **) concat_2(s1, s2); 
            i++; 
        }
    }
    closedir(dir); 
    file_count = i; 

    FILE* files[MAX_FILES] = {0};
    for (i = 0; i < file_count; i++) {
        files[i] = (FILE *) fopen((const char *) filenames[i], (const char *)"r"); 
        if (files[i] == NULL)
        {
            perror("fopen"); 
            exit(1); 
        }
    }   

    prod_cons(prod_count, cons_count, files, file_count, kw, table);    

    for (i = 0; i < kw.size(); i++)
         printf("[info] keyword = %s, count = %d\n", kw.at(i), *(table + i));

    return 0;
}