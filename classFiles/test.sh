#!/usr/bin/env bash
#David Mandelbaum
#Last updated: 2.24.18
echo '####################################################################################################################################################################################################'
echo 'BEGINNING PROGRAM'
./server 8000 . 4 10 HPIC &

./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /index.html &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /nigel.jpg &
./client localhost 8000 /nigel.jpg &

sleep 1
echo 'ENDING PROGRAM' 

echo '####################################################################################################################################################################################################' 
