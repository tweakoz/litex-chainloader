#!/usr/bin/env python3

# copied from https://gist.github.com/anthonygclark/5377510

import os, binascii, sys, time

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor

TOINT = lambda x: int(binascii.hexlify(x), 16)
TOBYT = lambda x: binascii.unhexlify(hex(x)[2:].rjust(4, '0'))

##############################################################

class Client:

    def __init__(self,xport,addr,mode,filename):
        self._transport = xport
        self._addr = addr
        self._filename = filename
        self._file = open(filename,"rb")
        self._blocksize = 512
        self._fsize = os.path.getsize(filename)
        self._numbytes = 0
        host = addr[0]
        print(host, "(", mode, ")", "wants", filename, "with size", self._fsize, "bytes")
        self.sendBlock(1)

    def fetchBlock(self):
        try:
          while True:
            #print("fetchBlock<%s>\n"%(self._filename))
            data = self._file.read(self._blocksize)
            as_str = binascii.hexlify(data)
            #print("data<%s>\n"%str(as_str))
            if not data:
                break
            yield data
        except KeyboardInterrupt:
            sys.exit(0)

    def sendBlock(self, block_number):
        try:
            block = next(self.fetchBlock())
            blocklen = len(block)
            print("send file<%s> len<0x%08x> block_number<%s> [0x%08x..0x%08x]"%(self._filename,self._fsize, block_number,self._numbytes,self._numbytes+blocklen))
            self._numbytes += blocklen
            as_bytes = bytes('\x00\x03',"utf8")
            as_bytes += TOBYT(block_number)
            as_bytes += block
            self._transport.write(as_bytes, self._addr)
            #time.sleep(.001)

        except StopIteration:
            self._file.close()
        except:
            print(sys.exc_info()[0])
            self._file.close()
            assert False

##############################################################

class TFTP(DatagramProtocol):

    def __init__(self, clientdb):
        self._clientdb = clientdb
        self._clients = dict()

    def datagramReceived(self, data, addr):
        host = addr[0]
        port = addr[1]

        #print("got dg host<%s> port<%s>"%(host, port))

        _class = self._clientdb.get(host, '.')

        cmda = int(data[0])
        cmdb = int(data[1])

        if (cmda==0 and cmdb==1):
            data_content = data[2:]
            strs = data_content.split(b'\0')
            filename = _class + strs[0].decode("utf8")
            print("host<%s> request for filename<%s>"%(host,filename))
            mode = strs[1].decode("utf8")

            if not os.path.exists(filename):
                msg = "file <%s> not found" % (filename)
                as_bytes = bytes('\x00\x05\x00\x01%s\x00'%msg, "utf8")
                self.transport.write(as_bytes, addr)
                return

            client = Client(self.transport,addr,mode,filename)
            self._clients[addr] = client

        elif (cmda==0 and cmdb==4): # ACK
            inta = int(data[2])
            intb = int(data[3])
            new_block = (inta << 8) | (intb & 0xff)
            client = self._clients[addr]
            client.sendBlock(new_block + 1)



##################################

CLIENTDB = {
    '10.0.0.1' : 'arty/',
}

##################################
# listen
##################################

reactor.listenUDP(6069, TFTP(CLIENTDB))
reactor.run()
