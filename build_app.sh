#!/bin/bush

sudo apt-get update
sudo apt-get install g++

g++ ./server.cpp -o server
g++ ./client.cpp -o client
mkdir ./savedir

echo "app build success"
