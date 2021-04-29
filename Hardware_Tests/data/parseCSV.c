#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
	if(argc == 2)
		printf("%s\n", argv[1]);
/*
	FILE *file = fopen("aodv_out.csv", "r");
	char buf[400];
	int wait_for_rx= 0;
	int sleepTime= 100;
	int num_bytes= 10;

	while(fgets(buf, sizeof(buf), file)) {
		const char delim[2] = ",";

		char *data;
		data= strtok(buf, delim);
		wait_for_rx= atoi(data);

		data= strtok(NULL, delim);
		sleepTime= atoi(data);

		data= strtok(NULL, delim);
		num_bytes= atoi(data);

		printf("--> %d  %d  %d\n ", wait_for_rx, sleepTime, num_bytes);

	}
*/
	return 0;
}
