#include "mbed.h"
#include "ESP8266Interface.h"
 
// Library to use https://github.com/ARMmbed/mbed-mqtt
#include <MQTTClientMbedOs.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ratio>
using namespace std::chrono;

#define MEASURE_TIME_INTERVAL               50
#define MQTT_ID                             MBED_CONF_APP_MQTT_DEVICE_ID
#define MQTT_USERNAME                       MBED_CONF_APP_MQTT_DEVICE_USERNAME
#define MQTT_PASSWORD                       MBED_CONF_APP_MQTT_DEVICE_PASSWORD

int rotation_time;
int rotation_time_new;
int direction = 0;
bool connect_to_wifi = true;
bool connect_to_mqtt = true;

//Pins
AnalogIn diode(A0);
DigitalIn button(D6);
ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);
PwmOut motor_en(D10);
DigitalOut motor_dir(D9);

// Variable for the pwm
float usT = 50;     // f = 10000 Hz  T = 100 us
float speed = 1.0f; //speed 0.0 - 1.0

//timer(s)
Timer total;
Timer ft;

//Data variables
int *values = new int[1024];
int *ma_values = new int[1024];
int times[1024];
int counter;
int current_value;
int max_value = 0;
int end_value = 0;
int max_time;
int code;

SocketAddress MQTTBroker;
TCPSocket socket;
MQTTClient client(&socket);
MQTTPacket_connectData data;

//functions
void mqtt_send(int max_value, int end_value);
void wifi_connect();
void moving_average(int *values, int *ma_arr, int len, int range);
int get_max(int *arr, int len);

int main() 
{   
    motor_dir.write(direction);
    motor_en.period_us(usT);

    ThisThread::sleep_for(2s);
    if (button.read()) {
        printf("Hold the button down until the mirror has rotated full cycle\n");
        ThisThread::sleep_for(2s);
        motor_en.write(speed);
        ft.start();
        while(button.read());
        ft.stop();
        motor_en.write(0);
        rotation_time = (duration_cast<milliseconds>(ft.elapsed_time()).count());
        printf("rotation time: %d\n", rotation_time);
    } else { 
        rotation_time = 21210; 
    }

    if (connect_to_wifi) {
        wifi_connect();

        //setup mqtt
        data = MQTTPacket_connectData_initializer;
        esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
        MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);
        data.MQTTVersion = 3;
        data.clientID.cstring = MQTT_ID;
        data.username.cstring = MQTT_USERNAME;
        data.password.cstring = MQTT_PASSWORD;
    }
    
    //main loop
    counter = 0;
    while(1) {
        rotation_time_new = rotation_time;

        motor_en.write(speed);
        ft.start();
        total.start();
        while(duration_cast<milliseconds>(total.elapsed_time()).count() < rotation_time) {
            ft.reset();
            current_value = (int)(1023*diode.read());
            //Save values and times to arrays
            values[counter] = current_value;
            times[counter] = (duration_cast<milliseconds>(total.elapsed_time()).count());
            //keep track of max value
            //max_value = max_value < current_value ? current_value : max_value;  
            printf("values: %d    n: %d\n", current_value, counter);
            counter++;
            ThisThread::sleep_for(MEASURE_TIME_INTERVAL - duration_cast<milliseconds>(ft.elapsed_time()).count());
        }
        total.stop();
        total.reset();
        ft.stop();
        ft.reset();
        motor_en.write(0);
        printf("main loop end");
        //printf("Initial rotation took %llu ms\n", duration_cast<milliseconds>(total.elapsed_time()).count());

        moving_average(values, ma_values, counter, 3);
        int index = get_max(ma_values, counter);
        max_value = ma_values[index];
        max_time = times[index];

        while(1) {
            if (rotation_time_new < 500) {
                total.start();
                if (connect_to_mqtt) {
                    mqtt_send(max_value, end_value); 
                }
                while(max_value - (int)(1023*diode.read()) < max_value*0.5) {
                    if (duration_cast<seconds>(total.elapsed_time()).count() > 120) {
                        total.stop();
                        total.reset();
                        break;
                    }
                }
                break;
            }
                    
            counter = 0;
            motor_en.write(0);
            ThisThread::sleep_for(100ms);
            direction = !direction;
            motor_dir.write(direction);

            rotation_time_new = (rotation_time_new - max_time);
            ft.start();
            total.start();
            motor_en.write(speed);

            while(duration_cast<milliseconds>(total.elapsed_time()).count() < rotation_time_new) {
                ft.reset();
                current_value = (int)(1023*diode.read());
                //Save values and times to arrays
                values[counter] = current_value;
                times[counter] = (duration_cast<milliseconds>(total.elapsed_time()).count());
                //keep track of max value
                //max_value = max_value < current_value ? current_value : max_value;  
                printf("values: %d    n: %d\n", current_value, counter);
                counter++;
                ThisThread::sleep_for(MEASURE_TIME_INTERVAL - duration_cast<milliseconds>(ft.elapsed_time()).count());
            }
            motor_en.write(0);
            total.stop();
            total.reset();
            ft.stop();
            ft.reset();

            moving_average(values, ma_values, counter, 3);
            int index = get_max(ma_values, counter);
            max_value = ma_values[index];
            max_time = times[index];
        }
        printf("max value: %d \n", max_value);
        printf("max value index: %d \n", index);
        printf("max value time: %d \n", max_time);        
    }        
}
int get_max(int *arr, int len) {
    int index = 0;
    for (int i = 0; i <= len; i++) {
        if (arr[i] > arr[index]) {
            index = i;
        }
    }
    return index;
}
void moving_average(int *values, int *ma_arr, int len, int range) {
    int sum = 0;
    int x = 0;
    for (int i = 0; i <= (len-1); i++) {
        sum = 0;
        for (int y = -range/2; y <= range/2; y++) {
            x=i+y;
            if (x < len-range/2 && x > range/2 - 1) {
            } else if (x < 0) {
                x = len + x;
            } else if (x > (len-1)) {
                x = x - (len-1) - 1;
            } 
            sum = sum + values[x];   
        }
        ma_arr[i] = sum/range;
    }
}

void wifi_connect() {
    //Store device IP
    SocketAddress deviceIP;
    printf("\nConnecting wifi..\n");
 
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if(ret != 0) {
        printf("\nConnection error\n");
    } else {
        printf("\nConnection success\n");
    }

    esp.get_ip_address(&deviceIP);
    printf("IP via DHCP: %s\n", deviceIP.get_ip_address());
}

void mqtt_send(int max_value, int end_value) {
/*opens socket and connect to mqtt broker, sends the data and closes the connection*/
    socket.open(&esp); //open a socket in specified network interface, wifi card in this case
    socket.connect(MQTTBroker); //connect to a remote host using socket opened in the network interface
    char buffer[64];   
    //sprintf(buffer, "Hello from Mbed OS %d.%d", MBED_MAJOR_VERSION, MBED_MINOR_VERSION);
    sprintf(buffer, "{\n\"end_value\": %d,\n\"max_value\": %d}", max_value, end_value); //formatting to JSON string 

    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buffer;
    msg.payloadlen = strlen(buffer);

    if (!client.isConnected()) {
        code = client.connect(data) == 0 ? printf("connected\n") : printf("connect error: %d\n", code);
    }
    code = client.publish(MBED_CONF_APP_MQTT_TOPIC, msg) == 0 ? printf("data sent\n") : printf("publish error: %d", code);
    code = client.disconnect() == 0 ? printf("disconnected\n") : printf("disconnect error: %d\n", code);

    
} 