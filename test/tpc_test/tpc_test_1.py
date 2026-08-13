# 终端1：假服务器，连上后发一个完整包
# 4字节长度头 + JSON，用 python 一行发
import socket, json, struct
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 8888))
s.listen(1)
c, _ = s.accept()
msg = json.dumps({'type':'command','cmd':'SET_SPEED','params':{'speed':3.0}})
c.send(struct.pack('!I', len(msg)) + msg.encode())
import time; time.sleep(10)