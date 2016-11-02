# Copyright (c) 2014-2016 Josh Blum
# SPDX-License-Identifier: BSL-1.0

from . PothosModule import *

class Label(object):
    def __init__(self, id, data, index):
        self.id = id
        self.data = data
        self.index = index

    def toProxy(self, env):
        """
        Convert to a Pothos::Label Proxy object.
        """
        cls = env.findProxy("Pothos/Label")
        return cls(self.id, self.data, self.index)

class LabelIteratorRange(object):
    def __init__(self, labelIter):
        self._labelIter = labelIter

    def __getattr__(self, name):
        return lambda *args: self._labelIter.call(name, *args)

    def __iter__(self):
        index = 0
        while True:
            i = self._labelIter.at(index)
            if i == self._labelIter.end(): break
            yield i.deref()
            index += 1
