# Convenience test module to run all of the OpenSSL-related tests in the
# standard library.

import ssl
import sys
import subprocess

TESTS = [
    'test_asyncio', 'test_ensurepip.py', 'test_ftplib', 'test_hashlib',
    'test_hmac', 'test_ssl', 'test_venv', 
]

def run_regrtests(*extra_args):
    print(ssl.OPENSSL_VERSION)
    args = [
        sys.executable,
        '-Werror', '-bb',  # turn warnings into exceptions
        '-m', 'test',
    ]
    if not extra_args:
        args.extend([
            '-r',  # randomize
            '-w',  # re-run failed tests with -v
            '-u', 'network',  # use network
            '-u', 'urlfetch',  # download test vectors
            '-j', '0'  # use multiple CPUs
        ])
    else:
        args.extend(extra_args)
    args.extend(TESTS)
    result = subprocess.call(args)
    sys.exit(result)

if __name__ == '__main__':
    run_regrtests(*sys.argv[1:])
