#!/usr/bin/python

from mininet.node import OVSBridge
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI

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

topo = MyTopo()
net = Mininet(topo = topo, switch = OVSBridge, controller = None)

net.start()
CLI(net)
net.stop()
