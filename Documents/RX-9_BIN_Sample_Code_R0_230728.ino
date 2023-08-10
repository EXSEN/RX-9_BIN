/*
   Coded by EXSEN
   date: 23.07.26
   This code tested under 
      - Arduino nano
      - Arduino uno
      - Arduino pro mini 5V
      + external 3.3V source
   Code revision: R00
   R0: Initiate RX-9_BIN sample code
   

   Schematic
   - RX-9_BIN has 4 pins
    RX-9_BIN   VCC  2. GND 3. EMF 4. THER
                |       |      |       |
    Arduino   3.3 V    GND     A0      A1
                |       |
    LDO       3.3V     GND

   - Don't use 3.3V of arduino's. Arduino 3.3V have low
     allowable current. using arduino 3.3V source may cause
     of malfunction.
   - please use clean power source. 3.3V is connected to OPAMP and Sensor directly.
     if power source fluctuates, sensor value can fluctuate too.

     * 센서에 인가되는 전압을 안정적인 3.3V를 사용하십시오. 
     * 3.3V는 아두이노의 것을 사용하지 마시고, 별도의 전원을 추가하여 사용하십시오. 

*/

#define EMF_pin   0 //RX-9_BIN, A0 of Arudino, Analog pin
#define THER_pin  1 //RX-9_MIN, A1 of Arduino, Analog pin

// Mauna Loa observatory in Hawaii, official CO2 value, average of 2022, https://gml.noaa.gov/ccgg/trends/
// 가장 오랫동안 지구의 CO2 농도를 측정해온 Mauna Loa 관측소의 값을 지구 이산화탄소 농도 값으로 제안드립니다. 
// 하지만, 보통의 CO2 센서들은 400 ppm을 최저값으로 표현하니, 아래의 Base_line값을 400 으로 설정하셔서 사용하는 것이 일반적입니다. 
// Base_line보다 현재 표시하는 값이 낮을 경우 CO2 농도 값을 Base_line값으로 업데이트 합니다. 

#define Base_line 418

/*
   RX-9_BIN is accurate between 400 and 2000 ppm.
   but it could measure CO2 concentration over 2000 ppm
   it could measure over 10,000 ppm but it is not accurate.
   RX-9_BIN은 2000 ppm까지의 정확도를 보장합니다. 2000 ppm 이상도 측정은 가능하지만, 사양서에서 보장하는 오차 범위를 벗어납니다. 
   오차를 벗어난 최대 측정 범위는 수만 ppm 까지 측정됩니다. 
   사용하고자 하는 농도 범위에 따라 조절하시면 됩니다. 
*/
const int32_t max_co2 = 2000;

//Timing setting
unsigned int warm_up_time = 180; // sensor need time to warm up, default: 180, longer is better to stablity. at least 180 seconds is needed.
unsigned long current_time = 0;
unsigned long prev_time = 0;
unsigned int set_time = 1;      // every 1 second

/*
    센서 값을 안정화하기 위해서 이동평균 값을 이용합니다. averaging_count가 크면 클 수록 더 안정적인 값이 되지만, 반응은 늦게 됩니다. 
*/
//Moving average
#define averaging_count 10        // to minimize analog signal flunctuate. higher number shows stable value but slow. 
float m_averaging_data[averaging_count + 1][2] = {0,};
float sum_data[2] = {0,};
float EMF = 0;
float THER = 0;
int averaged_count = 0;

//Sensor data
// 센서 값을 저장하기 위한 변수
float EMF_data = 0.0; // 센서 원신호
float THER_data = 0.0; // 센서 온도
float THER_ini = 0.0;  // 센서 온도값 보정을 위한 변수

// Thermister constant
// RX-9 have thermistor inside of sensor package. this thermistor check the temperature of sensor to compensate the data
// don't edit the number
// 센서 온도를 읽기 위한 써미스터의 상수입니다. 
// 아래 상수를 변경하지 마십시오. 
#define C1 0.00230088
#define C2 0.000224
#define C3 0.00000002113323296
float Resist_0 = 15;

//Status of Sensor
bool status_sensor = 0;

//Step of co2
// if you want to use this sensor to show steps of CO2 air quality not specific value of CO2.
// In my field experience, many developer want to show the value of CO2 concentration at development start. but client don't have interest on digit.
// Many digits make client tired.
// you can use below example level. it divided 5 levels
// 1. 400~700: Green
// 2. 700~1000: Blue
// 3. 1000~2000: Yellow
// 4. 2000~4000: Orange
// 5. >4000: Red
// In house, >2000 is difficult to reach. It can be divided into more fine concentration steps.
// In automotive, >2000 is very easy.
// 사용하시는 어플리케이션에 따라 아래의 단계농도를 편집하십시오. 
// RX-9_BIN은 2000 ppm 까지의 농도의 정확도를 보장하지만, 단계로 표시한다는 것은 정확도가 그렇게 중요한 어플리케이션이 아니라는 것이기 때문에
// max_co2 변수값을 편집하여 CD4 보다 높은 값으로 변경하고 아래의 값을 예시로 사용하십시오. 

int CD1 = 700; // Very fresh, In Korea,
int CD2 = 1000; // normal
int CD3 = 2000; // high
int CD4 = 4000; // Very high
int status_step_CD = 0;

// CO2 to HUMAN
/*
   < 450 ppm  : outdoor co2 concentration, Very fresh and healthy             NATURE
   ~ 700 ppm  : normal indoor concentration                                   HOME
   ~ 1000 ppm : no damage to human but sensitive person can feel discomfort,  OFFICE
   ~ 2000 ppm : little sleepy,                                                BUS
   ~ 3000 ppm : Stiff shoulder, headache,                                     METRO
   ~ 4000 ppm : Eye, throat irritation, headache, dizzy, blood pressure rise
   ~ 6000 ppm : Increased respiratory rate
   ~ 8000 ppm : Shortness of breath
   ~ 10000 ppm: black out in 2~3 minutes
   ~ 20000 ppm: very low oxygen exchange at lung. could be dead.
*/

// Calibration Data
// Factory calibration data are provided in bin rank. you can use the representative value of the
// bin rank as a calibration value.
// cal_A value is updated by internal auto calibration function. so below cal_A value is a just initial vale. it is not worthy.
// but internally updated value is important. it is updated when sensor_reset() is act or auto_calib_co2() is act.
// cal_B value is factory calibration factor. this value is constant value of each sensor. it is not changed.
// cal_B representative value. use below value to calibrate your sensor.
// A Bin: 48
// B Bin: 52
// C Bin: 58
// D Bin: 66
// E Bin: 74
/*
    RX-9_BIN은 cal_B값에 정확한 특정값을 넣지 않고 해당 Bin의 대푯값을 넣습니다. 
    제품의 뒷면에 보면 QR 코드와 Bin 정보가 표현되어 있는데, A~E까지의 알파벳이 Bin 정보입니다. 
    A~E까지의 정보를 읽고 각 단계의 대표값을 cal_B 값에 대신하여 넣어주면 됩니다. 
    발주시에 상호 협의된 Bin 정보를 명기하여 발주하고 그에 맞는 제품을 동일 Bin으로 협의된 수량만큼 납품하기 때문에
    제품의 펌웨어를 매번 바꾸거나, 내부 변수를 매번 업데이트 하는 일 없이 사용할 수 있습니다. 
    cal_A: 코드 내부에서 업데이트되는 값
    cal_B: 공정 교정값, 이 값을 Bin의 대푯값으로 넣어야 함
    A Bin: 48
    B Bin: 52
    C Bin: 58
    D Bin: 66
    E Bin: 74

*/

float cal_A = 372.1;        //Calibrated number
float cal_B = 58;        //Calibrated number
float cal_B_offset = 1.0;   //cal_B could be changed by their structure.
float co2_ppm = 0.0;
float co2_ppm_output = 0.0;
float DEDT = 1.0;


//Auto Calibration Coef.
unsigned long prev_time_METI = 0;
int MEIN = 1440;                      // !!!IMPORTANT!!! MEIN value is autocalibration interval value.
// 1440 means, autocalibration executed every day. 120 is every 2 hour.
// so if you want to change the value think about the autocalibration interval.
// EXSEN suggest value as  Indoor: 1440, Car: 120.
int MEIN_common = 0;
int MEIN_start = 1;
bool MEIN_flag = 0;
int MEIN_rec;
int start_stablize = 900;             // 900 seconds to stablize the sensor core. < start_stablize time, sensor will be reset every 1 min. 
                                      // larger value is more stable. 
int METI = 60;                        //Every 60 second, check the autocalibration coef.
float EMF_max = 0;
float THER_max = 0;
int ELTI = 0;
int upper_cut = 0;
int under_cut_count = 0;
float under_cut = 0.99;               // if co2_ppm shows lower than (Base_line * undercut), sensor do re-calculation

// Damage recovery
// Sensor can be damaged from chemical gas like high concentrated VOC(Volatile Organic Compound), H2S, NH3, Acidic gas, etc highly reactive gas
// so if damage is come to sensor, sensor don't lose their internal calculation number about CO2.
// this code and variant are to prevent changing calculation number with LOCKING
bool damage_cnt_fg = 0;
unsigned int damage_cnt = 0;
unsigned int ppm_max_cnt = 0;
float cal_A_LOG[2][10] = {0,};
bool cal_A_LOCK = 0;
float cal_A_LOCK_value = 0.0;
float emf_LOCK_value = 0.0;
unsigned int LOCK_delta = 50;
unsigned int LOCK_disable = 5;
unsigned int LOCK_timer = 5;
unsigned int LOCK_timer_cnt = 0;
unsigned int S3_cnt = 0;
unsigned int EMF_DMG = 450;

//debug
bool debug = 0;
bool display_mode = 0;

//Communication of Arduino
const int timeout = 1000; // UART timeout


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(timeout);
  delay(1000);
  cal_A_LOCK = 0;
  damage_cnt_fg = 0;
  damage_cnt = 0;
  MEIN_rec = MEIN;

}

void loop() {
  // put your main code here, to run repeatedly:
  current_time = millis() / 1000; // every 1 sec
  if (current_time - prev_time >= set_time) {
    warm_up_chk();
    ppm_cal();
    DMG_REC();
    DMG_5000();
    step_cal_CD();
    auto_calib_co2();
    display_data();
    prev_time = current_time;
  }
}

//CHECK warming up time
void warm_up_chk() {
  if (current_time < warm_up_time) {
    status_sensor = 0;
  }
  else if (current_time >= warm_up_time && status_sensor == 0) {
    status_sensor = 1;
    sensor_reset();
  }
}

//sensor reset to get cal_A value. cal_A value is defined after sensor_reset. it is changed by this function and auto_calib function
void sensor_reset() {
  if (cal_A_LOCK == 0) {
    THER_ini = THER_data;
    EMF_max = EMF_data;
    THER_max = THER_data;
    ELTI = 0;
    cal_A = EMF_data + log10(Base_line) * (cal_B * cal_B_offset);
  }
}

/*  CO2 ppm calculation function
    Sequence
    1. Get EMF and THER analog value
    2. calculate THER to Celsius. 
    3. Moving average (EMF, THER)
    4. use moving averaged value to calculate CO2 concentration.
    5. apply limit value to co2_ppm, under, upper.
    6. apply under_cut, naturally, in Earth, there is no ppm under Earth's CO2 ppm. so if 
       CO2_data shows under Earth's CO2 ppm, it is incorrect value. so check it and update. 
*/ 
void ppm_cal() {
  EMF = analogRead(EMF_pin);
  EMF = EMF / 1024; // 10 bits, Change the number if your MCU have other resolution
  EMF = EMF * 5;    // 5V     , Change the number if your MCU have other voltage
  EMF = EMF / 6;    //OPAMP   , Don't change!
  EMF = EMF * 1000;  //V to mV conversion
  delay(1);
  THER = analogRead(THER_pin);
  THER = 1 / (C1 + C2 * log((Resist_0 * THER) / (1024 - THER)) + C3 * pow(log((Resist_0 * THER) / (1024 - THER)), 3)) - 273.15;
  delay(1);

  // Moving Average START --->
  m_averaging_data[averaged_count][0] = EMF;
  m_averaging_data[averaged_count][1] = THER;

  if (averaged_count < averaging_count) {
    averaged_count++;
  }
  else if (averaged_count >= averaging_count) {
    for (int i = 0; i < averaging_count; i++) {
      sum_data[0] = sum_data[0] + m_averaging_data[i][0]; //EMF
      sum_data[1] = sum_data[1] + m_averaging_data[i][1]; //THER
      for (int j = 0; j < 2; j++) {
        m_averaging_data[i][j] = m_averaging_data[i + 1][j];
      }
    }
    EMF_data = sum_data[0] / averaging_count;
    THER_data = sum_data[1] / averaging_count;

    sum_data[0] = 0;
    sum_data[1] = 0;
  }
  // <---Moving Average END

  // CO2 Concentration Calculation START --->
  co2_ppm = pow(10, ((cal_A - (EMF_data + DEDT * (THER_ini - THER_data))) / (cal_B * cal_B_offset)));
  co2_ppm = co2_ppm * 100 / 100;
  if (co2_ppm > max_co2) {
    co2_ppm_output = max_co2;
  }
  else if (co2_ppm <= Base_line) {
    co2_ppm_output = Base_line;
  }
  else {
    co2_ppm_output = co2_ppm;
  }

  if (co2_ppm <= Base_line * under_cut) {
    under_cut_count++;
    if (under_cut_count > 5) {
      under_cut_count = 0;
      sensor_reset();
    }
  }
  else {
    // do nothing
  }
  // <--- CO2 Concentration Calculation END
}

// divide steps of CO2 conc. you can you status_step_CD value to display data of your system. 
// like LED color or number of LED. 
void step_cal_CD() {
  if (status_sensor == 1) {
    if (co2_ppm < CD1) {
      status_step_CD = 0;
    }
    else if (co2_ppm >= CD1 && co2_ppm < CD2) {
      status_step_CD = 1;
    }
    else if (co2_ppm >= CD2 && co2_ppm < CD3) {
      status_step_CD = 2;
    }
    else if (co2_ppm >= CD3 && co2_ppm < CD4) {
      status_step_CD = 3;
    }
    else if (co2_ppm >= CD4) {
      status_step_CD = 4;
    }
  }
}

/* 
  The sensitivity of sensors decreases with use. To compensate for this, 
  a function call auto_calibration(ABC) logic is used, which is the algorithm used by
  all CO2 sensor companies. 

  it is very simple logic. Since the lowest value that can be displayed by the sensor is
  the Earth's CO2 concentration, the lowest value among the CO2 concentration values measured
  during a specific period is calculated to have the Earth's CO2 concentration. 

  if 500 ppm is the lowest value of sensor for 1 day (or 1 week, specific period). calculate 500 ppm as Earth's CO2_ppm.
  update cal_A value to make 500 ppm to Earth's CO2 ppm. it is simple and powerful at zero point adjustment.
*/
void auto_calib_co2() {
  if (current_time < start_stablize && MEIN_flag == 0) {
    MEIN_flag = 1;
    MEIN_common = MEIN_start;
  }
  else if (current_time >= start_stablize + 1 && MEIN_flag == 1) {
    MEIN_common = MEIN;
    //if(display_mode){Serial.println("MEIN_common = MEIN");}
  }
  if (current_time - prev_time_METI >= METI && status_sensor == 1) {
    if (ELTI < MEIN_common) {
      if (cal_A_LOCK == 0) {
        ELTI++;
      }
      else {
        LOCK_timer_cnt++;
      }
    }
    else if (ELTI >= MEIN_common) {
      if (cal_A_LOCK == 0) {
        cal_A = (EMF_max + DEDT * (THER_max - THER_data)) + log10(Base_line) * (cal_B * cal_B_offset);
        THER_ini = THER_data;
        EMF_max = EMF_data;
        THER_max = THER_data;
        ELTI = 0;
      }
      if (damage_cnt_fg == 1)
      {
        damage_cnt++;
      }
    }
    if (EMF_max >= EMF_data) {
      upper_cut = 0;
    }
    else if (EMF_max < EMF_data) {
      upper_cut++;
      if (upper_cut > 3) {
        EMF_max = EMF_data;
        THER_max = THER_data;
        upper_cut = 0;
      }
    }
    prev_time_METI = current_time;
  }
}

/*
    순간적으로 강한 충격을 받은 경우 해당 
    2가지 방식으로 케어함
    1. 7초 전의 센서 원신호(EMF)와 현재의 센서 원신호의 값의 차가 LOCK_delta 값보다 크면 (현재 값 - 과거(7초 전)값 >= LOCK_delta)
      
*/


void DMG_REC() {
  for (int i = 0; i < 10; i++) {
    cal_A_LOG[0][i] = cal_A_LOG[0][i + 1];
    cal_A_LOG[1][i] = cal_A_LOG[1][i + 1];
  }
  cal_A_LOG[0][9] = cal_A;
  cal_A_LOG[1][9] = EMF_data;
  if (status_sensor == 1) {
    if ((cal_A_LOG[1][9] - cal_A_LOG[1][2] >= LOCK_delta) && (cal_A_LOG[1][8] - cal_A_LOG[1][1] >= LOCK_delta) && (cal_A_LOG[1][7] - cal_A_LOG[1][0] >= LOCK_delta)) {
      if (cal_A_LOCK == 0) {
        cal_A_LOCK = 1;
        cal_A_LOCK_value = cal_A_LOG[0][0];
        emf_LOCK_value = cal_A_LOG[1][0];
        cal_A = cal_A_LOCK_value;
        //if(debug); Serial.println("S1 ---- cal_A_LOG[1][0] = " + cal_A_LOG[1][0] + "cal_A_LOG[1][9] = " + cal_A_LOG[1][9]);
      }
    }
    else if ((cal_A_LOG[1][2] > EMF_DMG - LOCK_delta) && (cal_A_LOG[1][1] > EMF_DMG - LOCK_delta) && (cal_A_LOG[1][0] > EMF_DMG - LOCK_delta) && (cal_A_LOG[1][2] <= EMF_DMG - LOCK_disable) && (cal_A_LOG[1][1] <= EMF_DMG - LOCK_disable) && (cal_A_LOG[1][0] <= EMF_DMG - LOCK_disable)) {
      if ((cal_A_LOG[1][7] > EMF_DMG) && (cal_A_LOG[1][8] > EMF_DMG) && (cal_A_LOG[1][9] > EMF_DMG)) {
        if (cal_A_LOCK == 0) {
          cal_A_LOCK = 1;
          cal_A_LOCK_value = cal_A_LOG[0][0];
          emf_LOCK_value = cal_A_LOG[1][0];
          cal_A = cal_A_LOCK_value;
          //if(debug); Serial.println("S2 ---- cal_A_LOG[1][0] = " + cal_A_LOG[1][0] + "cal_A_LOG[1][9] = " + cal_A_LOG[1][9]);
        }
      }
    }
    else {
      //do nothing
    }
  }
  if (cal_A_LOCK == 1) {
    if (cal_A_LOG[1][9] - emf_LOCK_value < LOCK_disable) {
      S3_cnt++;
      if (S3_cnt >= 10) {
        S3_cnt = 0;
        cal_A_LOCK = 0;
        ELTI = 0;
        THER_ini = THER_data;
        EMF_max = EMF_data;
        THER_max = THER_data;
        LOCK_timer_cnt = 0;
      }
      else {
        //do nothing
      }
    }
    else if (LOCK_timer_cnt >= LOCK_timer) {
      cal_A_LOCK = 0;
      ELTI = 0;
      THER_ini = THER_data;
      EMF_max = EMF_data;
      THER_max = THER_data;
      LOCK_timer_cnt = 0;
    }
    else {
      S3_cnt = 0;
    }
  }
  else {
    //do nothing
  }
}


/*
      display_mode = 0: CO2 value + Warm up status only.
      display_mode = 1: CO2 value + EMF, THER, Warm up status
*/
void display_data() {
  if (display_mode == 0) {
    Serial.print("# ");
    if (co2_ppm <= 999) {
      Serial.print("0");
      Serial.print(co2_ppm_output, 0);
    }
    else {
      Serial.print(co2_ppm_output, 0);
    }
    Serial.print(" ");

    if (status_sensor == 0) {
      Serial.print("WU");
    }
    else {
      Serial.print("NR");
    }
    Serial.println("");
  }
  else if (display_mode == 1) {
    Serial.print("T ");
    Serial.print(current_time);
    Serial.print(" # ");
    if (co2_ppm <= 999) {
      Serial.print("0");
      Serial.print(co2_ppm, 0);
    }
    else {
      Serial.print(co2_ppm, 0);
    }
    Serial.print(" | CS: ");
    Serial.print(status_step_CD);
    Serial.print(" | ");
    Serial.print(EMF_data);
    Serial.print(" mV | ");
    Serial.print(THER_data);

    if (status_sensor == 0) {
      Serial.print(" oC | WU");
    }
    else {
      Serial.print(" | NR");
    }
    Serial.println("");
  }
}

/*  
    max_co2가 5000보다 낮은 경우에는 작동하지 않습니다. 
    일반적으로 co2 농도가 5000보다 높은 경우는 센서가 고농도 화학물질에 의해 데미지를 입었을 경우가 많으므로
    이 경우에는 자동보정 주기를 현저히 줄여서, 센서 값을 안정화시키려고 합니다. 

    5000 ppm을 넘는 값이 60초 이상 유지되면, 자동보정주기를 3분으로 변경하여 5회 실시함
    자동보정 진행 후 자동보정 주기를 원래의 주기로 변경(MEIN_rec)
*/
void DMG_5000()
{
  if (status_sensor == 1) {
    if (co2_ppm_output >= 5000) {
      if (ppm_max_cnt > 60) {
        MEIN = 3;
        damage_cnt_fg = 1;
        ppm_max_cnt = 0;
      }
      else {
        ppm_max_cnt++;
      }
    }
    else {
      ppm_max_cnt = 0;
    }
  }
  if (damage_cnt > 5) {
    MEIN = MEIN_rec;
    damage_cnt = 0;
    damage_cnt_fg = 0;
  }
  else {
    //do nothing
  }
}
