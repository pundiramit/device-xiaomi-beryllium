#!/usr/bin/env python2.7

import ctypes
import collections

from ctypes import CDLL, CFUNCTYPE, POINTER, cast, py_object
from ctypes import c_char_p, c_void_p, c_int, pointer

_qrtr = CDLL("./libqrtr.so")

class qrtr:
    Result = collections.namedtuple('Result', ['service', 'instance', 'addr'])
    _cbtype = CFUNCTYPE(None, c_void_p, c_int, c_int, c_int, c_int)
    def __init__(self, port=0):
        self.sock = _qrtr.qrtr_open(port)
        if self.sock < 0:
            raise RuntimeError("unable to open qrtr socket")
        self.service = None
        self._qrtr = _qrtr

    def __del__(self):
        self._qrtr.qrtr_close(self.sock)

    def _lookup_list_add(self, ptr, srv, instance, node, port):
        res = qrtr.Result(srv, instance, (node, port))
        cast(ptr, POINTER(py_object)).contents.value.append(res)

    def lookup(self, srv, instance=0, ifilter=0):
        results = []
        err = _qrtr.qrtr_lookup(self.sock, srv, instance, ifilter,
                qrtr._cbtype(self._lookup_list_add), cast(pointer(py_object(results)), c_void_p))
        if err:
            raise RuntimeError("query failed")
        return results

    def publish(self, service, version, instance):
        err = _qrtr.qrtr_publish(self.sock, service, version, instance)
        if err:
            raise RuntimeError("publish failed")
        self.service = (service, version, instance)

    def new_server(self, service, version, instance):
        err = _qrtr.qrtr_new_server(self.sock, service, version, instance)
        if err:
            raise RuntimeError("new_server failed")
        self.service = (service, version, instance)

        return self.service

    def remove_server(self, service):
        err = _qrtr.qrtr_remove_server(self.sock, *service)
        if err:
            raise RuntimeError("remove_server failed")
        self.service = None

    def new_lookup(self, service, version, instance):
        err = _qrtr.qrtr_new_lookup(self.sock, service, version, instance)
        if err:
            raise RuntimeError("new_lookup failed")
        return (service, version, instance)

    def remove_lookup(self, lookup):
        err = _qrtr.qrtr_remove_lookup(self.sock, *lookup)
        if err:
            raise RuntimeError("remove_lookup failed")

    def send(self, addr, data):
        node, port = addr
        n = _qrtr.qrtr_sendto(self.sock, node, port, c_char_p(data), len(data))
        if n:
            raise RuntimeError("sendto failed")

    def recv(self, sz=65536):
        buf = ctypes.create_string_buffer(sz)
        n = _qrtr.qrtr_recv(self.sock, c_char_p(ctypes.addressof(buf)), sz)
        if n <= 0:
            raise RuntimeError("recv failed")
        return buf[0:n]

    def recvfrom(self, sz=65536):
        node = ctypes.c_int()
        port = ctypes.c_int()
        buf = ctypes.create_string_buffer(sz)
        n = _qrtr.qrtr_recvfrom(self.sock, c_char_p(ctypes.addressof(buf)),
                ctypes.byref(node), ctypes.byref(port))
        if n <= 0:
            raise RuntimeError("recvfrom failed")
        return (buf[0:n], (node.value, port.value))

    def poll(self, tout=0):
        return _qrtr.qrtr_poll(self.sock, tout)

if __name__ == "__main__":
    svcs = qrtr().lookup(15) # 15 is the test service
    print " service  instance  addr"
    for svc in svcs:
        print "% 8d  % 8d  %s" % (svc.service, svc.instance, svc.addr)
