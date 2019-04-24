#!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI

class RouterTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        r1 = self.addHost('r1')
        r2 = self.addHost('r2')
        r3 = self.addHost('r3')

        self.addLink(h1, r1)
        self.addLink(r1, r2)
        self.addLink(r2, r3)
        self.addLink(r3, h2)

if __name__ == '__main__':
    topo = RouterTopo()
    net = Mininet(topo = topo, controller = None) 

    h1, h2 = net.get('h1', 'h2')
    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')
    h2.cmd('ifconfig h2-eth0 10.0.4.44/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.4.2')

    for h in (h1, h2):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    for id in range(1, 4):
        r = net.get('r' + str(id))
        r.cmd('ifconfig r' + str(id) + '-eth0 10.0.' + str(id) + '.1/24')
        r.cmd('ifconfig r' + str(id) + '-eth1 10.0.' + str(id + 1) + '.2/24')
        
    
    r1 = net.get('r1')
    r1.cmd('route add -net 10.0.4.0 netmask 255.255.255.0 gw 10.0.2.1 dev r1-eth1')
    r1.cmd('route add -net 10.0.3.0 netmask 255.255.255.0 gw 10.0.2.1 dev r1-eth1')
    r2 = net.get('r2')
    r2.cmd('route add -net 10.0.4.0 netmask 255.255.255.0 gw 10.0.3.1 dev r2-eth1')
    r2.cmd('route add -net 10.0.1.0 netmask 255.255.255.0 gw 10.0.2.2 dev r2-eth0')
    r3 = net.get('r3')
    r3.cmd('route add -net 10.0.1.0 netmask 255.255.255.0 gw 10.0.3.2 dev r3-eth0')
    r3.cmd('route add -net 10.0.2.0 netmask 255.255.255.0 gw 10.0.3.2 dev r3-eth0')
    
    for r in (r1, r2, r3):
        r.cmd('./scripts/disable_arp.sh')
        r.cmd('./scripts/disable_icmp.sh')
        r.cmd('./scripts/disable_ip_forward.sh')
        # r.cmd('./router-reference& ')
    
    net.start()
    CLI(net)
    net.stop()
