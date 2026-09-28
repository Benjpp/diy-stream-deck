CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I../esp32-client/include
LDFLAGS = -lpaho-mqttpp3 -lpaho-mqtt3as -lpthread

TARGET = listener
SRC = src/mqttListener.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
