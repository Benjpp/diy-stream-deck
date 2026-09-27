#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include "mqtt/async_client.h"

#define PORT 1883

using namespace std;

const string SERVER_ADDRESS = "tcp://broker:1883";
const string CLIENT_ID = "stream-deck-controller";
const string TOPIC = "homelab/stream-deck";
const int QOS = 1;
const int TIMEOUT = 10000;

class callback : public virtual mqtt::callback
{
    u_int32_t n_retries;

    public:
        void connection_lost(const string& cause) override
        {
            cout << "Connection lost. Cause: " << cause << endl; 
        }

        void message_arrived(mqtt::const_message_ptr msg) override
        {
            cout << "Message arrived: " << msg->get_payload() << endl;
        }

        void delivery_complete(mqtt::delivery_token_ptr tok) override
        {
            cout << "Delivery complete" << endl;
        }
};

int main(int argc, char* argv[]){    
    cout << "================================================================" << endl;
    cout << "=====================STARTING MQTT LISTENER=====================" << endl;
    cout << "================================================================" << endl;

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try{
        callback cb;
        client.set_callback(cb);

        mqtt::token_ptr connectionToken = client.connect(connOpts);
        connectionToken->wait();

        mqtt::token_ptr subToken = client.subscribe(TOPIC, QOS);
        subToken->wait();

        while (true)
        {
            // Wait for messages
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        mqtt::token_ptr disconnectionToken = client.disconnect();
        disconnectionToken->wait();
    }catch (const mqtt::exception& ex){
        cout << "================================================================" << endl;
        std::cerr << "MQTT Exception: " << ex.what() << std::endl;
        cout << "================================================================" << endl;
        return 1;
    }

    return 0;
}