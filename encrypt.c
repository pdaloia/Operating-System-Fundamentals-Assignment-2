/*

# Family Name: D'Aloia

# Given Name: Philip

# Section: E

# Student Number: 213672431

# CSE Login: pdaloia

*/

#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

#define TEN_MILLIS_IN_NANOS 10000000

typedef  struct {

     char  data ;
     off_t offset ;
     char state; //u = unprocessed, c = changing, p = processed, w = written to output file

} BufferItem ;

//command line information
int inputKey;
int bufSize;
int numberOfInputThreads;
int numberOfWorkThreads;
int numberOfOutputThreads;

//buffer array and buffer variables
BufferItem *buffer;
int bufferCounter;

//files
FILE *inputFilePointer;
FILE *outputFilePointer;

//counters for number of threads completed and their mutexes
int finishedInThreadCounter;
int finishedOutThreadCounter;
int finishedWorkThreadCounter;

//mutex locks
pthread_mutex_t bufferLock;
pthread_mutex_t bufferCounterLock;
pthread_mutex_t inputFileLock;
pthread_mutex_t outputFileLock;
pthread_mutex_t finishedInThreadCounterLock;
pthread_mutex_t finishedOutThreadCounterLock;
pthread_mutex_t finishedWorkThreadCounterLock;

//working mode variable
char runningMode; //e for encrypt and d for decrypt

void *in_thread_function(void *unused){

	//declare structs and variables for nanosleep
	struct timespec t;
	unsigned int seed=0;

	BufferItem result; //temporary result to store inside the buffer

	int i; //for loop variable

	while(1){

		//put thread to sleep when created
		t.tv_sec = 0;
		t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
		nanosleep(&t, NULL);

		//acquire the lock for the input file
		pthread_mutex_lock(&inputFileLock);

		//critical section for reading character from input file and storing information into result
		result.offset = ftell(inputFilePointer);     /* get position of byte in file */
		result.data = fgetc(inputFilePointer);       /* read byte from file */
		result.state = 'u';                          /* set state to u for unprocesses */
		//break if the end of the input file has been reached
		if(result.data == EOF){
			pthread_mutex_unlock(&inputFileLock);
			break;
		}
		
		//release the lock for the input file
		pthread_mutex_unlock(&inputFileLock);

		//-----------------------------------Done with input file-----------------------------------------------------------------------------
		
		//acquire the lock for the buffer
		pthread_mutex_lock(&bufferLock);

		//check to see if the buffer is full
		while(bufSize == bufferCounter){
			pthread_mutex_unlock(&bufferLock);
			t.tv_sec = 0;
			t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
			nanosleep(&t, NULL);
			pthread_mutex_lock(&bufferLock);
			//printf("in thread has buffer lock 2\n");
		}

		//place the result into an empty spot in the buffer
		for(i = 0; i < bufSize; i++){
			if(buffer[i].state == 'e'){
				buffer[i] = result;
				pthread_mutex_lock(&bufferCounterLock);
				bufferCounter++;
				pthread_mutex_unlock(&bufferCounterLock);
				break;
			}
		}

		//release the buffer lock
		pthread_mutex_unlock(&bufferLock);

	}

	//increment the number of in threads that have finished
	pthread_mutex_lock(&finishedInThreadCounterLock);
	finishedInThreadCounter++;
	pthread_mutex_unlock(&finishedInThreadCounterLock);

	pthread_exit(0);

}

void *work_thread_function(void *unused){

	struct timespec t;
	unsigned int seed=0;

	BufferItem result;
	int i;
	int signalToFinish = 0; //changes to 1 when current thread should finish

	//----------------------FIGURE OUT WHEN TO BREAK OUT OF THIS WHILE LOOP----------------------------------------------------------------------------------------------

	while(1){

		//put the thread to sleep when created
		t.tv_sec = 0;
		t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
		nanosleep(&t, NULL);

		//acquire the lock for the buffer and finished in thread counter
		pthread_mutex_lock(&bufferLock);
		pthread_mutex_lock(&finishedInThreadCounterLock);

		//sleep while the buffer is empty (has nothing to process)
		while(bufferCounter == 0 && numberOfInputThreads != finishedInThreadCounter){
			pthread_mutex_unlock(&bufferLock);
			pthread_mutex_unlock(&finishedInThreadCounterLock);

			//go to sleep
			t.tv_sec = 0;
			t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
			nanosleep(&t, NULL);

			pthread_mutex_lock(&bufferLock);
			pthread_mutex_lock(&finishedInThreadCounterLock);
		}

		//release the finished in thread counter lock
		pthread_mutex_unlock(&finishedInThreadCounterLock);

		//check for a buffer spot that is labelled as unprocesses, change its state to changing, and store in result
		for(i = 0; i < bufSize; i++){
			if(buffer[i].state == 'u'){
				buffer[i].state = 'c';
				result = buffer[i];
				break;
			}
		}

		//release the buffer lock
		pthread_mutex_unlock(&bufferLock);

		//----------------------------------------------------------------------------------------------------------------------------------------------------------------

		//acquire the buffer lock
		pthread_mutex_lock(&bufferLock);

		//process result with certain formula if in encryption mode
		if(runningMode == 'e'){
			if(result.data > 31 && result.data < 127){
				result.data = (((int)result.data - 32) + 2 * 95 + inputKey) % 95 + 32;
			}
		}
		//process result with certain formula if in decryption mode
		else if(runningMode == 'd'){
			if(result.data > 31 && result.data < 127){
				result.data = (((int)result.data - 32) + 2 * 95 - inputKey) % 95 + 32;
			}
		}

		//set the results state to processed
		result.state = 'p';

		//release the buffer lock
		pthread_mutex_unlock(&bufferLock);

		//----------------------------------------------------------------------------------------------------------------------------------------------------------------

		//acquire the buffer lock
		pthread_mutex_lock(&bufferLock);

		//store the result in a buffer spot that has its status set to c for changing
		for(i = 0; i < bufSize; i++){
			if(buffer[i].state == 'c'){
				buffer[i] = result;
				break;
			}
		}

		//acquire lock for counter of finished in threads
		pthread_mutex_lock(&finishedInThreadCounterLock);

		//check to see if all input threads have finished, if they have check to see if all buffer spots have been processed
		if(finishedInThreadCounter == numberOfInputThreads){
			signalToFinish = 1;
			for(i = 0; i < bufSize; i++){
				if(buffer[i].state == 'u'){
					signalToFinish = 0;
					break;
				}
			}
		}

		//release counter for finished in threads
		pthread_mutex_unlock(&finishedInThreadCounterLock);

		//release buffer lock
		pthread_mutex_unlock(&bufferLock);

		//break out of while loop if the signal to finish has been changed to 1
		if(signalToFinish == 1){
			break;
		}

		//----------------------------------------------------------------------------------------------------------------------------------------------------------------

	}

	//increment counter for number of finished work threads
	pthread_mutex_lock(&finishedWorkThreadCounterLock);
	finishedWorkThreadCounter++;
	pthread_mutex_unlock(&finishedWorkThreadCounterLock);

	pthread_exit(0);

}

void *out_thread_function(void *unused){

	struct timespec t;
	unsigned int seed=0;

	BufferItem result;
	int i = 0;
	int signalToFinish = 0; //changes to 1 when thread should finish

	while(1){

		//put thread to sleep when created
		t.tv_sec = 0;
		t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
		nanosleep(&t, NULL);

		//acquire lock for the buffer and the counter for finished work threads
		pthread_mutex_lock(&bufferLock);
		pthread_mutex_lock(&finishedWorkThreadCounterLock);

		//put thread to sleep when buffer is empty and the work threads have not all finished yet
		while(bufferCounter == 0 && numberOfWorkThreads != finishedWorkThreadCounter){
			pthread_mutex_unlock(&bufferLock);
			pthread_mutex_unlock(&finishedWorkThreadCounterLock);

			//go to sleep
			t.tv_sec = 0;
			t.tv_nsec = rand_r(&seed)%(TEN_MILLIS_IN_NANOS+1);
			nanosleep(&t, NULL);

			pthread_mutex_lock(&bufferLock);
			pthread_mutex_lock(&finishedWorkThreadCounterLock);
		}

		//release lock for counter of the finished work threads
		pthread_mutex_unlock(&finishedWorkThreadCounterLock);

		//check for processed item in the buffer and remove to out into the output file
		for(i = 0; i < bufSize; i++){
			if(buffer[i].state == 'p'){
				result = buffer[i];
				buffer[i].state = 'e';
				//decrement the amount of buffer spots taken
				pthread_mutex_lock(&bufferCounterLock);
				bufferCounter--;
				pthread_mutex_unlock(&bufferCounterLock);
				break;
			}
		}

		//release lock for counter of number of finished work threads
		pthread_mutex_lock(&finishedWorkThreadCounterLock);

		//check to see if all work threads have finished and that all processed items have been output to the output file
		if(finishedWorkThreadCounter == numberOfWorkThreads){
			signalToFinish = 1;
			for(i = 0; i < bufSize; i++){
				if(buffer[i].state == 'p'){
					signalToFinish = 0;
					break;
				}
			}
		}

		//release lock for buffer and finished work counter 
		pthread_mutex_unlock(&finishedWorkThreadCounterLock);
		pthread_mutex_unlock(&bufferLock);

		//----------------------------------------------------------------------------------------------------------------------------------------------------------------

		//acquire lock for output file
		pthread_mutex_lock(&outputFileLock);

		if (fseek(outputFilePointer, result.offset, SEEK_SET) == -1) {
	    		fprintf(stderr, "error setting output file position to %u\n", (unsigned int) result.offset);
	    		exit(-1);
		}

		if (fputc(result.data, outputFilePointer) == EOF) {
		    fprintf(stderr, "error writing byte %d to output file\n", result.data);
		    exit(-1);
		}

		//release lock for output file
		pthread_mutex_unlock(&outputFileLock);

		if(signalToFinish == 1){
			break;
		}

	}

	pthread_exit(0);

}

int main(int argc, char *argv[]){

	//encrypt <KEY> <nIN> <nWORK> <nOUT> <file_in> <file_out> <bufSize>

	/*<KEY>: the key value used for encryption or decryption, and its valid value is from -127 to 127. If it is positive
	         WORK threads use  <KEY> as the KEY value to encrypt each data byte. If it is negative
	         WORK threads use the absolute value of <KEY> as the key value to decrypt each data byte.
	<nIN>: the number of IN threads to create. There should be at least 1.
	<nWORK>: the number of WORK threads to create. There should be at least 1.
	<nOUT>:  the number of OUT threads to create. There should be at least 1.
	<file_in>: the pathname of the file to be converted. It should exist and be readable.
	<file_out>: the name to be given to the target file. If a file with that name already exists, it should be overwritten.
	<bufSize>:  the capacity, in terms of BufferItem’s, of the  shared buffer. This should be at least 1.*/

	int i; //variable for for loops

	//get the arguments from the command line and assign to proper variables
	inputKey = atoi(argv[1]);
	if(inputKey >= 0){
		runningMode = 'e';
	}
	else{
		runningMode = 'd';
		inputKey = abs(inputKey);
	}

	numberOfInputThreads = atoi(argv[2]);
	numberOfWorkThreads = atoi(argv[3]);
	numberOfOutputThreads = atoi(argv[4]);
	bufSize = atoi(argv[7]);
	inputFilePointer = fopen(argv[5], "r");
	outputFilePointer = fopen(argv[6], "w");

	//set the buffer to be used by all threads
	buffer = (BufferItem *)malloc(bufSize * sizeof(BufferItem));
	bufferCounter = 0;

	//initialize all buffer spots to have 'e' for status (empty);
	for(i = 0; i < bufSize; i++){
		buffer[i].state = 'e';
	}

	//set the mutexes
	if (pthread_mutex_init(&bufferLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&bufferCounterLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&inputFileLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&outputFileLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&finishedInThreadCounterLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&finishedOutThreadCounterLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&finishedWorkThreadCounterLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

	finishedInThreadCounter = 0;
	finishedOutThreadCounter = 0;
	finishedWorkThreadCounter = 0;

    //create all pthreads
    pthread_t inputThreadTids[numberOfInputThreads];
    pthread_t workThreadTids[numberOfWorkThreads];
    pthread_t outputThreadTids[numberOfOutputThreads];
    for(i = 0; i < numberOfInputThreads; i++){
    	pthread_create(&inputThreadTids[i], NULL, in_thread_function, NULL);
    }
    for(i = 0; i < numberOfWorkThreads; i++){
    	pthread_create(&workThreadTids[i], NULL, work_thread_function, NULL);
    }
    for(i = 0; i < numberOfOutputThreads; i++){
    	pthread_create(&outputThreadTids[i], NULL, out_thread_function, NULL);
    }

    //join all pthreads
    for(i = 0; i < numberOfInputThreads; i++){
		pthread_join(inputThreadTids[i], NULL);
	}
	for(i = 0; i < numberOfWorkThreads; i++){
		pthread_join(workThreadTids[i], NULL);
	}
	for(i = 0; i < numberOfOutputThreads; i++){
		pthread_join(outputThreadTids[i], NULL);
	}

	//close the input and output file
	fclose(inputFilePointer);
	fclose(outputFilePointer);
	free(buffer);

	return 0;

}