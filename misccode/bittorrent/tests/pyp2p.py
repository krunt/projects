#!/usr/bin/python

import os
import sys
import time
import socket
import select
import struct
import random
import string
import urlparse
import bencode
import logging
import threading
import signal
import SocketServer
import BaseHTTPServer

from errno import EAGAIN, EWOULDBLOCK, ECONNABORTED

# bittorrent p2p

HTTP_TRACKER_PORT = 13101
UDP_TRACKER_PORT = 13101

HTTP_PEER_BASE_PORT = 13200
HTTP_PEER_COUNT = 1

UDP_PEER_BASE_PORT = 13300
UDP_PEER_COUNT = 1

HTTP_TYPE = 0 
UDP_TYPE = 1

PIECE_PART_SIZE = 16384

# packet types
K_KEEPALIVE = -1
K_CHOKE = 0
K_UNCHOKE = 1
K_INTERESTED = 2
K_NOT_INTERESTED = 3
K_HAVE = 4
K_BITFIELD = 5
K_REQUEST = 6
K_PIECE = 7
K_CANCEL = 8


logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
logger.addHandler(logging.StreamHandler(sys.stderr))

peer_list = []
peer_threads = []

class TorrentInfo:
    def __init__(self, encoded_info):
        root = bencode.decode(encoded_info)
        info = root["info"]
        self.fname = info["name"]
        self.flength = int(info["length"])
        self.piece_length = int(info["piece length"])
        self.pieces = []
        for i in range(0, len(info["pieces"]), 20):
            self.pieces.append(info["pieces"][i:i+20])
    
    def get_piece_fname(self):
        return self.fname

    def get_piece_length(self):
        return self.piece_length

    def get_piece_count(self):
        return len(self.pieces)


class ConnectionClosed(Exception):
    pass


class Packet:
    def __init__(self, t, body, length):
        self.t = t
        self.body = body
        self.length = length

    def get_type(self):
        return self.t

    def get_body(self):
        return self.body

    def get_length(self):
        return self.length


class PacketStream:

    k_reading_header = 0
    k_reading_body = 1

    def __init__(self):
        self.state = self.k_reading_header
        self.pending_count = 4
        self.pending = ""
        self.packets = []

    def _feed(self, buf):
        while len(buf) > 0:

            to_read = min(len(buf), self.pending_count)

            self.pending += buf[0:to_read]
            buf = buf[to_read:]

            self.pending_count -= to_read

            logger.debug("pending %d bytes" % self.pending_count)

            if self.pending_count == 0:
                if self.state == self.k_reading_header:
                    self.state = self.k_reading_body
                    self.pending_count = struct.unpack('>I', self.pending[:4])[0]
                    logger.debug("got header with %d bytes" % self.pending_count)
                else:
                    packet_length = struct.unpack('>I', self.pending[:4])[0]
                    t = K_KEEPALIVE if packet_length == 0 \
                            else struct.unpack('B', self.pending[4])[0]

                    self.packets.append(Packet(t, self.pending[1+4:], 
                        packet_length))

                    self.pending = ""
                    self.state = self.k_reading_header
                    self.pending_count = 4


    def feed(self, fd):

        while True:
            buf = ""
            try:
                buf = fd.recv(1024)
            except socket.error as err:
                if err.args[0] in (EAGAIN, EWOULDBLOCK):
                    return
                else:
                    raise

            if len(buf) == 0:
                raise ConnectionClosed()

            logger.debug("feeding %d bytes to packet-stream", len(buf))
            self._feed(buf)
            

    def get_packets(self):
        return self.packets

    def clear_packets(self):
        self.packets = []

    def have_available(self):
        return len(self.packets) > 0


class PeerRequestHandler(SocketServer.StreamRequestHandler):

    def handshake(self):
        peer = self.server
        b = self.rfile.read(1 + 19 + 8 + 20 + 20)

        logger.debug("%s: got handshake, sending response" % peer.get_id())

        if b[0] != '\x13' or b[1:20] != 'BitTorrent protocol':
            logger.error("magic mismatch")
            return

        # patching id
        newb = b[0:48]
        newb += peer.get_id()

        self.wfile.write(newb)
        self.wfile.flush()

        logger.debug("handshake %s: sent %d bytes" 
                % (peer.get_id(), len(newb)))


    # t - type, b - buf
    def _send_packet(self, t, b):
        buf = struct.pack('>I', 1 + len(b))
        buf = buf + struct.pack('B', t) + b
        self.wfile.write(buf)
        self.wfile.flush()

    def _enqueue_to_send_queue(self, t, b):
        buf = struct.pack('>I', 1 + len(b))
        buf = buf + struct.pack('B', t) + b
        self.send_queue.append(buf)

    def send_bitmap(self):
        peer = self.server          
        bm = peer.get_bitmap()

        res = ""
        for i in xrange(0, len(bm), 8):
            b = 0
            for j in range(8)[::-1]:
                if i + j < len(bm) and bm[i + j]:
                    b |= 1
                b <<= 1
            res += struct.pack('B', b >> 1)

        logger.debug("sent bitfield with length=%d" % len(res))
        self._send_packet(K_BITFIELD, res)

    def send_unchoke(self):
        self._send_packet(K_UNCHOKE, "")

    def handle_packet(self, p):
        peer = self.server
        body = p.get_body()

        logger.info("got packet with %d type, %d body-size" 
                % (p.get_type(), len(body)))

        if p.get_type() == K_REQUEST:

            piece_index = struct.unpack('>I', body[:4])[0]
            piece_offset = struct.unpack('>I', body[4:8])[0]
            length = struct.unpack('>I', body[8:12])[0]

            logger.info("trying to get piece with id=%d,offset=%d,len=%d"
                    % (piece_index, piece_offset, length))

            # unexpected length arrived
            if length != PIECE_PART_SIZE:
                logger.warn("length != piece_part_size")
                raise ConnectionClosed

            if piece_offset % PIECE_PART_SIZE != 0:
                logger.warn("piece % piece_part_size != 0")
                raise ConnectionClosed

            piece_part = peer.get_piece_part(piece_index, 
                    piece_offset / PIECE_PART_SIZE)

            piece_part = body[:8] + piece_part

            logger.info("sending piece with id=%d,offset=%d,len=%d"
                    % (piece_index, piece_offset, len(piece_part)))

            self._enqueue_to_send_queue(K_PIECE, piece_part)


    def handle_send_queue(self, fd):
        q = self.send_queue

        while len(q) > 0:
            if len(q[0]) == 0:
                q = q[1:]
                continue

            try:
                rc = fd.send(q[0])
                q[0] = q[0][rc:]
            except socket.error as err:
                if err.args[0] in (EAGAIN, EWOULDBLOCK):
                    return
                else:
                    raise
            finally:
                self.send_queue = q


    def maybe_send_some_requests(self):
        if random.randint(1,2000) < 3:
            for i in range(random.randint(1,5)):
                piece_index = random.randint(0, 
                        self.server.get_torrent_info().get_piece_count() - 1)

                piece_part_index = random.randint(0, 
                    self.server.get_torrent_info().get_piece_length() 
                    / PIECE_PART_SIZE  - 1)

                body = struct.pack('>III', 
                    piece_index, piece_part_index, PIECE_PART_SIZE)

                self._enqueue_to_send_queue(K_REQUEST, body)



    def handle_incoming(self):
        self.send_queue = []
        peer = self.server
        fd = self.request

        fd.setblocking(0)

        packet_stream = PacketStream()
        while True:

            closed = False

            read, write, err = select.select([fd], [fd], [], 0.25)

            if fd in write:
                self.handle_send_queue(fd)

            self.maybe_send_some_requests()

            if fd not in read:
                continue

            try:
                packet_stream.feed(fd)
            except ConnectionClosed:
                closed = True

            for p in packet_stream.get_packets():
                try:
                    self.handle_packet(p) 
                except ConnectionClosed:
                    closed = True
                    break

            packet_stream.clear_packets()

            if closed:
                logger.debug("breaking out of incoming packets loop")
                break


    def handle(self):
        logger.info("starting connection from %s" % str(self.client_address))

        self.handshake()
        self.send_bitmap()
        self.send_unchoke()
        self.handle_incoming()

        logger.info("finishing connection from %s" % str(self.client_address))


class Peer(SocketServer.TCPServer):
    def __init__(self, t, port, torrent_info):
        self.peer_id = "".join(map(\
            lambda i: string.ascii_letters[random.randint(1,20)], \
            range(0, 20)))
        self.t = t
        self.ip = '127.0.0.1'
        self.port = port
        self.address_family = socket.AF_INET
        self.allow_reuse_address = True
        self.timeout = 10
        self.disable_nagle_algorithm = 1
        self.torrent_info = torrent_info
        self.fd = open(self.torrent_info.get_piece_fname(), 'rb')
        self._generate_bitmap()

        SocketServer.TCPServer.__init__(self, 
                ('0.0.0.0', self.port), PeerRequestHandler)

    def get_piece_part(self, piece_index, part_index):
        self.fd.seek(piece_index * self.torrent_info.get_piece_length() \
                + part_index * PIECE_PART_SIZE)
        buf = self.fd.read(PIECE_PART_SIZE)
        if len(buf) != PIECE_PART_SIZE:
            buf = buf + "\0" * (PIECE_PART_SIZE - len(buf))
        return buf

    def _generate_bitmap(self):
        self.bitmap = []
#        for i in xrange(self.get_torrent_info().get_piece_count()):
#            self.bitmap.append(1 if random.randint(1,20) < 10 else 0)

        for i in xrange(self.get_torrent_info().get_piece_count()):
            self.bitmap.append(1)

        buf = ""
        for i in range(len(self.bitmap)):
            buf += "1" if self.bitmap[i] else "0"

        logger.debug("peer bitmap=" + buf)

    def get_type(self):
        return self.t
    
    def get_torrent_info(self):
        return self.torrent_info

    def get_id(self): 
        return self.peer_id

    def get_ip(self): 
        return self.ip

    def get_port(self): 
        return self.port

    def get_bitmap(self):
        return self.bitmap
        

class PeerThread(threading.Thread):
    def __init__(self, peer):
        self.peer = peer
        threading.Thread.__init__(self)

    def run(self):
        self.peer.serve_forever()


def setup_peers(torrent_info):
    arr = [[HTTP_PEER_BASE_PORT, HTTP_PEER_COUNT, HTTP_TYPE], 
            [UDP_PEER_BASE_PORT, UDP_PEER_COUNT, UDP_TYPE]]

    for elm in arr:
        for i in xrange(elm[1]):
            peer = Peer(elm[2], elm[0] + i, torrent_info)
            peer_thread = PeerThread(peer)
            peer_thread.setDaemon(True)
    
            peer_list.append(peer)
            peer_threads.append(peer_thread)
    
            logger.info("Starting peer on %s:%d" % (peer.get_ip(), peer.get_port()))
            peer_thread.start()


class HttpRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        peers = []
        for peer in peer_list:

            if peer.get_type() == UDP_TYPE:
                continue

            peers.append({ \
                "peer id": peer.get_id(), \
                "ip": peer.get_ip(), \
                "port": peer.get_port()})

        res = {"interval": 5, "peers": peers}
        self.wfile.write(bencode.encode(res))


class HttpTrackerServer(BaseHTTPServer.HTTPServer):
    def __init__(self, port, requestHandlerClass):
        self.allow_reuse_address = True
        self.timeout = 10
        BaseHTTPServer.HTTPServer.__init__(self, ('0.0.0.0', port), \
                requestHandlerClass)

    
class HttpTrackerThread(threading.Thread):

    def run(self):
        logger.info("Started http-tracker on %d port" % HTTP_TRACKER_PORT)
        s = HttpTrackerServer(HTTP_TRACKER_PORT, HttpRequestHandler)
        s.serve_forever()


class UdpRequestHandler(SocketServer.DatagramRequestHandler):

    def process_connect_request(self):
        if len(self.packet) < 16:
            logger.debug("payload size < 16")
            return 1

        p = self.packet[0:16]
        self.packet = self.packet[16:]

        action = struct.unpack('>I', p[8:12])[0]

        if action != 0:
            logger.debug("expected action `connect' in connect-request")
            return 1

        buf = p[8:12] + p[12:16] + p[0:8]

        logger.debug("sending %d for connect response", len(buf))
        self.socket.sendto(buf, self.client_address)
        return 0


    def process_announce_request(self):
        if len(self.packet) < 98:
            logger.debug("payload size < 98")
            return 1

        p = self.packet
        action = struct.unpack('>I', p[8:12])[0]

        if action != 1:
            logger.debug("not announce request")
            return 1

        response = p[8:12] \
                + p[12:16] \
                + struct.pack('>I', 15) \
                + struct.pack('>I', 0) \
                + struct.pack('>I', 0)

        for peer in peer_list:

            if peer.get_type() == HTTP_TYPE:
                continue

            response += socket.inet_aton(peer.get_ip())
            response += struct.pack('>H', peer.get_port())

        self.socket.sendto(response, self.client_address)
        return 0


    def handle(self):
        logger.debug("got udp-tracker-request from %s", str(self.client_address))

        if self.process_connect_request() != 0:
            return

        (packet, address) = self.socket.recvfrom(self.server.max_packet_size)
        self.packet += packet

        logger.debug("got announce request with length %d", len(self.packet))

        if self.process_announce_request() != 0:
            return

        logger.debug("udp-tracker-request done")


class UdpTrackerServer(SocketServer.UDPServer):
    def __init__(self, port, requestHandlerClass):
        self.allow_reuse_address = True
        self.timeout = 10
        SocketServer.UDPServer.__init__(self, ('0.0.0.0', port), \
                requestHandlerClass)


class UdpTrackerThread(threading.Thread):

    def run(self):
        logger.info("Started udp-tracker on %d port" % UDP_TRACKER_PORT)
        s = UdpTrackerServer(UDP_TRACKER_PORT, UdpRequestHandler)
        s.serve_forever()


if len(sys.argv) != 2:
    print >>sys.stderr, "Usage: %s <path-to-torrent>" % sys.argv[0]
    sys.exit(1)

fd = open(sys.argv[1], 'rb')
if not fd:
    print >>sys.stderr, "can't open file " + sys.argv[1]
    sys.exit(2)

torrent_info = TorrentInfo(fd.read())
setup_peers(torrent_info)

http_tracker_thread = HttpTrackerThread()
http_tracker_thread.setDaemon(True)
http_tracker_thread.start()

udp_tracker_thread = UdpTrackerThread()
udp_tracker_thread.setDaemon(True)
udp_tracker_thread.start()

try:
    while True:
        signal.pause()
except KeyboardInterrupt:
    pass
