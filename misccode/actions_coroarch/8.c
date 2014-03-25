#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <curl/curl.h>

#include <EXTERN.h>
#include <perl.h>

#include "libasync.h"

void h1(void *arg) {
    int i = 0, fd, outfd, rc;
    char buf[1024];

    for (; i < 10000000; ++i) {}

    if ((fd = open("/etc/passwd", O_RDONLY)) == -1) {
        return;
    }

    if ((outfd = open("/dev/null", O_WRONLY)) == -1) {
        return;
    }

    while ((rc = read(fd, buf, sizeof(buf)))) {
        if (rc == -1)
            return;
        write(outfd, buf, sizeof(buf));
    }

    for (; i < 10000000; ++i) {}

    close(fd);
}

void h2(void *arg) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:19700/");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

EXTERN_C void boot_Data__Dumper (pTHX_ CV* cv);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

static void
mine_xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

static PerlInterpreter *my_perl;

EXTERN_C void xs_init (pTHX);

static 
void call_dump_perl(void *sv) {
    dSP;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVsv((SV *)sv)));
    PUTBACK;

    call_pv("dump_perl", G_DISCARD);

    FREETMPS;
    LEAVE;
}

void h3(void *arg) {
    int argc = 3;
    char *argv[] = { "", "-e", "use Data::Dumper;"
        "sub dump_perl { print STDERR Data::Dumper::Dumper([shift]); }", 
            NULL };
    char *env[] = { NULL };
    void *original_context = PERL_GET_CONTEXT;
    SV *sv;

    PERL_SYS_INIT3(&argc,&argv,&env);
    my_perl = perl_alloc();

    sv = newRV_inc(newSViv(5));

    PERL_SET_CONTEXT(my_perl);
    perl_construct(my_perl);
    
    perl_parse(my_perl, mine_xs_init, argc, argv, NULL);

    call_dump_perl(sv);
    
    perl_destruct(my_perl);
    perl_free(my_perl);

    PERL_SET_CONTEXT(original_context);
}

int main() {
    int i = 0;
    async_state_t state;

    async_init(&state);

    async_push(&state, get_action(h3));

    async_loop(&state);

    return 0;
}
