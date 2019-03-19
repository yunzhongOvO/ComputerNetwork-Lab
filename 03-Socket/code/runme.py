#!/usr/bin/python

from mininet.node import OVSBridge
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI
import os

nworkers = 3

class MyTopo(Topo):
    def build(self):
        s1 = self.addSwitch('s1')

        hosts = list()
        for i in range(1, nworkers+2):
            h = self.addHost('h' + str(i))
            hosts.append(h)

        for h in hosts:
            self.addLink(h, s1)

os.system("make")
# print("Compile completed.\n")

topo = MyTopo()
net = Mininet(topo = topo, switch = OVSBridge, controller = None)

net.start()
# CLI(net) # 

h1, h2, h3, h4 = net.get('h1', 'h2', 'h3', 'h4')
h2.cmd('./worker > h2.txt &')
h3.cmd('./worker > h3.txt &')
h4.cmd('./worker > h4.txt &')

print h1.cmd('./master war_and_peace.txt')

net.stop()

print("-------------- Process h2 --------------")
os.system("cat h2.txt")
print("\n-------------- Process h3 --------------")
os.system("cat h3.txt")
print("\n-------------- Process h3 --------------")
os.system("cat h4.txt")

os.system("make clean")
