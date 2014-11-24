#!/bin/bash

. attcreden

curl "https://api.att.com/oauth/v4/token" \
    --header "Content-Type: application/x-www-form-urlencoded" \
    --header "Accept: application/json" \
    --data "client_id=${T2S_APP_KEY}&client_secret=${T2S_APP_SECRET}&scope=TTS&grant_type=client_credentials" \
    --request POST

echo
