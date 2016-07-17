#g++ -std=c++0x server.cpp -o test.out -lboost_system -lboost_thread -lboost_filesystem -lpthread -lssl -lcrypto -lglog -lgflags -Wl,-rpath,/usr/local/lib
g++ -std=c++0x client.cpp -o test.out -lfolly -lpthread -lgflags -lglog -lwangle -lssl -lcrypto
