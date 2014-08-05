#include "LSM303.h"

int file;

void  readBlock(uint8_t command, uint8_t size, uint8_t *data){
     int result = i2c_smbus_read_i2c_block_data(file, command, size, data);
     if (result != size) {
          printMsg(stderr, LSM303, "Failed to read block from I2C.");
          exit(1);
    }
}

void selectDevice(int file, int addr){
     if (ioctl(file, I2C_SLAVE, addr) < 0) {
          printMsg(stderr, LSM303, "Error: Could not select device LSM303: %s\n", strerror_r(errno));
     }
}

void readACC(uint8_t* a, struct timespec* timestamp){
    struct timespec init, end;
    selectDevice(file,ACC_ADDRESS);
    //Read a 6bit-block. Start at address LSM303_OUT_X_L_A (0x28) and end at address OUT_Z_H_A(0x2D)
    //Read p28 of datasheet for further information.
    clock_gettime(CLOCK_MONOTONIC, &init);
        readBlock(0x80 | LSM303_OUT_X_L_A, 6, a);
    clock_gettime(CLOCK_MONOTONIC, &end);

    timestamp->tv_sec = (init.tv_sec + end.tv_sec) / 2 - T_ZERO.tv_sec;
    timestamp->tv_nsec = (init.tv_nsec + end.tv_nsec) / 2 - T_ZERO.tv_nsec;
}

void readMAG(uint8_t* m, struct timespec* timestamp){
    struct timespec init, end;
    selectDevice(file,MAG_ADDRESS);

    clock_gettime(CLOCK_MONOTONIC, &init);
        readBlock(0x80 | LSM303_OUT_X_H_M, 6, m);  //Read p28 of datasheet for further information
    clock_gettime(CLOCK_MONOTONIC, &end);

    timestamp->tv_sec = (init.tv_sec + end.tv_sec) / 2 - T_ZERO.tv_sec;
    timestamp->tv_nsec = (init.tv_nsec + end.tv_nsec) / 2 - T_ZERO.tv_nsec;
}

void readTMP(uint8_t* t, struct timespec* timestamp){
    struct timespec init, end;
    //uint8_t block[2];
    selectDevice(file,MAG_ADDRESS);
    //Read p39 of datasheet for further information
    clock_gettime(CLOCK_MONOTONIC, &init);
        readBlock(0x80 | LSM303_TEMP_OUT_H_M, 2, t);
    clock_gettime(CLOCK_MONOTONIC, &end);

    timestamp->tv_sec = (init.tv_sec + end.tv_sec) / 2 - T_ZERO.tv_sec;
    timestamp->tv_nsec = (init.tv_nsec + end.tv_nsec) / 2 - T_ZERO.tv_nsec;
    //High bytes first
    //*t = (int16_t)(block[1] | block[0] << 8) >> 4;
}
void writeAccReg(uint8_t reg, uint8_t value){
    selectDevice(file,ACC_ADDRESS);
    int result = i2c_smbus_write_byte_data(file, reg, value);
    if (result == -1){
        printMsg(stderr, LSM303, "Failed to write byte to I2C Acc.\n");
        exit(1);
    }
}

void writeMagReg(uint8_t reg, uint8_t value){
     selectDevice(file,MAG_ADDRESS);
     int result = i2c_smbus_write_byte_data(file, reg, value);
     if (result == -1){
          printMsg(stderr, LSM303, "Failed to write byte to I2C Mag.\n");
          exit(1);
     }
}

void writeTmpReg(uint8_t reg, uint8_t value){
     //Temperature lectures provided by magnetometer
     selectDevice(file, MAG_ADDRESS);
     int result = i2c_smbus_write_byte_data(file, reg, value);
     if (result == -1){
          printMsg(stderr, LSM303, "Failed to write byte to I2C Temp.\n");
          exit(1);
     }
}

void enableLSM303(){
     //__u16 block[I2C_SMBUS_BLOCK_MAX];

     //int res, bus,  size;

     char filename[20];
     sprintf(filename, "/dev/i2c-%d", 1);
     file = open(filename, O_RDWR);
     if (file<0){
          printMsg(stderr, LSM303, "Unable to open I2C bus!\n");
          //exit(1);
     }

     // Enable accelerometer.
     writeAccReg(LSM303_CTRL_REG1_A, 0b01010111); // P.24 datasheet. z,y,x axis enabled , 100Hz data rate
     writeAccReg(LSM303_CTRL_REG4_A, 0b00101000); // P.26 datasheet. Flag FS=XX10XXXX. +-8G full scale, Gain 4mG/LSB. high resolution output mode

     // Enable magnetometer
     writeMagReg(LSM303_MR_REG_M, 0x00);		// P.37 datasheet. Enable magnometer
     writeMagReg(LSM303_CRB_REG_M, 0b00100000);	//P.37 datasheet. Flag GN=001XXXXX. +-1.3 Gauss, Gain xy 1100LSB/Gauss and Gain z 980LSB/Gauss.

     // Enable termometer
     //P.36 datasheet Enable termometer, minimun data output rate 15Hz
     writeTmpReg(LSM303_CRA_REG_M, 0b10010000);
}