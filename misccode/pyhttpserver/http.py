#!/usr/bin/python

import os
import sys
import BaseHTTPServer

PRETEMPFILENAME="tmp.tmp.wav"
TEMPFILENAME="tmp.wav"
TRANSCODENAME="ffmpeg.exe -i %s -y -ar 22050 -ac 1 -ab 64 %s"
COMMANDNAME="perl -i -ne '/root/&&print' %s"

class PhoneMeBaseHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_POST(self):
        contentLength = int(self.headers.get('Content-Length', '0'))
        content = self.rfile.read(contentLength)
        if len(content) != contentLength:
            print >>sys.stderr, "contentLength != <data-length>"
            return

        with open(PRETEMPFILENAME, "wb") as fd:
            fd.write(content)

        cmd = TRANSCODENAME % (os.path.abspath(PRETEMPFILENAME), 
            os.path.abspath(TEMPFILENAME))
        cmd2 = COMMANDNAME % os.path.abspath(TEMPFILENAME)

        print(cmd)
        if os.system(cmd) != 0:
            print("cmd failed with nonnull")

        print(cmd2)
        if os.system(cmd2) != 0:
            print("cmd2 failed with nonnull")

        with open(TEMPFILENAME, "rb") as fd:
            self.wfile.write(fd.read())


server_address = ('', 19999)

httpd = BaseHTTPServer.HTTPServer( server_address, PhoneMeBaseHandler );

sa = httpd.socket.getsockname()
print "Serving HTTP on", sa[0], "port", sa[1]

httpd.serve_forever()
