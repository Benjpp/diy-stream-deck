#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stack>
#include <cstdio>
#include <vector>
#include "mqtt/async_client.h"
#include "commands.h"

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
    stack<string> container_stack;

    vector<string> get_command_output(string cmd) {
        vector<string> output;
        char buffer[128];
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return output;

        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            string line = buffer;
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty()) output.push_back(line);
        }
        pclose(pipe);
        return output;
    }

    public:
        void connection_lost(const string& cause) override
        {
            cout << "Connection lost. Cause: " << cause << endl;
        }

        void message_arrived(mqtt::const_message_ptr msg) override
        {
            cout << "Message arrived: " << msg->to_string() << endl;
            switch (stoi(msg->to_string())){
            case RESTART_CONTAINERS:
            {
                cout << "Restarting containers..." << endl;
                if(system("docker ps -q | grep -vE \"$(docker ps -aqf \"name=^/mqtt.*\")\" | xargs -r docker restart") == 0){
                    cout << "Containers have been restarted sucesfully" << endl;
                }else{
                    cout << "There was an error restarting prod docker containers" << endl;
                }
                break;
            }
            
            case STOP_CONTAINERS:
            {
                cout << "Stopping containers..." << endl;
                string stop_filter = "docker ps -q | grep -vE \"$(docker ps -aqf \"name=^/mqtt.*\")\"";
                vector<string> ids = get_command_output(stop_filter);

                for (const string& id : ids) {
                    container_stack.push(id);
                }

                if(system("docker stop $(docker ps -q | grep -vE \"$(docker ps -aqf \"name=^/mqtt.*\")\") 2>/dev/null") == 0){
                    cout << "Containers have been stopped succesfully" << endl;
                }else{
                    cout << "There was an error stopping the containers" << endl;
                }
                break;
            }

            case START_CONTAINERS:
            {
                cout << "Starting containers..." << endl;
                if (container_stack.empty()) {
                    cout << "Stack is empty, nothing to start." << endl;
                    break;
                }

                string start_cmd = "docker start ";
                while (!container_stack.empty()) {
                    start_cmd += container_stack.top() + " ";
                    container_stack.pop();
                }

                if(system(start_cmd.c_str()) == 0){
                    cout << "Containers have started succesfully" << endl;
                }else{
                    cout << "There was an error starting the containers" << endl;
                }
                break;
            }
            
            default:
                cout << "Unkown cmd received. Continuing." << endl;
                break;
            }
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