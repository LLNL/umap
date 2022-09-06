#ifndef PROF_CONF_HH
#define PROF_CONF_HH

#define DEBUG(format, ...){\
printf("DEBUG::%s, %s, %d::" format "\n", __FILE__, __func__, __LINE__, ## __VA_ARGS__);\
}

#define ALERT(format, ...){\
printf("ALERT::%s, %s, %d::" format "\n", __FILE__, __func__, __LINE__, ## __VA_ARGS__);\
}

#define INFO(format, ...){\
printf("INFO::%s, %s, %d::" format "\n", __FILE__, __func__, __LINE__, ## __VA_ARGS__);\
}


/*
 * profiler
 */


#define CHUNK_NUM 10000 //chunk num per data structure
#define MIN_CHUNK_SIZE 32 // min page number of one chunk
#endif
