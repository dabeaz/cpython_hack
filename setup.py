# Autodetecting setup.py script for building the Python extensions

import argparse
import importlib._bootstrap
import importlib.machinery
import importlib.util
import os
import re
import sys
import sysconfig
from glob import glob


try:
    import subprocess
    del subprocess
    SUBPROCESS_BOOTSTRAP = False
except ImportError:
    # Bootstrap Python: distutils.spawn uses subprocess to build C extensions,
    # subprocess requires C extensions built by setup.py like _posixsubprocess.
    #
    # Use _bootsubprocess which only uses the os module.
    #
    # It is dropped from sys.modules as soon as all C extension modules
    # are built.
    import _bootsubprocess
    sys.modules['subprocess'] = _bootsubprocess
    del _bootsubprocess
    SUBPROCESS_BOOTSTRAP = True


from distutils import log
from distutils.command.build_ext import build_ext
from distutils.command.build_scripts import build_scripts
from distutils.command.install import install
from distutils.command.install_lib import install_lib
from distutils.core import Extension, setup
from distutils.errors import CCompilerError, DistutilsError
from distutils.spawn import find_executable


# Compile extensions used to test Python?
TEST_EXTENSIONS = True

# This global variable is used to hold the list of modules to be disabled.
DISABLED_MODULE_LIST = []


def get_platform():
    # Cross compiling
    if "_PYTHON_HOST_PLATFORM" in os.environ:
        return os.environ["_PYTHON_HOST_PLATFORM"]

    # Get value of sys.platform
    if sys.platform.startswith('osf1'):
        return 'osf1'
    return sys.platform


CROSS_COMPILING = ("_PYTHON_HOST_PLATFORM" in os.environ)
HOST_PLATFORM = get_platform()
MS_WINDOWS = (HOST_PLATFORM == 'win32')
CYGWIN = (HOST_PLATFORM == 'cygwin')
MACOS = (HOST_PLATFORM == 'darwin')
AIX = (HOST_PLATFORM.startswith('aix'))
VXWORKS = ('vxworks' in HOST_PLATFORM)


SUMMARY = """
Python is an interpreted, interactive, object-oriented programming
language. It is often compared to Tcl, Perl, Scheme or Java.

Python combines remarkable power with very clear syntax. It has
modules, classes, exceptions, very high level dynamic data types, and
dynamic typing. There are interfaces to many system calls and
libraries, as well as to various windowing systems (X11, Motif, Tk,
Mac, MFC). New built-in modules are easily written in C or C++. Python
is also usable as an extension language for applications that need a
programmable interface.

The Python implementation is portable: it runs on many brands of UNIX,
on Windows, DOS, Mac, Amiga... If your favorite system isn't
listed here, it may still be supported, if there's a C compiler for
it. Ask around on comp.lang.python -- or just try compiling Python
yourself.
"""

CLASSIFIERS = """
Development Status :: 6 - Mature
License :: OSI Approved :: Python Software Foundation License
Natural Language :: English
Programming Language :: C
Programming Language :: Python
Topic :: Software Development
"""


def run_command(cmd):
    status = os.system(cmd)
    return os.waitstatus_to_exitcode(status)


# Set common compiler and linker flags derived from the Makefile,
# reserved for building the interpreter and the stdlib modules.
# See bpo-21121 and bpo-35257
def set_compiler_flags(compiler_flags, compiler_py_flags_nodist):
    flags = sysconfig.get_config_var(compiler_flags)
    py_flags_nodist = sysconfig.get_config_var(compiler_py_flags_nodist)
    sysconfig.get_config_vars()[compiler_flags] = flags + ' ' + py_flags_nodist


def add_dir_to_list(dirlist, dir):
    """Add the directory 'dir' to the list 'dirlist' (after any relative
    directories) if:

    1) 'dir' is not already in 'dirlist'
    2) 'dir' actually exists, and is a directory.
    """
    if dir is None or not os.path.isdir(dir) or dir in dirlist:
        return
    for i, path in enumerate(dirlist):
        if not os.path.isabs(path):
            dirlist.insert(i + 1, dir)
            return
    dirlist.insert(0, dir)


def sysroot_paths(make_vars, subdirs):
    """Get the paths of sysroot sub-directories.

    * make_vars: a sequence of names of variables of the Makefile where
      sysroot may be set.
    * subdirs: a sequence of names of subdirectories used as the location for
      headers or libraries.
    """

    dirs = []
    for var_name in make_vars:
        var = sysconfig.get_config_var(var_name)
        if var is not None:
            m = re.search(r'--sysroot=([^"]\S*|"[^"]+")', var)
            if m is not None:
                sysroot = m.group(1).strip('"')
                for subdir in subdirs:
                    if os.path.isabs(subdir):
                        subdir = subdir[1:]
                    path = os.path.join(sysroot, subdir)
                    if os.path.isdir(path):
                        dirs.append(path)
                break
    return dirs


MACOS_SDK_ROOT = None
MACOS_SDK_SPECIFIED = None

def macosx_sdk_root():
    """Return the directory of the current macOS SDK.

    If no SDK was explicitly configured, call the compiler to find which
    include files paths are being searched by default.  Use '/' if the
    compiler is searching /usr/include (meaning system header files are
    installed) or use the root of an SDK if that is being searched.
    (The SDK may be supplied via Xcode or via the Command Line Tools).
    The SDK paths used by Apple-supplied tool chains depend on the
    setting of various variables; see the xcrun man page for more info.
    Also sets MACOS_SDK_SPECIFIED for use by macosx_sdk_specified().
    """
    global MACOS_SDK_ROOT, MACOS_SDK_SPECIFIED

    # If already called, return cached result.
    if MACOS_SDK_ROOT:
        return MACOS_SDK_ROOT

    cflags = sysconfig.get_config_var('CFLAGS')
    m = re.search(r'-isysroot\s*(\S+)', cflags)
    if m is not None:
        MACOS_SDK_ROOT = m.group(1)
        MACOS_SDK_SPECIFIED = MACOS_SDK_ROOT != '/'
    else:
        MACOS_SDK_ROOT = '/'
        MACOS_SDK_SPECIFIED = False
        cc = sysconfig.get_config_var('CC')
        tmpfile = '/tmp/setup_sdk_root.%d' % os.getpid()
        try:
            os.unlink(tmpfile)
        except:
            pass
        ret = run_command('%s -E -v - </dev/null 2>%s 1>/dev/null' % (cc, tmpfile))
        in_incdirs = False
        try:
            if ret == 0:
                with open(tmpfile) as fp:
                    for line in fp.readlines():
                        if line.startswith("#include <...>"):
                            in_incdirs = True
                        elif line.startswith("End of search list"):
                            in_incdirs = False
                        elif in_incdirs:
                            line = line.strip()
                            if line == '/usr/include':
                                MACOS_SDK_ROOT = '/'
                            elif line.endswith(".sdk/usr/include"):
                                MACOS_SDK_ROOT = line[:-12]
        finally:
            os.unlink(tmpfile)

    return MACOS_SDK_ROOT


def macosx_sdk_specified():
    """Returns true if an SDK was explicitly configured.

    True if an SDK was selected at configure time, either by specifying
    --enable-universalsdk=(something other than no or /) or by adding a
    -isysroot option to CFLAGS.  In some cases, like when making
    decisions about macOS Tk framework paths, we need to be able to
    know whether the user explicitly asked to build with an SDK versus
    the implicit use of an SDK when header files are no longer
    installed on a running system by the Command Line Tools.
    """
    global MACOS_SDK_SPECIFIED

    # If already called, return cached result.
    if MACOS_SDK_SPECIFIED:
        return MACOS_SDK_SPECIFIED

    # Find the sdk root and set MACOS_SDK_SPECIFIED
    macosx_sdk_root()
    return MACOS_SDK_SPECIFIED


def is_macosx_sdk_path(path):
    """
    Returns True if 'path' can be located in an OSX SDK
    """
    return ( (path.startswith('/usr/') and not path.startswith('/usr/local'))
                or path.startswith('/System/')
                or path.startswith('/Library/') )


def find_file(filename, std_dirs, paths):
    """Searches for the directory where a given file is located,
    and returns a possibly-empty list of additional directories, or None
    if the file couldn't be found at all.

    'filename' is the name of a file, such as readline.h or libcrypto.a.
    'std_dirs' is the list of standard system directories; if the
        file is found in one of them, no additional directives are needed.
    'paths' is a list of additional locations to check; if the file is
        found in one of them, the resulting list will contain the directory.
    """
    if MACOS:
        # Honor the MacOSX SDK setting when one was specified.
        # An SDK is a directory with the same structure as a real
        # system, but with only header files and libraries.
        sysroot = macosx_sdk_root()

    # Check the standard locations
    for dir in std_dirs:
        f = os.path.join(dir, filename)

        if MACOS and is_macosx_sdk_path(dir):
            f = os.path.join(sysroot, dir[1:], filename)

        if os.path.exists(f): return []

    # Check the additional directories
    for dir in paths:
        f = os.path.join(dir, filename)

        if MACOS and is_macosx_sdk_path(dir):
            f = os.path.join(sysroot, dir[1:], filename)

        if os.path.exists(f):
            return [dir]

    # Not found anywhere
    return None


def find_library_file(compiler, libname, std_dirs, paths):
    result = compiler.find_library_file(std_dirs + paths, libname)
    if result is None:
        return None

    if MACOS:
        sysroot = macosx_sdk_root()

    # Check whether the found file is in one of the standard directories
    dirname = os.path.dirname(result)
    for p in std_dirs:
        # Ensure path doesn't end with path separator
        p = p.rstrip(os.sep)

        if MACOS and is_macosx_sdk_path(p):
            # Note that, as of Xcode 7, Apple SDKs may contain textual stub
            # libraries with .tbd extensions rather than the normal .dylib
            # shared libraries installed in /.  The Apple compiler tool
            # chain handles this transparently but it can cause problems
            # for programs that are being built with an SDK and searching
            # for specific libraries.  Distutils find_library_file() now
            # knows to also search for and return .tbd files.  But callers
            # of find_library_file need to keep in mind that the base filename
            # of the returned SDK library file might have a different extension
            # from that of the library file installed on the running system,
            # for example:
            #   /Applications/Xcode.app/Contents/Developer/Platforms/
            #       MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/
            #       usr/lib/libedit.tbd
            # vs
            #   /usr/lib/libedit.dylib
            if os.path.join(sysroot, p[1:]) == dirname:
                return [ ]

        if p == dirname:
            return [ ]

    # Otherwise, it must have been in one of the additional directories,
    # so we have to figure out which one.
    for p in paths:
        # Ensure path doesn't end with path separator
        p = p.rstrip(os.sep)

        if MACOS and is_macosx_sdk_path(p):
            if os.path.join(sysroot, p[1:]) == dirname:
                return [ p ]

        if p == dirname:
            return [p]
    else:
        assert False, "Internal error: Path not found in std_dirs or paths"

def validate_tzpath():
    base_tzpath = sysconfig.get_config_var('TZPATH')
    if not base_tzpath:
        return

    tzpaths = base_tzpath.split(os.pathsep)
    bad_paths = [tzpath for tzpath in tzpaths if not os.path.isabs(tzpath)]
    if bad_paths:
        raise ValueError('TZPATH must contain only absolute paths, '
                         + f'found:\n{tzpaths!r}\nwith invalid paths:\n'
                         + f'{bad_paths!r}')

def find_module_file(module, dirlist):
    """Find a module in a set of possible folders. If it is not found
    return the unadorned filename"""
    list = find_file(module, [], dirlist)
    if not list:
        return module
    if len(list) > 1:
        log.info("WARNING: multiple copies of %s found", module)
    return os.path.join(list[0], module)


class PyBuildExt(build_ext):

    def __init__(self, dist):
        build_ext.__init__(self, dist)
        self.srcdir = None
        self.lib_dirs = None
        self.inc_dirs = None
        self.config_h_vars = None
        self.failed = []
        self.failed_on_import = []
        self.missing = []
        self.disabled_configure = []
        if '-j' in os.environ.get('MAKEFLAGS', ''):
            self.parallel = True

    def add(self, ext):
        self.extensions.append(ext)

    def set_srcdir(self):
        self.srcdir = sysconfig.get_config_var('srcdir')
        if not self.srcdir:
            # Maybe running on Windows but not using CYGWIN?
            raise ValueError("No source directory; cannot proceed.")
        self.srcdir = os.path.abspath(self.srcdir)

    def remove_disabled(self):
        # Remove modules that are present on the disabled list
        extensions = [ext for ext in self.extensions
                      if ext.name not in DISABLED_MODULE_LIST]
        # move ctypes to the end, it depends on other modules
        ext_map = dict((ext.name, i) for i, ext in enumerate(extensions))
        if "_ctypes" in ext_map:
            ctypes = extensions.pop(ext_map["_ctypes"])
            extensions.append(ctypes)
        self.extensions = extensions

    def update_sources_depends(self):
        # Fix up the autodetected modules, prefixing all the source files
        # with Modules/.
        moddirlist = [os.path.join(self.srcdir, 'Modules')]

        # Fix up the paths for scripts, too
        self.distribution.scripts = [os.path.join(self.srcdir, filename)
                                     for filename in self.distribution.scripts]

        # Python header files
        headers = [sysconfig.get_config_h_filename()]
        headers += glob(os.path.join(sysconfig.get_path('include'), "*.h"))

        for ext in self.extensions:
            ext.sources = [ find_module_file(filename, moddirlist)
                            for filename in ext.sources ]
            if ext.depends is not None:
                ext.depends = [find_module_file(filename, moddirlist)
                               for filename in ext.depends]
            else:
                ext.depends = []
            # re-compile extensions if a header file has been changed
            ext.depends.extend(headers)

    def remove_configured_extensions(self):
        # The sysconfig variables built by makesetup that list the already
        # built modules and the disabled modules as configured by the Setup
        # files.
        sysconf_built = sysconfig.get_config_var('MODBUILT_NAMES').split()
        sysconf_dis = sysconfig.get_config_var('MODDISABLED_NAMES').split()

        mods_built = []
        mods_disabled = []
        for ext in self.extensions:
            # If a module has already been built or has been disabled in the
            # Setup files, don't build it here.
            if ext.name in sysconf_built:
                mods_built.append(ext)
            if ext.name in sysconf_dis:
                mods_disabled.append(ext)

        mods_configured = mods_built + mods_disabled
        if mods_configured:
            self.extensions = [x for x in self.extensions if x not in
                               mods_configured]
            # Remove the shared libraries built by a previous build.
            for ext in mods_configured:
                fullpath = self.get_ext_fullpath(ext.name)
                if os.path.exists(fullpath):
                    os.unlink(fullpath)

        return (mods_built, mods_disabled)

    def set_compiler_executables(self):
        # When you run "make CC=altcc" or something similar, you really want
        # those environment variables passed into the setup.py phase.  Here's
        # a small set of useful ones.
        compiler = os.environ.get('CC')
        args = {}
        # unfortunately, distutils doesn't let us provide separate C and C++
        # compilers
        if compiler is not None:
            (ccshared,cflags) = sysconfig.get_config_vars('CCSHARED','CFLAGS')
            args['compiler_so'] = compiler + ' ' + ccshared + ' ' + cflags
        self.compiler.set_executables(**args)

    def build_extensions(self):
        self.set_srcdir()

        # Detect which modules should be compiled
        self.detect_modules()

        self.remove_disabled()

        self.update_sources_depends()
        mods_built, mods_disabled = self.remove_configured_extensions()
        self.set_compiler_executables()

        build_ext.build_extensions(self)

        if SUBPROCESS_BOOTSTRAP:
            # Drop our custom subprocess module:
            # use the newly built subprocess module
            del sys.modules['subprocess']

        for ext in self.extensions:
            self.check_extension_import(ext)

        self.summary(mods_built, mods_disabled)

    def summary(self, mods_built, mods_disabled):
        longest = max([len(e.name) for e in self.extensions], default=0)
        if self.failed or self.failed_on_import:
            all_failed = self.failed + self.failed_on_import
            longest = max(longest, max([len(name) for name in all_failed]))

        def print_three_column(lst):
            lst.sort(key=str.lower)
            # guarantee zip() doesn't drop anything
            while len(lst) % 3:
                lst.append("")
            for e, f, g in zip(lst[::3], lst[1::3], lst[2::3]):
                print("%-*s   %-*s   %-*s" % (longest, e, longest, f,
                                              longest, g))

        if self.missing:
            print()
            print("Python build finished successfully!")
            print("The necessary bits to build these optional modules were not "
                  "found:")
            print_three_column(self.missing)
            print("To find the necessary bits, look in setup.py in"
                  " detect_modules() for the module's name.")
            print()

        if mods_built:
            print()
            print("The following modules found by detect_modules() in"
            " setup.py, have been")
            print("built by the Makefile instead, as configured by the"
            " Setup files:")
            print_three_column([ext.name for ext in mods_built])
            print()

        if mods_disabled:
            print()
            print("The following modules found by detect_modules() in"
            " setup.py have not")
            print("been built, they are *disabled* in the Setup files:")
            print_three_column([ext.name for ext in mods_disabled])
            print()

        if self.disabled_configure:
            print()
            print("The following modules found by detect_modules() in"
            " setup.py have not")
            print("been built, they are *disabled* by configure:")
            print_three_column(self.disabled_configure)
            print()

        if self.failed:
            failed = self.failed[:]
            print()
            print("Failed to build these modules:")
            print_three_column(failed)
            print()

        if self.failed_on_import:
            failed = self.failed_on_import[:]
            print()
            print("Following modules built successfully"
                  " but were removed because they could not be imported:")
            print_three_column(failed)
            print()

    def build_extension(self, ext):

        if ext.name == '_ctypes':
            if not self.configure_ctypes(ext):
                self.failed.append(ext.name)
                return

        try:
            build_ext.build_extension(self, ext)
        except (CCompilerError, DistutilsError) as why:
            self.announce('WARNING: building of extension "%s" failed: %s' %
                          (ext.name, why))
            self.failed.append(ext.name)
            return

    def check_extension_import(self, ext):
        # Don't try to import an extension that has failed to compile
        if ext.name in self.failed:
            self.announce(
                'WARNING: skipping import check for failed build "%s"' %
                ext.name, level=1)
            return

        # Workaround for Mac OS X: The Carbon-based modules cannot be
        # reliably imported into a command-line Python
        if 'Carbon' in ext.extra_link_args:
            self.announce(
                'WARNING: skipping import check for Carbon-based "%s"' %
                ext.name)
            return

        if MACOS and (
                sys.maxsize > 2**32 and '-arch' in ext.extra_link_args):
            # Don't bother doing an import check when an extension was
            # build with an explicit '-arch' flag on OSX. That's currently
            # only used to build 32-bit only extensions in a 4-way
            # universal build and loading 32-bit code into a 64-bit
            # process will fail.
            self.announce(
                'WARNING: skipping import check for "%s"' %
                ext.name)
            return

        # Workaround for Cygwin: Cygwin currently has fork issues when many
        # modules have been imported
        if CYGWIN:
            self.announce('WARNING: skipping import check for Cygwin-based "%s"'
                % ext.name)
            return
        ext_filename = os.path.join(
            self.build_lib,
            self.get_ext_filename(self.get_ext_fullname(ext.name)))

        # If the build directory didn't exist when setup.py was
        # started, sys.path_importer_cache has a negative result
        # cached.  Clear that cache before trying to import.
        sys.path_importer_cache.clear()

        # Don't try to load extensions for cross builds
        if CROSS_COMPILING:
            return

        loader = importlib.machinery.ExtensionFileLoader(ext.name, ext_filename)
        spec = importlib.util.spec_from_file_location(ext.name, ext_filename,
                                                      loader=loader)
        try:
            importlib._bootstrap._load(spec)
        except ImportError as why:
            self.failed_on_import.append(ext.name)
            self.announce('*** WARNING: renaming "%s" since importing it'
                          ' failed: %s' % (ext.name, why), level=3)
            assert not self.inplace
            basename, tail = os.path.splitext(ext_filename)
            newname = basename + "_failed" + tail
            if os.path.exists(newname):
                os.remove(newname)
            os.rename(ext_filename, newname)

        except:
            exc_type, why, tb = sys.exc_info()
            self.announce('*** WARNING: importing extension "%s" '
                          'failed with %s: %s' % (ext.name, exc_type, why),
                          level=3)
            self.failed.append(ext.name)

    def add_multiarch_paths(self):
        # Debian/Ubuntu multiarch support.
        # https://wiki.ubuntu.com/MultiarchSpec
        cc = sysconfig.get_config_var('CC')
        tmpfile = os.path.join(self.build_temp, 'multiarch')
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        ret = run_command(
            '%s -print-multiarch > %s 2> /dev/null' % (cc, tmpfile))
        multiarch_path_component = ''
        try:
            if ret == 0:
                with open(tmpfile) as fp:
                    multiarch_path_component = fp.readline().strip()
        finally:
            os.unlink(tmpfile)

        if multiarch_path_component != '':
            add_dir_to_list(self.compiler.library_dirs,
                            '/usr/lib/' + multiarch_path_component)
            add_dir_to_list(self.compiler.include_dirs,
                            '/usr/include/' + multiarch_path_component)
            return

        if not find_executable('dpkg-architecture'):
            return
        opt = ''
        if CROSS_COMPILING:
            opt = '-t' + sysconfig.get_config_var('HOST_GNU_TYPE')
        tmpfile = os.path.join(self.build_temp, 'multiarch')
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        ret = run_command(
            'dpkg-architecture %s -qDEB_HOST_MULTIARCH > %s 2> /dev/null' %
            (opt, tmpfile))
        try:
            if ret == 0:
                with open(tmpfile) as fp:
                    multiarch_path_component = fp.readline().strip()
                add_dir_to_list(self.compiler.library_dirs,
                                '/usr/lib/' + multiarch_path_component)
                add_dir_to_list(self.compiler.include_dirs,
                                '/usr/include/' + multiarch_path_component)
        finally:
            os.unlink(tmpfile)

    def add_cross_compiling_paths(self):
        cc = sysconfig.get_config_var('CC')
        tmpfile = os.path.join(self.build_temp, 'ccpaths')
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        ret = run_command('%s -E -v - </dev/null 2>%s 1>/dev/null' % (cc, tmpfile))
        is_gcc = False
        is_clang = False
        in_incdirs = False
        try:
            if ret == 0:
                with open(tmpfile) as fp:
                    for line in fp.readlines():
                        if line.startswith("gcc version"):
                            is_gcc = True
                        elif line.startswith("clang version"):
                            is_clang = True
                        elif line.startswith("#include <...>"):
                            in_incdirs = True
                        elif line.startswith("End of search list"):
                            in_incdirs = False
                        elif (is_gcc or is_clang) and line.startswith("LIBRARY_PATH"):
                            for d in line.strip().split("=")[1].split(":"):
                                d = os.path.normpath(d)
                                if '/gcc/' not in d:
                                    add_dir_to_list(self.compiler.library_dirs,
                                                    d)
                        elif (is_gcc or is_clang) and in_incdirs and '/gcc/' not in line and '/clang/' not in line:
                            add_dir_to_list(self.compiler.include_dirs,
                                            line.strip())
        finally:
            os.unlink(tmpfile)

    def add_ldflags_cppflags(self):
        # Add paths specified in the environment variables LDFLAGS and
        # CPPFLAGS for header and library files.
        # We must get the values from the Makefile and not the environment
        # directly since an inconsistently reproducible issue comes up where
        # the environment variable is not set even though the value were passed
        # into configure and stored in the Makefile (issue found on OS X 10.3).
        for env_var, arg_name, dir_list in (
                ('LDFLAGS', '-R', self.compiler.runtime_library_dirs),
                ('LDFLAGS', '-L', self.compiler.library_dirs),
                ('CPPFLAGS', '-I', self.compiler.include_dirs)):
            env_val = sysconfig.get_config_var(env_var)
            if env_val:
                parser = argparse.ArgumentParser()
                parser.add_argument(arg_name, dest="dirs", action="append")
                options, _ = parser.parse_known_args(env_val.split())
                if options.dirs:
                    for directory in reversed(options.dirs):
                        add_dir_to_list(dir_list, directory)

    def configure_compiler(self):
        # Ensure that /usr/local is always used, but the local build
        # directories (i.e. '.' and 'Include') must be first.  See issue
        # 10520.
        if not CROSS_COMPILING:
            add_dir_to_list(self.compiler.library_dirs, '/usr/local/lib')
            add_dir_to_list(self.compiler.include_dirs, '/usr/local/include')
        # only change this for cross builds for 3.3, issues on Mageia
        if CROSS_COMPILING:
            self.add_cross_compiling_paths()
        self.add_multiarch_paths()
        self.add_ldflags_cppflags()

    def init_inc_lib_dirs(self):
        if (not CROSS_COMPILING and
                os.path.normpath(sys.base_prefix) != '/usr' and
                not sysconfig.get_config_var('PYTHONFRAMEWORK')):
            # OSX note: Don't add LIBDIR and INCLUDEDIR to building a framework
            # (PYTHONFRAMEWORK is set) to avoid # linking problems when
            # building a framework with different architectures than
            # the one that is currently installed (issue #7473)
            add_dir_to_list(self.compiler.library_dirs,
                            sysconfig.get_config_var("LIBDIR"))
            add_dir_to_list(self.compiler.include_dirs,
                            sysconfig.get_config_var("INCLUDEDIR"))

        system_lib_dirs = ['/lib64', '/usr/lib64', '/lib', '/usr/lib']
        system_include_dirs = ['/usr/include']
        # lib_dirs and inc_dirs are used to search for files;
        # if a file is found in one of those directories, it can
        # be assumed that no additional -I,-L directives are needed.
        if not CROSS_COMPILING:
            self.lib_dirs = self.compiler.library_dirs + system_lib_dirs
            self.inc_dirs = self.compiler.include_dirs + system_include_dirs
        else:
            # Add the sysroot paths. 'sysroot' is a compiler option used to
            # set the logical path of the standard system headers and
            # libraries.
            self.lib_dirs = (self.compiler.library_dirs +
                             sysroot_paths(('LDFLAGS', 'CC'), system_lib_dirs))
            self.inc_dirs = (self.compiler.include_dirs +
                             sysroot_paths(('CPPFLAGS', 'CFLAGS', 'CC'),
                                           system_include_dirs))

        config_h = sysconfig.get_config_h_filename()
        with open(config_h) as file:
            self.config_h_vars = sysconfig.parse_config_h(file)

        # OSF/1 and Unixware have some stuff in /usr/ccs/lib (like -ldb)
        if HOST_PLATFORM in ['osf1', 'unixware7', 'openunix8']:
            self.lib_dirs += ['/usr/ccs/lib']

        # HP-UX11iv3 keeps files in lib/hpux folders.
        if HOST_PLATFORM == 'hp-ux11':
            self.lib_dirs += ['/usr/lib/hpux64', '/usr/lib/hpux32']

        if MACOS:
            # This should work on any unixy platform ;-)
            # If the user has bothered specifying additional -I and -L flags
            # in OPT and LDFLAGS we might as well use them here.
            #
            # NOTE: using shlex.split would technically be more correct, but
            # also gives a bootstrap problem. Let's hope nobody uses
            # directories with whitespace in the name to store libraries.
            cflags, ldflags = sysconfig.get_config_vars(
                    'CFLAGS', 'LDFLAGS')
            for item in cflags.split():
                if item.startswith('-I'):
                    self.inc_dirs.append(item[2:])

            for item in ldflags.split():
                if item.startswith('-L'):
                    self.lib_dirs.append(item[2:])

    def detect_simple_extensions(self):
        #
        # The following modules are all pretty straightforward, and compile
        # on pretty much any POSIXish platform.
        #

        # array objects
        self.add(Extension('array', ['arraymodule.c']))

        shared_math = 'Modules/_math.o'

        # math library functions, e.g. sin()
        self.add(Extension('math',  ['mathmodule.c'],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE'],
                           extra_objects=[shared_math],
                           depends=['_math.h', shared_math],
                           libraries=['m']))

        # complex math library functions
        self.add(Extension('cmath', ['cmathmodule.c'],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE'],
                           extra_objects=[shared_math],
                           depends=['_math.h', shared_math],
                           libraries=['m']))

        # time libraries: librt may be needed for clock_gettime()
        time_libs = []
        lib = sysconfig.get_config_var('TIMEMODULE_LIB')
        if lib:
            time_libs.append(lib)

        # time operations and variables
        self.add(Extension('time', ['timemodule.c'],
                           libraries=time_libs))
        # libm is needed by delta_new() that uses round() and by accum() that
        # uses modf().
        self.add(Extension('_datetime', ['_datetimemodule.c'],
                           libraries=['m']))
        # zoneinfo module
        self.add(Extension('_zoneinfo', ['_zoneinfo.c'])),
        # random number generator implemented in C
        self.add(Extension("_random", ["_randommodule.c"],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE']))
        # bisect
        self.add(Extension("_bisect", ["_bisectmodule.c"]))
        # heapq
        self.add(Extension("_heapq", ["_heapqmodule.c"]))
        # C-optimized pickle replacement
        self.add(Extension("_pickle", ["_pickle.c"],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE']))
        # atexit
        self.add(Extension("atexit", ["atexitmodule.c"]))
        # _json speedups
        self.add(Extension("_json", ["_json.c"],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE']))

        # profiler (_lsprof is for cProfile.py)
        self.add(Extension('_lsprof', ['_lsprof.c', 'rotatingtree.c']))
        # static Unicode character database
        self.add(Extension('unicodedata', ['unicodedata.c'],
                           depends=['unicodedata_db.h', 'unicodename_db.h']))
        # _opcode module
        self.add(Extension('_opcode', ['_opcode.c']))
        # _abc speedups
        self.add(Extension("_abc", ["_abc.c"]))
        # _queue module
        self.add(Extension("_queue", ["_queuemodule.c"]))
        # _statistics module
        self.add(Extension("_statistics", ["_statisticsmodule.c"]))

        # Modules with some UNIX dependencies -- on by default:
        # (If you have a really backward UNIX, select and socket may not be
        # supported...)

        # fcntl(2) and ioctl(2)
        libs = []
        if (self.config_h_vars.get('FLOCK_NEEDS_LIBBSD', False)):
            # May be necessary on AIX for flock function
            libs = ['bsd']
        self.add(Extension('fcntl', ['fcntlmodule.c'],
                           libraries=libs))
        # pwd(3)
        self.add(Extension('pwd', ['pwdmodule.c']))
        # grp(3)
        if not VXWORKS:
            self.add(Extension('grp', ['grpmodule.c']))

        # Lance Ellinghaus's syslog module
        # syslog daemon interface
        self.add(Extension('syslog', ['syslogmodule.c']))

        # Python interface to subinterpreter C-API.
        self.add(Extension('_xxsubinterpreters', ['_xxsubinterpretersmodule.c']))

        #
        # Here ends the simple stuff.  From here on, modules need certain
        # libraries, are platform-specific, or present other surprises.
        #

        # Multimedia modules
        # These don't work for 64-bit platforms!!!
        # These represent audio samples or images as strings:
        #
        # Operations on audio samples
        # According to #993173, this one should actually work fine on
        # 64-bit platforms.
        #

        # CSV files
        self.add(Extension('_csv', ['_csv.c']))

        # POSIX subprocess module helper.
        self.add(Extension('_posixsubprocess', ['_posixsubprocess.c']))

    def detect_test_extensions(self):
        # Python C API test module
        self.add(Extension('_testcapi', ['_testcapimodule.c'],
                           depends=['testcapi_long.h']))

        # Python Internal C API test module
        self.add(Extension('_testinternalcapi', ['_testinternalcapi.c'],
                           extra_compile_args=['-DPy_BUILD_CORE_MODULE']))

        # Python PEP-3118 (buffer protocol) test module
        self.add(Extension('_testbuffer', ['_testbuffer.c']))

        # Test loading multiple modules from one compiled file (http://bugs.python.org/issue16421)
        self.add(Extension('_testimportmultiple', ['_testimportmultiple.c']))

        # Test multi-phase extension module init (PEP 489)
        self.add(Extension('_testmultiphase', ['_testmultiphase.c']))

        # Fuzz tests.
        self.add(Extension('_xxtestfuzz',
                           ['_xxtestfuzz/_xxtestfuzz.c',
                            '_xxtestfuzz/fuzzer.c']))


    def detect_platform_specific_exts(self):
        # Unix-only modules
        if not MS_WINDOWS:
            if not VXWORKS:
                # Steen Lumholt's termios module
                self.add(Extension('termios', ['termios.c']))
                # Jeremy Hylton's rlimit interface
            self.add(Extension('resource', ['resource.c']))
        else:
            self.missing.extend(['resource', 'termios'])

        if MACOS:
            self.add(Extension('_scproxy', ['_scproxy.c'],
                               extra_link_args=[
                                   '-framework', 'SystemConfiguration',
                                   '-framework', 'CoreFoundation']))

    def detect_compress_exts(self):
        # Andrew Kuchling's zlib module.  Note that some versions of zlib
        # 1.1.3 have security problems.  See CERT Advisory CA-2002-07:
        # http://www.cert.org/advisories/CA-2002-07.html
        #
        # zlib 1.1.4 is fixed, but at least one vendor (RedHat) has decided to
        # patch its zlib 1.1.3 package instead of upgrading to 1.1.4.  For
        # now, we still accept 1.1.3, because we think it's difficult to
        # exploit this in Python, and we'd rather make it RedHat's problem
        # than our problem <wink>.
        #
        # You can upgrade zlib to version 1.1.4 yourself by going to
        # http://www.gzip.org/zlib/
        zlib_inc = find_file('zlib.h', [], self.inc_dirs)
        have_zlib = False
        if zlib_inc is not None:
            zlib_h = zlib_inc[0] + '/zlib.h'
            version = '"0.0.0"'
            version_req = '"1.1.3"'
            if MACOS and is_macosx_sdk_path(zlib_h):
                zlib_h = os.path.join(macosx_sdk_root(), zlib_h[1:])
            with open(zlib_h) as fp:
                while 1:
                    line = fp.readline()
                    if not line:
                        break
                    if line.startswith('#define ZLIB_VERSION'):
                        version = line.split()[2]
                        break
            if version >= version_req:
                if (self.compiler.find_library_file(self.lib_dirs, 'z')):
                    if MACOS:
                        zlib_extra_link_args = ('-Wl,-search_paths_first',)
                    else:
                        zlib_extra_link_args = ()
                    self.add(Extension('zlib', ['zlibmodule.c'],
                                       libraries=['z'],
                                       extra_link_args=zlib_extra_link_args))
                    have_zlib = True
                else:
                    self.missing.append('zlib')
            else:
                self.missing.append('zlib')
        else:
            self.missing.append('zlib')

        # Helper module for various ascii-encoders.  Uses zlib for an optimized
        # crc32 if we have it.  Otherwise binascii uses its own.
        if have_zlib:
            extra_compile_args = ['-DUSE_ZLIB_CRC32']
            libraries = ['z']
            extra_link_args = zlib_extra_link_args
        else:
            extra_compile_args = []
            libraries = []
            extra_link_args = []
        self.add(Extension('binascii', ['binascii.c'],
                           extra_compile_args=extra_compile_args,
                           libraries=libraries,
                           extra_link_args=extra_link_args))

        # Gustavo Niemeyer's bz2 module.
        if (self.compiler.find_library_file(self.lib_dirs, 'bz2')):
            if MACOS:
                bz2_extra_link_args = ('-Wl,-search_paths_first',)
            else:
                bz2_extra_link_args = ()
            self.add(Extension('_bz2', ['_bz2module.c'],
                               libraries=['bz2'],
                               extra_link_args=bz2_extra_link_args))
        else:
            self.missing.append('_bz2')

        # LZMA compression support.
        if self.compiler.find_library_file(self.lib_dirs, 'lzma'):
            self.add(Extension('_lzma', ['_lzmamodule.c'],
                               libraries=['lzma']))
        else:
            self.missing.append('_lzma')

    def detect_multibytecodecs(self):
        # Hye-Shik Chang's CJKCodecs modules.
        self.add(Extension('_multibytecodec',
                           ['cjkcodecs/multibytecodec.c']))
        for loc in ('kr', 'jp', 'cn', 'tw', 'hk', 'iso2022'):
            self.add(Extension('_codecs_%s' % loc,
                               ['cjkcodecs/_codecs_%s.c' % loc]))


    def detect_uuid(self):
        # Build the _uuid module if possible
        uuid_incs = find_file("uuid.h", self.inc_dirs, ["/usr/include/uuid"])
        if uuid_incs is not None:
            if self.compiler.find_library_file(self.lib_dirs, 'uuid'):
                uuid_libs = ['uuid']
            else:
                uuid_libs = []
            self.add(Extension('_uuid', ['_uuidmodule.c'],
                               libraries=uuid_libs,
                               include_dirs=uuid_incs))
        else:
            self.missing.append('_uuid')

    def detect_modules(self):
        self.configure_compiler()
        self.init_inc_lib_dirs()

        self.detect_simple_extensions()
        if TEST_EXTENSIONS:
            self.detect_test_extensions()
        self.detect_hash_builtins()
        self.detect_platform_specific_exts()
        self.detect_nis()
        self.detect_compress_exts()
        self.detect_multibytecodecs()
        self.detect_ctypes()
        self.detect_uuid()

##         # Uncomment these lines if you want to play with xxmodule.c
##         self.add(Extension('xx', ['xxmodule.c']))

        if 'd' not in sysconfig.get_config_var('ABIFLAGS'):
            self.add(Extension('xxlimited', ['xxlimited.c'],
                               define_macros=[('Py_LIMITED_API', '0x03050000')]))

    def configure_ctypes_darwin(self, ext):
        # Darwin (OS X) uses preconfigured files, in
        # the Modules/_ctypes/libffi_osx directory.
        ffi_srcdir = os.path.abspath(os.path.join(self.srcdir, 'Modules',
                                                  '_ctypes', 'libffi_osx'))
        sources = [os.path.join(ffi_srcdir, p)
                   for p in ['ffi.c',
                             'x86/darwin64.S',
                             'x86/x86-darwin.S',
                             'x86/x86-ffi_darwin.c',
                             'x86/x86-ffi64.c',
                             'powerpc/ppc-darwin.S',
                             'powerpc/ppc-darwin_closure.S',
                             'powerpc/ppc-ffi_darwin.c',
                             'powerpc/ppc64-darwin_closure.S',
                             ]]

        # Add .S (preprocessed assembly) to C compiler source extensions.
        self.compiler.src_extensions.append('.S')

        include_dirs = [os.path.join(ffi_srcdir, 'include'),
                        os.path.join(ffi_srcdir, 'powerpc')]
        ext.include_dirs.extend(include_dirs)
        ext.sources.extend(sources)
        return True

    def configure_ctypes(self, ext):
        if not self.use_system_libffi:
            if MACOS:
                return self.configure_ctypes_darwin(ext)
            print('INFO: Could not locate ffi libs and/or headers')
            return False
        return True

    def detect_ctypes(self):
        # Thomas Heller's _ctypes module
        self.use_system_libffi = False
        include_dirs = []
        extra_compile_args = ['-DPy_BUILD_CORE_MODULE']
        extra_link_args = []
        sources = ['_ctypes/_ctypes.c',
                   '_ctypes/callbacks.c',
                   '_ctypes/callproc.c',
                   '_ctypes/stgdict.c',
                   '_ctypes/cfield.c']
        depends = ['_ctypes/ctypes.h']

        if MACOS:
            sources.append('_ctypes/malloc_closure.c')
            sources.append('_ctypes/darwin/dlfcn_simple.c')
            extra_compile_args.append('-DMACOSX')
            include_dirs.append('_ctypes/darwin')
            # XXX Is this still needed?
            # extra_link_args.extend(['-read_only_relocs', 'warning'])

        elif HOST_PLATFORM == 'sunos5':
            # XXX This shouldn't be necessary; it appears that some
            # of the assembler code is non-PIC (i.e. it has relocations
            # when it shouldn't. The proper fix would be to rewrite
            # the assembler code to be PIC.
            # This only works with GCC; the Sun compiler likely refuses
            # this option. If you want to compile ctypes with the Sun
            # compiler, please research a proper solution, instead of
            # finding some -z option for the Sun compiler.
            extra_link_args.append('-mimpure-text')

        elif HOST_PLATFORM.startswith('hp-ux'):
            extra_link_args.append('-fPIC')

        ext = Extension('_ctypes',
                        include_dirs=include_dirs,
                        extra_compile_args=extra_compile_args,
                        extra_link_args=extra_link_args,
                        libraries=[],
                        sources=sources,
                        depends=depends)
        self.add(ext)
        if TEST_EXTENSIONS:
            # function my_sqrt() needs libm for sqrt()
            self.add(Extension('_ctypes_test',
                               sources=['_ctypes/_ctypes_test.c'],
                               libraries=['m']))

        ffi_inc_dirs = self.inc_dirs.copy()
        if MACOS:
            if '--with-system-ffi' not in sysconfig.get_config_var("CONFIG_ARGS"):
                return
            # OS X 10.5 comes with libffi.dylib; the include files are
            # in /usr/include/ffi
            ffi_inc_dirs.append('/usr/include/ffi')

        ffi_inc = [sysconfig.get_config_var("LIBFFI_INCLUDEDIR")]
        if not ffi_inc or ffi_inc[0] == '':
            ffi_inc = find_file('ffi.h', [], ffi_inc_dirs)
        if ffi_inc is not None:
            ffi_h = ffi_inc[0] + '/ffi.h'
            if not os.path.exists(ffi_h):
                ffi_inc = None
                print('Header file {} does not exist'.format(ffi_h))
        ffi_lib = None
        if ffi_inc is not None:
            for lib_name in ('ffi', 'ffi_pic'):
                if (self.compiler.find_library_file(self.lib_dirs, lib_name)):
                    ffi_lib = lib_name
                    break

        if ffi_inc and ffi_lib:
            ext.include_dirs.extend(ffi_inc)
            ext.libraries.append(ffi_lib)
            self.use_system_libffi = True

        if sysconfig.get_config_var('HAVE_LIBDL'):
            # for dlopen, see bpo-32647
            ext.libraries.append('dl')

    def detect_hash_builtins(self):
        # By default we always compile these even when OpenSSL is available
        # (issue #14693). It's harmless and the object code is tiny
        # (40-50 KiB per module, only loaded when actually used).  Modules can
        # be disabled via the --with-builtin-hashlib-hashes configure flag.
        supported = {"md5", "sha1", "sha256", "sha512", "sha3", "blake2"}

        configured = sysconfig.get_config_var("PY_BUILTIN_HASHLIB_HASHES")
        configured = configured.strip('"').lower()
        configured = {
            m.strip() for m in configured.split(",")
        }

        self.disabled_configure.extend(
            sorted(supported.difference(configured))
        )

        if "sha256" in configured:
            self.add(Extension(
                '_sha256', ['sha256module.c'],
                extra_compile_args=['-DPy_BUILD_CORE_MODULE'],
                depends=['hashlib.h']
            ))

        if "sha512" in configured:
            self.add(Extension(
                '_sha512', ['sha512module.c'],
                extra_compile_args=['-DPy_BUILD_CORE_MODULE'],
                depends=['hashlib.h']
            ))

        if "md5" in configured:
            self.add(Extension(
                '_md5', ['md5module.c'],
                depends=['hashlib.h']
            ))

        if "sha1" in configured:
            self.add(Extension(
                '_sha1', ['sha1module.c'],
                depends=['hashlib.h']
            ))

        if "blake2" in configured:
            blake2_deps = glob(
                os.path.join(self.srcdir, 'Modules/_blake2/impl/*')
            )
            blake2_deps.append('hashlib.h')
            self.add(Extension(
                '_blake2',
                [
                    '_blake2/blake2module.c',
                    '_blake2/blake2b_impl.c',
                    '_blake2/blake2s_impl.c'
                ],
                depends=blake2_deps
            ))

        if "sha3" in configured:
            sha3_deps = glob(
                os.path.join(self.srcdir, 'Modules/_sha3/kcp/*')
            )
            sha3_deps.append('hashlib.h')
            self.add(Extension(
                '_sha3',
                ['_sha3/sha3module.c'],
                depends=sha3_deps
            ))

    def detect_nis(self):
        if MS_WINDOWS or CYGWIN or HOST_PLATFORM == 'qnx6':
            self.missing.append('nis')
            return

        libs = []
        library_dirs = []
        includes_dirs = []

        # bpo-32521: glibc has deprecated Sun RPC for some time. Fedora 28
        # moved headers and libraries to libtirpc and libnsl. The headers
        # are in tircp and nsl sub directories.
        rpcsvc_inc = find_file(
            'rpcsvc/yp_prot.h', self.inc_dirs,
            [os.path.join(inc_dir, 'nsl') for inc_dir in self.inc_dirs]
        )
        rpc_inc = find_file(
            'rpc/rpc.h', self.inc_dirs,
            [os.path.join(inc_dir, 'tirpc') for inc_dir in self.inc_dirs]
        )
        if rpcsvc_inc is None or rpc_inc is None:
            # not found
            self.missing.append('nis')
            return
        includes_dirs.extend(rpcsvc_inc)
        includes_dirs.extend(rpc_inc)

        if self.compiler.find_library_file(self.lib_dirs, 'nsl'):
            libs.append('nsl')
        else:
            # libnsl-devel: check for libnsl in nsl/ subdirectory
            nsl_dirs = [os.path.join(lib_dir, 'nsl') for lib_dir in self.lib_dirs]
            libnsl = self.compiler.find_library_file(nsl_dirs, 'nsl')
            if libnsl is not None:
                library_dirs.append(os.path.dirname(libnsl))
                libs.append('nsl')

        if self.compiler.find_library_file(self.lib_dirs, 'tirpc'):
            libs.append('tirpc')

        self.add(Extension('nis', ['nismodule.c'],
                           libraries=libs,
                           library_dirs=library_dirs,
                           include_dirs=includes_dirs))


class PyBuildInstall(install):
    # Suppress the warning about installation into the lib_dynload
    # directory, which is not in sys.path when running Python during
    # installation:
    def initialize_options (self):
        install.initialize_options(self)
        self.warn_dir=0

    # Customize subcommands to not install an egg-info file for Python
    sub_commands = [('install_lib', install.has_lib),
                    ('install_headers', install.has_headers),
                    ('install_scripts', install.has_scripts),
                    ('install_data', install.has_data)]


class PyBuildInstallLib(install_lib):
    # Do exactly what install_lib does but make sure correct access modes get
    # set on installed directories and files. All installed files with get
    # mode 644 unless they are a shared library in which case they will get
    # mode 755. All installed directories will get mode 755.

    # this is works for EXT_SUFFIX too, which ends with SHLIB_SUFFIX
    shlib_suffix = sysconfig.get_config_var("SHLIB_SUFFIX")

    def install(self):
        outfiles = install_lib.install(self)
        self.set_file_modes(outfiles, 0o644, 0o755)
        self.set_dir_modes(self.install_dir, 0o755)
        return outfiles

    def set_file_modes(self, files, defaultMode, sharedLibMode):
        if not files: return

        for filename in files:
            if os.path.islink(filename): continue
            mode = defaultMode
            if filename.endswith(self.shlib_suffix): mode = sharedLibMode
            log.info("changing mode of %s to %o", filename, mode)
            if not self.dry_run: os.chmod(filename, mode)

    def set_dir_modes(self, dirname, mode):
        for dirpath, dirnames, fnames in os.walk(dirname):
            if os.path.islink(dirpath):
                continue
            log.info("changing mode of %s to %o", dirpath, mode)
            if not self.dry_run: os.chmod(dirpath, mode)


class PyBuildScripts(build_scripts):
    def copy_scripts(self):
        outfiles, updated_files = build_scripts.copy_scripts(self)
        fullversion = '-{0[0]}.{0[1]}'.format(sys.version_info)
        minoronly = '.{0[1]}'.format(sys.version_info)
        newoutfiles = []
        newupdated_files = []
        for filename in outfiles:
            if filename.endswith('2to3'):
                newfilename = filename + fullversion
            else:
                newfilename = filename + minoronly
            log.info('renaming %s to %s', filename, newfilename)
            os.rename(filename, newfilename)
            newoutfiles.append(newfilename)
            if filename in updated_files:
                newupdated_files.append(newfilename)
        return newoutfiles, newupdated_files


def main():
    set_compiler_flags('CFLAGS', 'PY_CFLAGS_NODIST')
    set_compiler_flags('LDFLAGS', 'PY_LDFLAGS_NODIST')

    class DummyProcess:
        """Hack for parallel build"""
        ProcessPoolExecutor = None

    sys.modules['concurrent.futures.process'] = DummyProcess
    validate_tzpath()

    # turn off warnings when deprecated modules are imported
    import warnings
    warnings.filterwarnings("ignore",category=DeprecationWarning)
    setup(# PyPI Metadata (PEP 301)
          name = "Python",
          version = sys.version.split()[0],
          url = "http://www.python.org/%d.%d" % sys.version_info[:2],
          maintainer = "Guido van Rossum and the Python community",
          maintainer_email = "python-dev@python.org",
          description = "A high-level object-oriented programming language",
          long_description = SUMMARY.strip(),
          license = "PSF license",
          classifiers = [x for x in CLASSIFIERS.split("\n") if x],
          platforms = ["Many"],

          # Build info
          cmdclass = {'build_ext': PyBuildExt,
                      'build_scripts': PyBuildScripts,
                      'install': PyBuildInstall,
                      'install_lib': PyBuildInstallLib},
          # The struct module is defined here, because build_ext won't be
          # called unless there's at least one extension module defined.
          ext_modules=[Extension('_struct', ['_struct.c'])],

          # If you change the scripts installed here, you also need to
          # check the PyBuildScripts command above, and change the links
          # created by the bininstall target in Makefile.pre.in
          scripts = ["Tools/scripts/pydoc3" ]
        )

# --install-platlib
if __name__ == '__main__':
    main()
