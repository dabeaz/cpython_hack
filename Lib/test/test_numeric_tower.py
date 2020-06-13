# test interactions between int, float, Decimal and Fraction

import unittest
import random
import math
import sys
import operator

# Constants related to the hash implementation;  hash(x) is based
# on the reduction of x modulo the prime _PyHASH_MODULUS.
_PyHASH_MODULUS = sys.hash_info.modulus
_PyHASH_INF = sys.hash_info.inf

class HashTest(unittest.TestCase):
    def check_equal_hash(self, x, y):
        # check both that x and y are equal and that their hashes are equal
        self.assertEqual(hash(x), hash(y),
                         "got different hashes for {!r} and {!r}".format(x, y))
        self.assertEqual(x, y)

    def test_bools(self):
        self.check_equal_hash(False, 0)
        self.check_equal_hash(True, 1)

    def test_integers(self):
        # check that equal values hash equal

        # exact integers
        for i in range(-1000, 1000):
            self.check_equal_hash(i, float(i))

        # the current hash is based on reduction modulo 2**n-1 for some
        # n, so pay special attention to numbers of the form 2**n and 2**n-1.
        for i in range(100):
            n = 2**i - 1
            if n == int(float(n)):
                self.check_equal_hash(n, float(n))
                self.check_equal_hash(-n, -float(n))

            n = 2**i
            self.check_equal_hash(n, float(n))
            self.check_equal_hash(-n, -float(n))

        # random values of various sizes
        for _ in range(1000):
            e = random.randrange(300)
            n = random.randrange(-10**e, 10**e)
            if n == int(float(n)):
                self.check_equal_hash(n, float(n))

    def test_complex(self):
        # complex numbers with zero imaginary part should hash equal to
        # the corresponding float

        test_values = [0.0, -0.0, 1.0, -1.0, 0.40625, -5136.5,
                       float('inf'), float('-inf')]

        for zero in -0.0, 0.0:
            for value in test_values:
                self.check_equal_hash(value, complex(value, zero))

    def test_hash_normalization(self):
        # Test for a bug encountered while changing long_hash.
        #
        # Given objects x and y, it should be possible for y's
        # __hash__ method to return hash(x) in order to ensure that
        # hash(x) == hash(y).  But hash(x) is not exactly equal to the
        # result of x.__hash__(): there's some internal normalization
        # to make sure that the result fits in a C long, and is not
        # equal to the invalid hash value -1.  This internal
        # normalization must therefore not change the result of
        # hash(x) for any x.

        class HalibutProxy:
            def __hash__(self):
                return hash('halibut')
            def __eq__(self, other):
                return other == 'halibut'

        x = {'halibut', HalibutProxy()}
        self.assertEqual(len(x), 1)

class ComparisonTest(unittest.TestCase):
    def test_mixed_comparisons(self):

        # ordered list of distinct test values of various types:
        # int, float, Fraction, Decimal
        test_values = [
            float('-inf'),
            -1e308,
            -3.14,
            -2,
            0.0,
            1e-320,
            True,
            float('1.4'),
            7e200,
            ]
        for i, first in enumerate(test_values):
            for second in test_values[i+1:]:
                self.assertLess(first, second)
                self.assertLessEqual(first, second)
                self.assertGreater(second, first)
                self.assertGreaterEqual(second, first)

    def test_complex(self):
        # comparisons with complex are special:  equality and inequality
        # comparisons should always succeed, but order comparisons should
        # raise TypeError.
        z = 1.0 + 0j
        w = -3.14 + 2.7j

        for v in 1, 1.0, complex(1):
            self.assertEqual(z, v)
            self.assertEqual(v, z)

        for v in 2, 2.0, complex(2):
            self.assertNotEqual(z, v)
            self.assertNotEqual(v, z)
            self.assertNotEqual(w, v)
            self.assertNotEqual(v, w)

        for v in (1, 1.0, complex(1),
                  2, 2.0, complex(2), w):
            for op in operator.le, operator.lt, operator.ge, operator.gt:
                self.assertRaises(TypeError, op, z, v)
                self.assertRaises(TypeError, op, v, z)


if __name__ == '__main__':
    unittest.main()
