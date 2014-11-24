#!/bin/bash

. attcreden

curl "https://api.att.com/oauth/v4/token" \
    --header "Content-Type: application/x-www-form-urlencoded" \
    --header "Accept: application/json" \
    --data "client_id=${S2T_APP_KEY}&client_secret=${S2T_APP_SECRET}&scope=SPEECH&grant_type=client_credentials" \
    --request POST

echo
