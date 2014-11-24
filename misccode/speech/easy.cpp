#include <stdio.h>
#include <string.h>
#include <sstream>
#include <iostream>

#include <curl/curl.h>

#include <jsoncpp/json.hpp>

const char *OATH_REQUEST = "client_id=5p8eyqkvfynsw9mlngcvrs6t1d1saxmp&client_secret=0q4z4cgxonq50imw0ohinehpduj8updb&scope=TTS&grant_type=client_credentials";

size_t WriteResponseData(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  std::stringstream* s = (std::stringstream*)userdata;
  size_t n = size * nmemb;  s->write(ptr, n);
  return n;
} 

static int bytesRead = 0;
size_t ReadRequestData(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    int toRead = 0;

    if ( bytesRead == strlen( OATH_REQUEST ) )
        return 0;

    toRead = std::min( size * nmemb, strlen( OATH_REQUEST ) - bytesRead );
    memcpy( ptr, OATH_REQUEST + bytesRead, toRead );
    bytesRead += toRead;

    return bytesRead;
}


int main(void)
{
  CURL *curl;
  CURLcode res;
  struct curl_slist *slist = NULL;
 
  curl = curl_easy_init();
  if(curl) {
      curl_easy_setopt(curl, CURLOPT_HEADER, 1);
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.att.com/oauth/v4/token");
    /* example.com is redirected, so we tell libcurl to follow redirection */ 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    slist = curl_slist_append(slist, 
        "Content-Type: application/x-www-form-urlencoded");
    slist = curl_slist_append(slist, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

    std::stringstream contentStream;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteResponseData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &contentStream);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &ReadRequestData);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(OATH_REQUEST));
    curl_easy_setopt(curl, CURLOPT_READDATA, NULL);
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);
    /* Check for errors */ 
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    fprintf( stderr, "read %d bytes, written %d bytes\n", 
            contentStream.str().length(), bytesRead );

    //std::cout << contentStream.str() << std::endl;
 
    /* always cleanup */ 
    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);
  }
  return 0;
}
