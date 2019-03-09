from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.cli import CLI


class MyTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        #s1 = self.addSwitch('s1')

        self.addLink(h1, h2, bw=1000, delay='10ms')
        #self.addLink(h2, s1)

topo = MyTopo()
net = Mininet(topo = topo, link = TCLink, controller=None)

net.start()
h2 = net.get('h2')
h2.cmd('python -m SimpleHTTPServer 80 &')
CLI(net)
h2.cmd('kill %python')
net.stop()
