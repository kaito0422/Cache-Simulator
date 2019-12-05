#include <iostream>
#include <ctime>

#define MAX_SIZE		524288		// 4MB

/* Cache size (byte) */
#define SIZE16KB		2048		// 16KB
#define SIZE32KB		4096		// 32KB
#define SIZE64KB		8192		// 64KB
#define SIZE128KB		16384		// 128KB
#define SIZE256KB		32768		// 256KB
#define SIZE512KB		65536		// 512KB
#define SIZE1024KB		131072		// 1MB

#define TEST_LEN		SIZE512KB

#define TEST_TIME		20

#define STRIDE			8

unsigned long data[MAX_SIZE];
unsigned long dirty[MAX_SIZE];

/* this function is used to insert unused data into the cache */
void insert_dirty_data()
{
	volatile unsigned long tmp;
	int times = 1024;
	while(times--) {
		for(int i = 0; i < MAX_SIZE; i++)
			tmp = dirty[i];
	}
}

int main()
{
	int times;
	volatile unsigned long tmp;
	clock_t start_t, end_t;
	unsigned long test_len;

	test_len = 2048;		// from 16KB to 1024KB
	for( ; test_len < 131172; test_len *= 2) {
		int test_times = TEST_TIME;
		int val[TEST_TIME];

		for(int i = 0; i < TEST_TIME; i++)
			val[i] = 0;

		std::cout << "================================================" << std::endl;
		std::cout << "tested cache size: " << test_len / 1024 * 8 << "KB" << std::endl;

		insert_dirty_data();

		/* test the cycles for accessing uncached data with 64 bytes stride */
		start_t = clock();
		for(int i = 0; i < test_len; i += STRIDE)
			tmp = data[i];
		end_t = clock();
		std::cout << "accessing cycles of uncached data: " << end_t - start_t << std::endl;
		
		/* test several times */
		while(test_times--) {
			/* this subpart is for eliminate the effect from valid cacheline */
			times = 1024 * 64;
			start_t = clock();
			while(times--) {
				for(int i = 0; i < test_len; i += STRIDE)
					tmp = data[i];
			}
			end_t = clock();

			/* test the accessing cycles of cached data several times
			 * because BOOM applies random replacement policy, which 
			 * measns the victim cacheline cannot be determined. Thus,
			 * when the target accessing data size is larger than the 
			 * cache size, the accessing cycles for different iterations
			 * may be differeny. And we can capture the unstable feature
			 * to test the cache size.
			 * */
			start_t = clock();
			for(int i = test_len - 1; i >=0; i -= STRIDE)
				tmp = data[i];
			end_t = clock();
			std::cout << "accessing cycles of cached data: " << end_t - start_t << std::endl;

			val[TEST_TIME - test_times - 1] = end_t - start_t;
		}

		/* calculate the average accessing cycles */
		float average = 0;
		for(int i = 0; i < TEST_TIME; i++)
			average += val[i];
		average /= TEST_TIME;

		/* calculate the variance to determine if the accessing cycles are stable */
		float variance = 0;
		for(int i = 0; i < TEST_TIME; i++) {
			float tmp = val[i] - average;
			variance += tmp * tmp;
		}
		variance /= TEST_TIME;
		std::cout << "accessing variance: " << variance << std::endl;
	}

	return 0;
}

