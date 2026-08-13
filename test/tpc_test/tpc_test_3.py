import socket, json, struct, time
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 8888))
s.listen(1)
c, _ = s.accept()
msg = json.dumps({'type':'command','cmd':'SET_SPEED','params':{'speed':5.0}})
data = struct.pack('!I', len(msg)) + msg.encode()
# 先发前一半，睡 1 秒，再发后一半
c.send(data[:6])
time.sleep(1)
c.send(data[6:])
time.sleep(10)