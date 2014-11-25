#!/usr/bin/python

import os
import sys
import BaseHTTPServer

TEMPFILENAME="tmp.wav"
COMMANDNAME="perl -i -ne '/root/&&print' %s"

class PhoneMeBaseHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_POST(self):
        contentLength = int(self.headers.get('Content-Length', '0'))
        content = self.rfile.read(contentLength)
        if len(content) != contentLength:
            print >>sys.stderr, "contentLength != <data-length>"
            return

        with open(TEMPFILENAME, "wb") as fd:
            fd.write(content)

        cmd = COMMANDNAME % os.path.abspath(TEMPFILENAME)

        print(cmd)

        if os.system(cmd) != 0:
            print("cmd failed with nonnull")

        with open(TEMPFILENAME, "rb") as fd:
            self.wfile.write(fd.read())


server_address = ('', 19999)

httpd = BaseHTTPServer.HTTPServer( server_address, PhoneMeBaseHandler );

sa = httpd.socket.getsockname()
print "Serving HTTP on", sa[0], "port", sa[1]

httpd.serve_forever()
