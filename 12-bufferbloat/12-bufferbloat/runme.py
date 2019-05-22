import matplotlib.pyplot as plt

f = open('qlen-10/cwnd.txt')
line = f.readline()
t, w = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        t.append(float(line.split()[0]))
        w.append(int(line.split()[6]))
    line = f.readline()
f.close()
plt.plot(t, w, label='qlen=10')

f = open('qlen-100/cwnd.txt')
line = f.readline()
t, w = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        t.append(float(line.split()[0]))
        w.append(int(line.split()[6]))
    line = f.readline()
f.close()
plt.plot(t, w, label='qlen=100')

f = open('qlen-500/cwnd.txt')
line = f.readline()
t, w = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        t.append(float(line.split()[0]))
        w.append(int(line.split()[6]))
    line = f.readline()
f.close()
plt.plot(t, w, label='qlen=500')

plt.ylabel('CWND(KB)')
plt.legend()
plt.savefig('./pic/cwnd.jpg')
plt.show()


##### packet ########

f = open('qlen-10/qlen.txt')
t, p = [], []
line = f.readline()
while line:
    if len(line.split()) > 1:
        p.append(int(line.split()[1]))
        if len(t):
            t.append(float(line.split()[0].split(',')[0])-t[0])
        else:
            t.append(float(line.split()[0].split(',')[0]))
    line = f.readline()
f.close()
t[0] = 0.0
plt.plot(t, p, label='qlen=10')

f = open('qlen-100/qlen.txt')
t, p = [], []
line = f.readline()
while line:
    if len(line.split()) > 1:
        p.append(int(line.split()[1]))
        if len(t):
            t.append(float(line.split()[0].split(',')[0])-t[0])
        else:
            t.append(float(line.split()[0].split(',')[0]))
    line = f.readline()
f.close()
t[0] = 0.0
plt.plot(t, p, label='qlen=100')

f = open('qlen-500/qlen.txt')
t, p = [], []
line = f.readline()
while line:
    if len(line.split()) > 1:
        p.append(int(line.split()[1]))
        if len(t):
            t.append(float(line.split()[0].split(',')[0])-t[0])
        else:
            t.append(float(line.split()[0].split(',')[0]))
    line = f.readline()
f.close()
t[0] = 0.0
plt.plot(t, p, label='qlen=500')

plt.ylabel('QLen: #(Packages)')
plt.legend()
plt.savefig('./pic/packet.jpg')
plt.show()


##### RTT ########

f = open('qlen-10/ping.txt')
line = f.readline()
line = f.readline()
t, r = [], []
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
for i in range(1, len(t)):
    t[i] = float((i+1)/10)
plt.plot(t, r, label='qlen=10')

f = open('qlen-100/ping.txt')
line = f.readline()
line = f.readline()
t, r = [], []
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
for i in range(1, len(t)):
    t[i] = float((i+1)/10)
plt.plot(t, r, label='qlen=100')

f = open('qlen-500/ping.txt')
line = f.readline()
line = f.readline()
t, r = [], []
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
for i in range(1, len(t)):
    t[i] = float((i+1)/10)
plt.plot(t, r, label='qlen=500')

plt.ylabel('RTT(ms)')
plt.legend()
plt.savefig('./pic/rtt.jpg')
plt.show()


##### algo ########

f = open('codel/ping.txt')
t, r = [], []
line = f.readline()
line = f.readline()
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
plt.plot(t, r, label='CoDel')

f = open('red/ping.txt')
t, r = [], []
line = f.readline()
line = f.readline()
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
plt.plot(t, r, label='RED')

f = open('taildrop/ping.txt')
line = f.readline()
line = f.readline()
t, r = [], []
while line:
    r.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()
t = list(range(len(r)))
plt.plot(t, r, label='Tail Drop')

plt.ylabel('RTT(ms)')
plt.legend()
plt.title('per-packet queue delay for dynamic bandwidth', loc='left')
plt.savefig('./pic/algo.jpg')
plt.show()