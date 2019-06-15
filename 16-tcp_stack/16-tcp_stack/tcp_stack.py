#!/usr/bin/python

import sys
import string
import socket
from time import sleep

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    cs, addr = s.accept()
    
    ofile = open('server-output.dat', 'wb')
    while True:
        data = cs.recv(1400)
        if data:
            ofile.write(data)
        else:
            break
    
    ofile.close()

    s.close()


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))

    ifile = open('client-input.dat', 'rb')

    while True:
        data = ifile.read(1400)
        if data:
            s.send(data)
        else:
            break
    
    ifile.close()

    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
