'''Test runner and result class for the regression test suite.

'''

import functools
import io
import sys
import time
import traceback
import unittest

from datetime import datetime

class QuietRegressionTestRunner:
    def __init__(self, stream, buffer=False):
        self.result = unittest.TextTestResult(stream, None, 0)
        self.result.buffer = buffer

    def run(self, test):
        test(self.result)
        return self.result

def get_test_runner_class(verbosity, buffer=False):
    if verbosity:
        return functools.partial(unittest.TextTestRunner,
                                 resultclass=unittest.TextTestResult,
                                 buffer=buffer,
                                 verbosity=verbosity)
    return functools.partial(QuietRegressionTestRunner, buffer=buffer)

def get_test_runner(stream, verbosity, capture_output=False):
    return get_test_runner_class(verbosity, capture_output)(stream)

if __name__ == '__main__':
    class TestTests(unittest.TestCase):
        def test_pass(self):
            pass

        def test_pass_slow(self):
            time.sleep(1.0)

        def test_fail(self):
            print('stdout', file=sys.stdout)
            print('stderr', file=sys.stderr)
            self.fail('failure message')

        def test_error(self):
            print('stdout', file=sys.stdout)
            print('stderr', file=sys.stderr)
            raise RuntimeError('error message')

    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestTests))
    stream = io.StringIO()
    runner_cls = get_test_runner_class(sum(a == '-v' for a in sys.argv))
    runner = runner_cls(sys.stdout)
    result = runner.run(suite)
    print('Output:', stream.getvalue())
