/*
 * Name: Wang Tao
 * ID: 515030910083
 * 
 * 
*/
#include "cachelab.h"
//recomanded head file
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFSIZE 1024


typedef enum cache_status{HIT, COLD_MISS, CONF_MISS} status_t;

typedef struct address{
	unsigned long long tag;
	unsigned long long set;
	unsigned long long offset;
} address_t;




typedef struct line{
	unsigned long long valid_time; 
	//if not allocated ,0; otherwise is the last visit time
	unsigned long long tag; 
	// the whole address is 64 bits
	// no need for block
} line_t;

typedef struct set{  // each set type have a pointer point to 
	                 // the first line in the same set
	line_t *lines;
}set_t;


typedef struct cache{
	// basic attributes 
	int s;
	int E; 
	int b;
    // pointer to the array of set 
	set_t *sets;

} cache_t;

void free_cache(cache_t * cache){
	int set_num = (1 << (cache->s));
	if(cache){
		int i;
	//	int j;
		for(i = 0; i < set_num; i++){
			//set_t aSet = cache->sets[i];
			line_t * lines = cache->sets[i].lines;
			free(lines);
	//		for(j = 0; j < cache->E; j++){
	//			lines++;
	//			free(lines);  //free each line				
	//		}
		}
	}

	free(cache->sets);
	free(cache);
}

cache_t *init_cache(int s, int E, int b){

	//check arguments
	if(s < 0 || E < 0 || b < 0 || s + b > 64){
		printf("wrong arguments!\n");
		return NULL;

	}

	cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
	
	int set_num = (1 << s);
	cache->s = s;
	cache->b = b;
	cache->E = E;
	
	cache->sets =(set_t *) malloc(sizeof(set_t) * set_num);
	int i,j;
	for(i = 0; i < set_num; i++){
	
		line_t *lines = (line_t *)malloc(sizeof(line_t) * E);  // important

		for(j = 0; j < E; j++)
			lines[j].valid_time = 0;
		
		(cache->sets[i]).lines = lines;
	
	}

	return cache;
}


typedef struct simulator{
	cache_t *cache;
	FILE * file;
	int hits;
	int misses;
	int evictions;

	unsigned long long time;
	
} simulator_t;

void free_sim(simulator_t *sim){
	if(sim){
		free_cache(sim->cache);
		fclose(sim->file);

	}
	free(sim);
}


simulator_t * init_sim(char *fileName, int s, int E, int b){
	FILE * file = fopen(fileName, "r");
	if(!file){
		printf("Can't open that file\n");
		return NULL;
	}

	// then create a struct of simulator
    simulator_t * sim = (simulator_t *)malloc(sizeof(simulator_t));
	if(!sim){
		printf("malloc error\n");
		return NULL;
	}

	cache_t *cache = init_cache(s, E, b); // initialize a cache
	if(!cache){
		printf("initialize cache error\n");
		free(sim);
		// close the file

		return NULL;
	}
	// malloc success, set simulator
	sim->cache = cache;
	sim->file = file;
    sim->time = 1; // initial time = 1
	sim->hits = 0;
	sim->misses = 0;
	sim->evictions = 0;
	
	return sim;
}

status_t simul_access(simulator_t *sim, address_t addr){//every time call this function , the time of simulator should +1 s
	sim->time++;  //update time
	cache_t *cache = sim->cache;
	set_t set = cache->sets[addr.set]; //set only have a pointer 
	int line_num = cache->E;
	line_t *now_line;
	status_t result;

	int i;
	for(i = 0; i < line_num; i++){
		now_line = &set.lines[i];
		if((now_line->valid_time) && (now_line->tag == addr.tag)){//hit
			// update last access time
			now_line->valid_time = sim->time;
			//return HIT;
			sim->hits++;
			return  HIT;

		}
	}
	// can't find
	unsigned long long oldest = sim->time;
	int index = 0;
	for(i = 0; i < line_num; i++){
		now_line = &set.lines[i];
		if(now_line->valid_time < oldest){
			oldest = now_line->valid_time;
			index = i;
		}
	}
	
	result = COLD_MISS;
	// update 
	if(set.lines[index].valid_time == 0)// cold_miss
		sim->misses++;
	else{ //conflict miss
		sim->evictions++;
		sim->misses++;
		result = CONF_MISS;
	}

	set.lines[index].valid_time = sim->time;
	set.lines[index].tag = addr.tag;

	return result;
}

address_t manage_address(cache_t *cache, unsigned long long old_addr){
	address_t addr;
	int b = cache->b;
	int s = cache->s;
	
	addr.offset = (old_addr & ((1 << b) - 1));
	addr.set = (old_addr >> b) & ((1 << s) - 1);
	addr.tag = (old_addr & ~((1<< (b + s)) - 1)) >> (b + s);

	return addr;
}// need modify

char *status_char[] = {"hit","cold miss","conflict miss"};

void execute_line(char *line, simulator_t *sim, int verbose){ //need modify
	int count = 1;
	if(line[0] != ' ') //ignore I
		return;
	if(line[1] == 'M') // follow by store
		count++;
	unsigned long long addr;
	int size;
	sscanf(line + 3, "%llx, %d", &addr, &size);
	if (verbose)
	  printf("%c %llx,%d", line[1], addr, size);
	
	address_t a = manage_address(sim->cache, addr);

	int i;
	for(i = 0; i < count; i++){
		status_t status = simul_access(sim,a);
		if(verbose)
			printf("%s", status_char[status]);
	}
	if(verbose)
		printf("\n");
}


int main(int argc, char *argv[])
{
	int opt;
	int verbose;
	int s,E,b;
	char *fileName = NULL;
	
	while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1){
		switch(opt){
			case 'v':
				verbose = 1;
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				fileName = optarg;
				break;	
			default:
				printf("Usage: ./%s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
				return 1;
		}
	} 
	// check if the parse funciton is right
	printf("v:%d, s:%d, E:%d, b:%d, filename:%s\n", verbose, s, E, b, fileName);
	
	// initialize a simulator
	simulator_t* sim = init_sim(fileName, s, E, b);
	if(!sim){
		printf("initialize simulator error\n");
		return 2;
	}	
	
	//work
	static char buffer[BUFFSIZE];
	while(fgets(buffer, BUFFSIZE,sim->file) != NULL)
		execute_line(buffer,sim,verbose);
	printf("shit: %d, %d, %d\n",sim->hits, sim->misses, sim->evictions);
    printSummary(sim->hits, sim->misses, sim->evictions);
	
	// done
	free_sim(sim);


    return 0;
}
