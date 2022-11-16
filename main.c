#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>

FILE* stream = NULL;

// Semaphores
sem_t* writer = NULL;
sem_t* reader = NULL;
sem_t* mutex = NULL;

// Shared buffer
void *shm_ptr = NULL;

int counter = 10;
int readers_counter = 0;

pid_t current_pid = -1;

// Interuption handler
void interuption_handler(int signum)
{
	if (current_pid == 0)
	{
		sem_close(writer);
		sem_close(reader);
		sem_close(mutex);
	}
	
	else if (current_pid != -1)
	{
		sem_unlink("/writerIn");
		sem_unlink("/readerIn");
		sem_unlink("/mutexForCounter");
		
		munmap(shm_ptr, sizeof(int));
	}
	
	if (stream && stream != stdout)
		fclose(stream);
		
	exit(1);
}

void error(char *str)
{
	if (str)
		fprintf(stderr, "Error: %s", str);

	exit(1);
}

int main()
{
	signal(SIGINT, interuption_handler);
	srand(time(0)); // Make numbers random
	
	writer = sem_open("/writerIn", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 0);
	reader = sem_open("/readerIn", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 0);
	mutex = sem_open("/mutexForCounter", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 1);

// Shared buffer
	shm_ptr = mmap(0, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	for (int i = 0; i < 2; i ++) 
	{
		if (current_pid != 0)
		{
			if (i == 0)
				stream = stdout;
			
			else if (i == 1)
				stream = fopen("output.txt", "w");
			
			current_pid = fork();
		}

		if (current_pid == -1)
			error("Can't create new process");
	}
	
	// Role specific code
	if (current_pid != 0) // Writer
	{
		for (int i = 0; i < counter; i ++)
		{
			puts("1");
			// <Critical section	
			int random_number = rand() % 1000000;
			printf("**%i**\n", random_number);
			memcpy(shm_ptr, (char *)&random_number, sizeof(int));
			// Critical section/>
			sem_post(writer);
			sem_wait(reader);
			puts("2");
		}
	}
	
	else // Readers
	{
		sem_t* writer = sem_open("/writerIn", O_EXCL);
		sem_t* reader = sem_open("/readerIn", O_EXCL);
		sem_t* mutex = sem_open("/mutexForCounter", O_EXCL);

		for (int i = 0; i < counter; i ++)
		{
			sem_wait(writer);
			readers_counter ++;
			sem_post(writer);
	
			// <Critical section
			int* random_number = (int *) shm_ptr;
			fprintf(stream, "%i ", *random_number);
			// Critical section/>
			
			sem_wait(mutex);
			readers_counter --;
			
			if (readers_counter == 1)
				sem_wait(writer);
			
			if (readers_counter == 0)
				sem_post(reader);

			sem_post(mutex);
		}
		
		if (stream != stdout)
			fclose(stream);
			
		sem_close(writer);
		sem_close(reader);
		sem_close(mutex);
	}
	
	while(wait(NULL) > 0);
	
	sem_unlink("/writerIn");
	sem_unlink("/readerIn");
	sem_unlink("/mutexForCounter");

	return 0;
}
