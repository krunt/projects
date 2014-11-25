#!/bin/bash


curl "http://localhost:19999" \
    --data-binary @passwd \
    --request POST
