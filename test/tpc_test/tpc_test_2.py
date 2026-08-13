import socket, json, struct
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 8888))
s.listen(1)
c, _ = s.accept()
# 连续发两个包，TCP 可能粘在一起
msg1 = json.dumps({'type':'command','cmd':'SET_SPEED','params':{'speed':2.0}})
msg2 = json.dumps({'type':'command','cmd':'START_SPRAY'})
data = struct.pack('!I', len(msg1)) + msg1.encode() + struct.pack('!I', len(msg2)) + msg2.encode()
c.send(data)  # 一次性发两个包
import time; time.sleep(10)