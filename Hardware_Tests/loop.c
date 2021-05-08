/*
 * @brief Allows the initiation of simulation 
 * for the sake of measuring the power consumption of transmission
 * a button can be pressed, and the simulation can be run without a monitor or keyboard
 * (removes the power consumption of those peripherals from measurements)
 *
 * Date: May 2nd, 2021
 * Author: Arden Diakhte-Palme
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpioLib.h"

#define GPIO_SWITCH 22

int main(void){
	GPIOExport(GPIO_SWITCH);
	GPIODirection(GPIO_SWITCH, 0);

	int switch_val; 
	while(1){

		//if button is pressed (once monitor and keyboard disconnected)
		switch_val= GPIORead(GPIO_SWITCH);
		if(switch_val){
			char command[50];
			strcpy(command, "sudo ./TX -f olsr_out.csv -n 1106");
			printf("%s\n",command);
			system(command);
			sleep(1);
			break;
		}
	}
	return 0;
}
