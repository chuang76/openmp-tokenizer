# Parallel word tokenization with OpenMP

The purpose of this project is to implement a parallel word tokenization of the text input. The design is based on the [producer-consumer problem](https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem), which is a classic example of a multi-process synchronization problem. All of the parallel code sections are implemented with the [OpenMP](https://en.wikipedia.org/wiki/OpenMP) interface. 



## Environment

- openSUSE 11.04
- g++ 4.5.1



## Policy

Given a keyword file that contains several keywords. Use OpenMP to implement a producer-consumer program in which some of the threads are producers and others are consumers. The producers read text from a collection of files, one per producer. They insert lines of text into a single shared queue. The consumers take the lines of text and tokenize them. Tokens are “words” separated by white space. When a consumer finds a token that is the keyword, the keyword count increases one. Please print each keyword and its count.

- prod_cons(): the function is used to divide tasks among threads (act as producer threads or consumer threads)

- enqueue(): to avoid the race condition, we need to protect the single shared queue, that is, use the OpenMP directive `critical` to keep only one producer insert a line of text into the queue per time

  ```c++
  void enqueue(char* line, struct list_node** queue_head, struct list_node** queue_tail)
  {
      struct list_node* tmp = NULL; 
      tmp = (struct list_node *) malloc(sizeof(struct list_node)); 
      tmp->data = line; 
      tmp->next = NULL;              
  
  #   pragma omp critical 
      {
          if (*queue_tail == NULL) {           // empty case
              *queue_head = tmp;      
              *queue_tail = tmp; 
          }
          else {
              (*queue_tail)->next = tmp;       // insert 
              *queue_tail = tmp; 
          }
      }
  }
  ```

- dequeue(): to avoid the race condition, we need to protect the single shared queue, that is, use OpenMP directive `critical` to keep only one consumer take a line of text from the queue per time

  ```c++
  struct list_node* dequeue(struct list_node** queue_head, struct list_node** queue_tail)
  {
      struct list_node* tmp = NULL; 
  
      if (*queue_head == NULL)        // empty case
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
  ```

- tokenize(): to achieve the [thread-safety](https://en.wikipedia.org/wiki/Thread_safety), we use the thread-safe function strtok_r() rather than strtok() to tokenize the text

  ```c++
  char *token, *saveptr;
  token = strtok_r(tmp, " ", &saveptr);
  
  // tokenize 
  while (token != NULL) {
      
      // process the frequency table 
      for (int i = 0; i < kw.size(); i++) {
          if (strcmp(token, kw.at(i)) == 0) {
              #pragma omp atomic
              table[i]++;
          }
      }
      token = strtok_r(NULL, " ", &saveptr);
  }
  ```

- Why the function strtok() is thread-unsafe? The function strtok() uses a static buffer while parsing, which may cause a [cache coherence](https://en.wikipedia.org/wiki/Cache_coherence) problem. If the buffer is overwritten, the result may be incorrect. The source code of strtok() is as follows. 

  ```c
  char * strtok (char *s, const char *delim)
  {
      static char *olds;
    	return __strtok_r (s, delim, &olds);
  }
  ```

- Measure the process time: if you want to measure the process time, please use the function omp_get_wtime(), which returns elapsed wall clock time in seconds.

  

## Usage

- Get a copy of this project by cloning the Git repository.

  ```
  git clone https://github.com/chuang76/openmp-tokenizer.git
  ```

- You can compile the source program `tokenizer.cpp` and run the executable, 

  ```
  g++ tokenizer.cpp -fopenmp -o a.out -std=c++0x
  ./a.out <number_of_threads> <keyword filename> <directory name>
  ```

  or you can just execute the make command. 

  ```
  make
  ```

  

## Example

Here is a short example. The keywords are `hello and is moon`. 

