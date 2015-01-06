#!/bin/bash


curl "http://192.168.0.100:19999" \
    --data-binary @hello.wav \
    --request POST
