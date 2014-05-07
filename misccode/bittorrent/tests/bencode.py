#!/usr/bin/python

import re
import unittest

class InvalidBencodeException(Exception):
    pass

num_re = re.compile("i(-?\d+)e")
string_eq = re.compile("(\d+):")

def _decode(b):

    res = num_re.match(b)
    if res is not None:
        return [int(res.group(1)), len(res.group(0))]

    res = string_eq.match(b)
    if res is not None:
        str_len = int(res.group(1))
        from_len = len(res.group(0))
        to_len = from_len + str_len
        return [b[from_len:to_len], to_len]

    if b[0] == 'l':
        offs = 1
        res = []

        while offs < len(b):
            if b[offs] == 'e':
                offs += 1
                break

            rc = _decode(b[offs:])

            res.append(rc[0])
            offs += rc[1]

        return [res, offs]

    if b[0] == 'd':
        offs = 1
        res = dict()

        while offs < len(b):
            if b[offs] == 'e':
                offs += 1
                break

            k = _decode(b[offs:])

            offs += k[1]

            v = _decode(b[offs:])

            offs += v[1]

            res[k[0]] = v[0]

            if b[offs] == 'e':
                offs += 1
                break

        return [res, offs]



# constructs object from bencoded-buffer
def decode(buf):
    if not len(buf):
        return

    rc = _decode(buf)

    if rc is None:
        raise InvalidBencodeException()

    return rc[0]


def _encode(s):

    if type(s) == type(1):
        return "i" + str(s) + "e"

    if type(s) == type(''):
        return str(len(s)) + ":" + s

    if type(s) == type([]):
        res = "l"
        for t in s:
            res += _encode(t)
        res += "e"
        return res

    if type(s) == type({}):
        res = "d"
        for k in s:
            res += _encode(k)
            res += _encode(s[k])
        res += "e"
        return res

    raise InvalidBencodeException("unknown type " + type(s))


def encode(s):
    if s is None:
        return
    return _encode(s)


class TestBencode(unittest.TestCase):

    def test_encode(self):
        self.assertEqual(decode("i1e"), 1)
        self.assertEqual(decode("i-1e"), -1)

        self.assertEqual(decode("4:spam"), "spam")
        self.assertEqual(decode("le"), [])
        self.assertEqual(decode("l4:spame"), ["spam"])
        self.assertEqual(decode("d4:spaml1:a1:bee"), {"spam": ['a','b']})


    def test_decode(self):
        self.assertEqual("i1e", encode(1))
        self.assertEqual("i-1e", encode(-1))

        self.assertEqual("4:spam", encode("spam"))
        self.assertEqual("le", encode([]))
        self.assertEqual("l4:spame", encode(["spam"]))
        self.assertEqual("d4:spaml1:a1:bee", encode({"spam": ['a','b']}))

if __name__ == '__main__':
    unittest.main()
