#include <mbed.h>
// #include <USBSerial.h>
#include "LIS3DSH.h"
#include <math.h>

bool system_on = false;
bool shutdown_flag = false;
Timeout shutdown;

// USBSerial serial;

Ticker system_update;

Timer t;
int acc_read_time;
Timer exercise_timer;
int exercise_time;

LIS3DSH acc(SPI_MOSI, SPI_MISO, SPI_SCK, PE_3);

int16_t acc_X, acc_Y, acc_Z;
float raw_acc_X[24];
float raw_acc_Y[24];
float raw_acc_Z[24];
float filt_coeff[24] = {0,0,0.004,0.0060,0.004,-0.006,-0.023,-0.033,-0.02,0.029,0.107,0.19,0.243,0.243,0.19,0.107,0.029,-0.02,-0.033,-0.023,-0.006,0.004,0.006,0.004};
float curr_acc_X, curr_acc_Y, curr_acc_Z;
float delta_mag, curr_mag, curr_mag_X, curr_mag_Y, curr_mag_Z;

bool In_exercise;
float activity;
float act_monitor[5];
float data_monitor[250];
uint8_t data_monitor_i;
float data_mean,data_variance;
float Sit_logist,Pus_logist,Jum_logist,Squ_logist;

DigitalIn Button_1(USER_BUTTON);

DigitalOut Led_1(LED3);
DigitalOut Led_2(LED4);
DigitalOut Led_3(LED5);
DigitalOut Led_4(LED6);

bool Sit = false;
bool Pus = false;
bool Jum = false;
bool Squ = false;

uint8_t Sit_num = 0;
uint8_t Pus_num = 0;
uint8_t Jum_num = 0;
uint8_t Squ_num = 0;

uint8_t Led_count = 0;
uint8_t Led_num = 0;

uint32_t iterator_1;

/*
// Read accelerometer and monitor In-exercise data 
*/
void read_acc() {
  if (system_on) {
    acc.ReadData(&acc_X, &acc_Y, &acc_Z);
    acc_read_time = t.read_ms();
    for (int i1 = 23; i1 > 0; i1--) { raw_acc_X[i1] = raw_acc_X[i1-1];raw_acc_Y[i1] = raw_acc_Y[i1-1];raw_acc_Z[i1] = raw_acc_Z[i1-1]; }
    raw_acc_X[0] = float(acc_X);
    raw_acc_Y[0] = float(acc_Y);
    raw_acc_Z[0] = float(acc_Z);
    curr_acc_X = 0;
    curr_acc_Y = 0;
    curr_acc_Z = 0;
    for (int i1 = 0; i1 < 24; i1++) { curr_acc_X+=raw_acc_X[i1]*filt_coeff[i1];curr_acc_Y+=raw_acc_Y[i1]*filt_coeff[i1];curr_acc_Z+=raw_acc_Z[i1]*filt_coeff[i1]; }
    delta_mag = (curr_acc_X*curr_acc_X + curr_acc_Y*curr_acc_Y + curr_acc_Z*curr_acc_Z - 286350480.6514399)/520736.9720767053;
    activity = 0.0;
    for (int i1 = 4; i1 > 0; i1--) {act_monitor[i1] = act_monitor[i1-1];activity += act_monitor[i1-1];}
    act_monitor[0] = abs(delta_mag);
    activity += abs(delta_mag);
    /*
    // Detection and classification of workout exercises
    */
    if (activity > 250) { 
      if (In_exercise == false) {
        In_exercise = true;exercise_timer.reset();exercise_timer.start();
        for (int i2 = 0; i2 < 250; i2++) {data_monitor[i2] = 0.0;}
        data_monitor_i = 0;
      } 
      if (data_monitor_i < 250) {
        data_monitor[data_monitor_i] = delta_mag;
        data_monitor_i++;
      }
    } else if (In_exercise) {
      In_exercise = false;
      exercise_timer.stop();
      exercise_time = exercise_timer.read_ms();
      if ((exercise_time > 1000) && (exercise_time < 4000)) {
        // Mean of the incoming exercise data
        data_mean = 0.0;
        for (int i2 = 0; i2 < data_monitor_i; i2++) {data_mean+=data_monitor[i2];}
        data_mean = data_mean/data_monitor_i;
        // Variance of the incoming exercise data
        data_variance = 0.0;
        for (int i2 = 0; i2 < data_monitor_i; i2++) {data_variance+= (data_monitor[i2]-data_mean)*(data_monitor[i2]-data_mean);}
        data_variance = data_variance/data_monitor_i;
        /*
        // Using logistic regression to identify the exercise performed
        // Mean, Variance and Exercise_Time_Period are used as the input features for the classification algorithm
        */
        Sit_logist = 1.5*exp(-( (data_mean - 35.75)*(data_mean - 35.75) )/3575.0) +  0.75*exp(-( (data_variance - 24929.61)*(data_variance - 24929.61) )/2492961.0) + 0.75*exp(-( (exercise_time - 2370.0)*(exercise_time - 2370.0) )/237000.0);
        Pus_logist = 1.5*exp(-( (data_mean + 13.109)*(data_mean + 13.109) )/1310.9) +  0.75*exp(-( (data_variance - 24880.8577)*(data_variance - 24880.8577) )/2488085.77) + 0.75*exp(-( (exercise_time - 1560.0)*(exercise_time - 1560.0) )/156000.0);
        Jum_logist = exp(-( (data_mean - 674.0857)*(data_mean - 674.0857) )/67408.57) +  1.5*exp(-( (data_variance - 1568471.1106)*(data_variance - 1568471.1106) )/156847111.06) + 0.5*exp(-( (exercise_time - 1473.33)*(exercise_time - 1473.33) )/147333.0);
        Squ_logist = exp(-( (data_mean - 83.607)*(data_mean - 83.607) )/8360.7) +  1.5*exp(-( (data_variance - 130649.396)*(data_variance - 130649.396) )/13064939.6) + 0.5*exp(-( (exercise_time - 2140.0)*(exercise_time - 2140.0) )/214000.0);
        if (Sit && (Sit_logist>0.5)) {
          if (Sit_num>=15) {Sit=false;} else {Sit_num++;}
        }
        if (Pus && (Pus_logist>0.5)) {
          if (Pus_num>=15) {Pus=false;} else {Pus_num++;}
        }
        if (Jum && (Jum_logist>0.5)) {
          if (Jum_num>=15) {Jum=false;} else {Jum_num++;}
        }
        if (Squ && (Squ_logist>0.5)) {
          if (Squ_num>=15) {Squ=false;} else {Squ_num++;}
        }
        if ((Sit == false) && (Pus == false) && (Jum == false) && (Squ == false)) {
          if ((Sit_logist > Pus_logist) && (Sit_logist > Jum_logist) && (Sit_logist > Squ_logist) && (Sit_logist>0.5) && (Sit_num < 15)) {Sit = true;Sit_num++;}
          if ((Pus_logist > Sit_logist) && (Pus_logist > Jum_logist) && (Pus_logist > Squ_logist) && (Pus_logist>0.5) && (Pus_num < 15)) {Pus = true;Pus_num++;}
          if ((Jum_logist > Sit_logist) && (Jum_logist > Pus_logist) && (Jum_logist > Squ_logist) && (Jum_logist>0.5) && (Jum_num < 15)) {Jum = true;Jum_num++;}
          if ((Squ_logist > Sit_logist) && (Squ_logist > Pus_logist) && (Squ_logist > Jum_logist) && (Squ_logist>0.5) && (Squ_num < 15)) {Squ = true;Squ_num++;}
        }
      }
    }
  }
}

/*
// Shutdown Callback Function
*/
void shutdown_func() {
  if (Button_1.read() == 1) {shutdown_flag=true;Led_1.write(0);Led_2.write(0);Led_3.write(0);Led_4.write(0);}
}

int main() {
  
  // Setup a recurring callback to read the accelerometer and monitor In-exercise data
  system_update.attach(&read_acc, 0.02);

  // Wait till the accelerometer is detected on the SPI bus
  if(acc.Detect() != 1) {
    while(1){};
  }

  while(1) {
    /*
    // Debugger Code
    serial.printf("%d,%d,%.9f,%.9f,%.9f,%.9f\n",iterator_1,acc_read_time,Sit_logist,Pus_logist,Jum_logist,Squ_logist);
    iterator_1++;
    */
    // On/Off switch routine
    if (Button_1.read() == 1) {
      if (system_on) { shutdown.attach(&shutdown_func, 3.0); } else {t.reset();t.start();system_on = true;} 
    }
    if (shutdown_flag) {
      Sit = false;Pus = false;Jum = false;Squ = false;Sit_num = 0;Pus_num = 0;Jum_num = 0;Squ_num = 0;
      t.stop();system_on=false;shutdown_flag=false;Led_1.write(0);Led_2.write(0);Led_3.write(0);Led_4.write(0);wait_ms(5000);
      }

    if (system_on) {
      // ===========================================================
      // Display Routine
      // ===========================================================
      Led_1.write(0);
      Led_2.write(0);
      Led_3.write(0);
      Led_4.write(0);
      
      Led_count = 0;
      if (Sit) {
        Led_num = Sit_num%5;
      }
      if (Pus) {
        Led_num = Pus_num%5;
      }
      if (Jum) {
        Led_num = Jum_num%5;
      }
      if (Squ) {
        Led_num = Squ_num%5;
      }

      // Some LED gymnastics
      for (int i1 = 0; i1 < 200; i1++) {
        // ===========================================================
        if (Sit) {

          if ((i1%20) == 0) {Led_1.write(1);}
          if ((i1%20) == 10) {Led_1.write(0);}

          if ((i1%40) == 0) {
            if (Sit_num >= 5) {
              Led_2.write(1);
              if (Sit_num >= 10) {
                Led_3.write(1);
                if (Sit_num >= 15) {Led_4.write(1);} else if (Led_count < Led_num) {Led_4.write(1);}
              } else if (Led_count < Led_num) {Led_3.write(1);}
            } else if (Led_count < Led_num) {Led_2.write(1);}
          }

          if ((i1%40) == 20) {
            if (Sit_num >= 5) {
              if (Sit_num >= 10) {
                if (Led_count < Led_num) {Led_4.write(0); Led_count += 1;}
              } else if (Led_count < Led_num) {Led_3.write(0); Led_count += 1;}
            } else if (Led_count < Led_num) {Led_2.write(0); Led_count += 1;}
          }

        }
        // ===========================================================
        if (Pus) {

          if ((i1%20) == 0) {Led_2.write(1);}
          if ((i1%20) == 10) {Led_2.write(0);}

          if ((i1%40) == 0) {
            if (Pus_num >= 5) {
              Led_3.write(1);
              if (Pus_num >= 10) {
                Led_4.write(1);
                if (Pus_num >= 15) {Led_1.write(1);} else if (Led_count < Led_num) {Led_1.write(1);}
              } else if (Led_count < Led_num) {Led_4.write(1);}
            } else if (Led_count < Led_num) {Led_3.write(1);}
          }

          if ((i1%40) == 20) {
            if (Pus_num >= 5) {
              if (Pus_num >= 10) {
                if (Led_count < Led_num) {Led_1.write(0); Led_count += 1;}
              } else if (Led_count < Led_num) {Led_4.write(0); Led_count += 1;}
            } else if (Led_count < Led_num) {Led_3.write(0); Led_count += 1;}
          }

        }
        // ===========================================================
        if (Jum) {

          if ((i1%20) == 0) {Led_3.write(1);}
          if ((i1%20) == 10) {Led_3.write(0);}

          if ((i1%40) == 0) {
            if (Jum_num >= 5) {
              Led_4.write(1);
              if (Jum_num >= 10) {
                Led_1.write(1);
                if (Jum_num >= 15) {Led_2.write(1);} else if (Led_count < Led_num) {Led_2.write(1);}
              } else if (Led_count < Led_num) {Led_1.write(1);}
            } else if (Led_count < Led_num) {Led_4.write(1);}
          }

          if ((i1%40) == 20) {
            if (Jum_num >= 5) {
              if (Jum_num >= 10) {
                if (Led_count < Led_num) {Led_2.write(0); Led_count += 1;}
              } else if (Led_count < Led_num) {Led_1.write(0); Led_count += 1;}
            } else if (Led_count < Led_num) {Led_4.write(0); Led_count += 1;}
          }

        }
        // ===========================================================
        if (Squ) {

          if ((i1%20) == 0) {Led_4.write(1);}
          if ((i1%20) == 10) {Led_4.write(0);}

          if ((i1%40) == 0) {
            if (Squ_num >= 5) {
              Led_1.write(1);
              if (Squ_num >= 10) {
                Led_2.write(1);
                if (Squ_num >= 15) {Led_3.write(1);} else if (Led_count < Led_num) {Led_3.write(1);}
              } else if (Led_count < Led_num) {Led_2.write(1);}
            } else if (Led_count < Led_num) {Led_1.write(1);}
          }

          if ((i1%40) == 20) {
            if (Squ_num >= 5) {
              if (Squ_num >= 10) {
                if (Led_count < Led_num) {Led_3.write(0); Led_count += 1;}
              } else if (Led_count < Led_num) {Led_2.write(0); Led_count += 1;}
            } else if (Led_count < Led_num) {Led_1.write(0); Led_count += 1;}
          }

        }
        // ===========================================================
        if ((Sit == false) && (Pus == false) && (Jum == false) && (Squ == false)) {
          if ((i1%20) == 0) {Led_1.write(1);Led_2.write(1);Led_3.write(1);Led_4.write(1);}
          if ((i1%20) == 10) {
            if (Sit_num < 15) {Led_1.write(0);}
            if (Pus_num < 15) {Led_2.write(0);}
            if (Jum_num < 15) {Led_3.write(0);}
            if (Squ_num < 15) {Led_4.write(0);}
          }
        }
        // ===========================================================
        wait_ms(25);
      }
    }
  }
}