/**
 * @file main.c
 * @author Alejandro García Montoro
 * @date 27 Jul 2014
 * @brief Server side of GranaSAT experiment, selected for BEXUS 19 campaign.
 *
 *
 */


#include <cv.h>
#include <highgui.h>

#include <stdio.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
//#include <string.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <errno.h>

//#include "packet.h"
#include "sensors.h"
#include "DMK41BU02.h"
//#include "DS1621.h"
//#include "connection.h"
//#include "LSM303.h"
//#include "i2c-dev.h"
#include "sync_control.h"
#include "attitude_determination.h"
#include "protocol.h"

#include <signal.h>
#include <stdint.h>

#include <pthread.h>

const char* acc_file_name = "accelerometer_measurements.data";
const char* mag_file_name = "magnetometer_measurements.data";
const int CAPTURE_RATE_NSEC = 2000000000;

pthread_t capture_thread, LS303DLHC_thread, connection_thread, processing_thread;

void intHandler(int dummy){
		printf("\nFinishing all threads\n");
		
        keep_running = 0;

        //pthread_cancel(capture_thread);
        //pthread_cancel(LS303DLHC_thread);
        pthread_cancel(connection_thread);
        //pthread_cancel(processing_thread);
}

void* capture_images(void* useless){
	uint8_t* image_data;
	struct timespec old, current, difference;
	int time_passed = 0;

	image_data = malloc(sizeof(*image_data) * 1280*960);

	//Enable DMK41BU02 sensor - Camera
	struct v4l2_parameters params;

	params.brightness_ = 0;
	params.gamma_ = 100;
	params.gain_ = 260;
	params.exp_mode_ = 1;
	params.exp_value_ = 200;
	
	enable_DMK41BU02(&params);

	clock_gettime(CLOCK_MONOTONIC, &old);

	while(keep_running){
		clock_gettime(CLOCK_MONOTONIC, &current);

		if( (time_passed = diff_times(&old, &current)) < CAPTURE_RATE_NSEC ){
			difference = nsec_to_timespec(CAPTURE_RATE_NSEC - time_passed);
			nanosleep( &difference, NULL );
		}
		else{
			capture_frame(image_data);
			clock_gettime(CLOCK_MONOTONIC, &old);
		}
	}

	disable_DMK41BU02();
	free(image_data);

	return NULL;
}

void* control_LS303DLHC(void* useless){
	//Enable LSM303 sensor - Magnetometer/Accelerometer
	enableLSM303();

	FILE* file_acc = fopen(acc_file_name, "w");
	FILE* file_mag = fopen(mag_file_name, "w");

	while(keep_running){
		readAndStoreAccelerometer(file_acc);
		readAndStoreMagnetometer(file_mag);
	}

	fclose(file_acc);
	fclose(file_mag);

	return NULL;
}

void* control_connection(void* useless){
	int listen_socket;
	int newsock_big, newsock_small, newsock_commands;
	int command, value;

	int cons_cent, magnitude, px_thresh;

	listen_socket = prepareSocket(PORT_COMMANDS);

	newsock_commands = connectToSocket(listen_socket);
	printf("New socket opened: %d\n", newsock_commands);

	newsock_big = connectToSocket(listen_socket);
	printf("New socket opened: %d\n", newsock_big);

	newsock_small = connectToSocket(listen_socket);
	printf("New socket opened: %d\n", newsock_small);

	while(keep_running){
		usleep(500000);
		sendAccAndMag(newsock_small);

		command = getCommand(newsock_commands);
		
		switch(command){
			case MSG_PASS:
				break;
				
			case MSG_END:
				keep_running = 0;
				break;

			case MSG_RESTART:
				keep_running = 0;
				break;

			case MSG_PING:
				sendData(0, newsock_commands);
				printf("MSG_PING received\n\n");
				break;

			case MSG_SET_STARS:
				cons_cent = getInt(newsock_commands);
				changeParameters(threshold, threshold2, ROI, threshold3, cons_cent, err);
				break;

			case MSG_SET_CATALOG:
				magnitude = getInt(newsock_commands);
				changeCatalogs(magnitude);
				break;

			case MSG_SET_PX_THRESH:
				px_thresh = getInt(newsock_commands);
				changeParameters(px_thresh, threshold2, ROI, threshold3, stars_used, err);
				break;

			case MSG_SET_ROI:
				break;

			case MSG_SET_POINTS:
				break;

			case MSG_SET_ERROR:
				break;

			default:
				break;
		}
	}
	
	close(newsock_big);
	close(newsock_small);
	close(newsock_commands);
}

void* process_images(void* useless){
	uint8_t* image;
	int is_processable = 0;
	image = malloc(sizeof(*image) * 1280*960);

	long long n_nsec, n_nsec_mean;

	char img_file_name[100];

	int rand_id;
	srand(time(NULL));

	struct timespec before, after, elapsed;


	enableStarTracker(150, 21, 21, 10, 4, 0.035, 4);

	int first = 1;

	while(keep_running){
		pthread_rwlock_rdlock( &camera_rw_lock );

			if(new_frame_proc){
				memcpy(image, current_frame, sizeof(uint8_t) * 1280*960);
				new_frame_proc = 0;
				is_processable = 1;
			}
			else
				is_processable = 0;

		pthread_rwlock_unlock( &camera_rw_lock );

		if(is_processable && !first){
			int count;
			long long ITER = 5;
			int NANO_FACTOR = 1000000000;

			n_nsec = n_nsec_mean = 0;

			for (count = 0; count < ITER; ++count){
				clock_gettime(CLOCK_MONOTONIC, &before);
					obtainAttitude(image);
				clock_gettime(CLOCK_MONOTONIC, &after);

				elapsed = diff_times_spec(&before, &after);

				n_nsec = elapsed.tv_sec * NANO_FACTOR + elapsed.tv_nsec;
				n_nsec_mean += n_nsec;

				fprintf(stderr, "Attitude obtained in %ld s %ldns = %lldns\n", elapsed.tv_sec, elapsed.tv_nsec, n_nsec);
			}
		}

		first = 0;
	}

	disableStarTracker();

	free(image);

	return NULL;
}

int main(int argc, char** argv){
	// *******************************
    // ***** SYNC  INITIALISATION ****
    // *******************************

	//Initialise signal
	signal(SIGINT, intHandler);
	signal(SIGTERM, intHandler);

	//Loop control
	keep_running = 1;

	//Semaphores for reading/writing frames and for changing algorithms parameters
	pthread_rwlock_init( &camera_rw_lock, NULL );
	pthread_mutex_init( &mutex_star_tracker, NULL );

	//Initilise clock
	clock_gettime(CLOCK_MONOTONIC, &T_ZERO);

	// *******************************
    // ******** START  THREADS *******
    // *******************************

	//pthread_create( &capture_thread, NULL, capture_images, NULL );
	pthread_create( &processing_thread, NULL, process_images, NULL );
	//pthread_create( &LS303DLHC_thread, NULL, control_LS303DLHC, NULL );
	//pthread_create( &connection_thread, NULL, control_connection, NULL );


	// *******************************
    // ********  JOIN THREADS  *******
    // *******************************	
	//pthread_join( capture_thread, NULL );
	pthread_join( processing_thread, NULL );
	//pthread_join( LS303DLHC_thread, NULL );
	//pthread_join( connection_thread, NULL );


	// *******************************
    // ********  DESTROY SEMS  *******
    // *******************************	
	pthread_rwlock_destroy( &camera_rw_lock );
	pthread_mutex_destroy( &mutex_star_tracker );

	return 0;
}