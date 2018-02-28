#!/usr/bin/env bash
#David Mandelbaum
#Last updated: 2.24.18
echo '####################################################################################################################################################################################################'
echo 'BEGINNING PROGRAM'
./server 4444 . 4 10 HPIC &

./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /index.html &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /nigel.jpg &
./client localhost 4444 /nigel.jpg &

sleep 1
echo 'ENDING PROGRAM' 

echo '####################################################################################################################################################################################################' 
