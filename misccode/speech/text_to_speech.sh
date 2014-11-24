#!/bin/bash
if [ $# != 2 ] ; then
    echo "Usage: $0 <access-token> <test-text>"
    exit 1
fi

set -x

. attcreden

curl -o out.wav "https://api.att.com/speech/v3/textToSpeech" \
    --header "Authorization: Bearer $1" \
    --header "Accept: audio/amr" \
    --header "Content-Type: text/plain" \
    --data $"$2" \
    --request POST

echo
