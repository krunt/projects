#!/bin/bash
if [ $# != 2 ] ; then
    echo "Usage: $0 <access-token> <file-path>"
    exit 1
fi

set -x

. attcreden

curl "https://api.att.com/speech/v3/speechToText" \
        --header "Authorization: Bearer $1" \
        --header "Accept: application/json" \
        --header "Content-Type: audio/wav" \
        --data-binary @"$2" \
        --request POST

echo
