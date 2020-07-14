
/* Signal module -- many thanks to Lance Ellinghaus */

/* XXX Signals should be recorded per thread, now we have thread state. */

#include "Python.h"
#include "pycore_atomic.h"
#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_pyerrors.h"
#include "pycore_pystate.h"    // _PyThreadState_GET()

#ifndef MS_WINDOWS
#include "posixmodule.h"
#endif
#ifdef MS_WINDOWS
#include "socketmodule.h"   /* needed for SOCKET_T */
#endif

#ifdef MS_WINDOWS
#include <windows.h>
#ifdef HAVE_PROCESS_H
#include <process.h>
#endif
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if defined(HAVE_PTHREAD_SIGMASK) && !defined(HAVE_BROKEN_PTHREAD_SIGMASK)
#  define PYPTHREAD_SIGMASK
#endif

#if defined(PYPTHREAD_SIGMASK) && defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#endif

#ifndef SIG_ERR
#define SIG_ERR ((PyOS_sighandler_t)(-1))
#endif

#ifndef NSIG
# if defined(_NSIG)
#  define NSIG _NSIG            /* For BSD/SysV */
# elif defined(_SIGMAX)
#  define NSIG (_SIGMAX + 1)    /* For QNX */
# elif defined(SIGMAX)
#  define NSIG (SIGMAX + 1)     /* For djgpp */
# else
#  define NSIG 64               /* Use a reasonable default value */
# endif
#endif


/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(HAVE_ALARM)

PyDoc_STRVAR(signal_alarm__doc__,
"alarm($module, seconds, /)\n"
"--\n"
"\n"
"Arrange for SIGALRM to arrive after the given number of seconds.");

#define SIGNAL_ALARM_METHODDEF    \
    {"alarm", (PyCFunction)signal_alarm, METH_O, signal_alarm__doc__},

static long
signal_alarm_impl(PyObject *module, int seconds);

static PyObject *
signal_alarm(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int seconds;
    long _return_value;

    seconds = _PyLong_AsInt(arg);
    if (seconds == -1 && PyErr_Occurred()) {
        goto exit;
    }
    _return_value = signal_alarm_impl(module, seconds);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong(_return_value);

exit:
    return return_value;
}

#endif /* defined(HAVE_ALARM) */

#if defined(HAVE_PAUSE)

PyDoc_STRVAR(signal_pause__doc__,
"pause($module, /)\n"
"--\n"
"\n"
"Wait until a signal arrives.");

#define SIGNAL_PAUSE_METHODDEF    \
    {"pause", (PyCFunction)signal_pause, METH_NOARGS, signal_pause__doc__},

static PyObject *
signal_pause_impl(PyObject *module);

static PyObject *
signal_pause(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return signal_pause_impl(module);
}

#endif /* defined(HAVE_PAUSE) */

PyDoc_STRVAR(signal_raise_signal__doc__,
"raise_signal($module, signalnum, /)\n"
"--\n"
"\n"
"Send a signal to the executing process.");

#define SIGNAL_RAISE_SIGNAL_METHODDEF    \
    {"raise_signal", (PyCFunction)signal_raise_signal, METH_O, signal_raise_signal__doc__},

static PyObject *
signal_raise_signal_impl(PyObject *module, int signalnum);

static PyObject *
signal_raise_signal(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int signalnum;

    signalnum = _PyLong_AsInt(arg);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_raise_signal_impl(module, signalnum);

exit:
    return return_value;
}

PyDoc_STRVAR(signal_signal__doc__,
"signal($module, signalnum, handler, /)\n"
"--\n"
"\n"
"Set the action for the given signal.\n"
"\n"
"The action can be SIG_DFL, SIG_IGN, or a callable Python object.\n"
"The previous action is returned.  See getsignal() for possible return values.\n"
"\n"
"*** IMPORTANT NOTICE ***\n"
"A signal handler function is called with two arguments:\n"
"the first is the signal number, the second is the interrupted stack frame.");

#define SIGNAL_SIGNAL_METHODDEF    \
    {"signal", (PyCFunction)(void(*)(void))signal_signal, METH_FASTCALL, signal_signal__doc__},

static PyObject *
signal_signal_impl(PyObject *module, int signalnum, PyObject *handler);

static PyObject *
signal_signal(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int signalnum;
    PyObject *handler;

    if (!_PyArg_CheckPositional("signal", nargs, 2, 2)) {
        goto exit;
    }
    signalnum = _PyLong_AsInt(args[0]);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    handler = args[1];
    return_value = signal_signal_impl(module, signalnum, handler);

exit:
    return return_value;
}

PyDoc_STRVAR(signal_getsignal__doc__,
"getsignal($module, signalnum, /)\n"
"--\n"
"\n"
"Return the current action for the given signal.\n"
"\n"
"The return value can be:\n"
"  SIG_IGN -- if the signal is being ignored\n"
"  SIG_DFL -- if the default action for the signal is in effect\n"
"  None    -- if an unknown handler is in effect\n"
"  anything else -- the callable Python object used as a handler");

#define SIGNAL_GETSIGNAL_METHODDEF    \
    {"getsignal", (PyCFunction)signal_getsignal, METH_O, signal_getsignal__doc__},

static PyObject *
signal_getsignal_impl(PyObject *module, int signalnum);

static PyObject *
signal_getsignal(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int signalnum;

    signalnum = _PyLong_AsInt(arg);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_getsignal_impl(module, signalnum);

exit:
    return return_value;
}

PyDoc_STRVAR(signal_strsignal__doc__,
"strsignal($module, signalnum, /)\n"
"--\n"
"\n"
"Return the system description of the given signal.\n"
"\n"
"The return values can be such as \"Interrupt\", \"Segmentation fault\", etc.\n"
"Returns None if the signal is not recognized.");

#define SIGNAL_STRSIGNAL_METHODDEF    \
    {"strsignal", (PyCFunction)signal_strsignal, METH_O, signal_strsignal__doc__},

static PyObject *
signal_strsignal_impl(PyObject *module, int signalnum);

static PyObject *
signal_strsignal(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int signalnum;

    signalnum = _PyLong_AsInt(arg);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_strsignal_impl(module, signalnum);

exit:
    return return_value;
}

#if defined(HAVE_SIGINTERRUPT)

PyDoc_STRVAR(signal_siginterrupt__doc__,
"siginterrupt($module, signalnum, flag, /)\n"
"--\n"
"\n"
"Change system call restart behaviour.\n"
"\n"
"If flag is False, system calls will be restarted when interrupted by\n"
"signal sig, else system calls will be interrupted.");

#define SIGNAL_SIGINTERRUPT_METHODDEF    \
    {"siginterrupt", (PyCFunction)(void(*)(void))signal_siginterrupt, METH_FASTCALL, signal_siginterrupt__doc__},

static PyObject *
signal_siginterrupt_impl(PyObject *module, int signalnum, int flag);

static PyObject *
signal_siginterrupt(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int signalnum;
    int flag;

    if (!_PyArg_CheckPositional("siginterrupt", nargs, 2, 2)) {
        goto exit;
    }
    signalnum = _PyLong_AsInt(args[0]);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    flag = _PyLong_AsInt(args[1]);
    if (flag == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_siginterrupt_impl(module, signalnum, flag);

exit:
    return return_value;
}

#endif /* defined(HAVE_SIGINTERRUPT) */

#if defined(HAVE_SETITIMER)

PyDoc_STRVAR(signal_setitimer__doc__,
"setitimer($module, which, seconds, interval=0.0, /)\n"
"--\n"
"\n"
"Sets given itimer (one of ITIMER_REAL, ITIMER_VIRTUAL or ITIMER_PROF).\n"
"\n"
"The timer will fire after value seconds and after that every interval seconds.\n"
"The itimer can be cleared by setting seconds to zero.\n"
"\n"
"Returns old values as a tuple: (delay, interval).");

#define SIGNAL_SETITIMER_METHODDEF    \
    {"setitimer", (PyCFunction)(void(*)(void))signal_setitimer, METH_FASTCALL, signal_setitimer__doc__},

static PyObject *
signal_setitimer_impl(PyObject *module, int which, PyObject *seconds,
                      PyObject *interval);

static PyObject *
signal_setitimer(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int which;
    PyObject *seconds;
    PyObject *interval = NULL;

    if (!_PyArg_CheckPositional("setitimer", nargs, 2, 3)) {
        goto exit;
    }
    which = _PyLong_AsInt(args[0]);
    if (which == -1 && PyErr_Occurred()) {
        goto exit;
    }
    seconds = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    interval = args[2];
skip_optional:
    return_value = signal_setitimer_impl(module, which, seconds, interval);

exit:
    return return_value;
}

#endif /* defined(HAVE_SETITIMER) */

#if defined(HAVE_GETITIMER)

PyDoc_STRVAR(signal_getitimer__doc__,
"getitimer($module, which, /)\n"
"--\n"
"\n"
"Returns current value of given itimer.");

#define SIGNAL_GETITIMER_METHODDEF    \
    {"getitimer", (PyCFunction)signal_getitimer, METH_O, signal_getitimer__doc__},

static PyObject *
signal_getitimer_impl(PyObject *module, int which);

static PyObject *
signal_getitimer(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int which;

    which = _PyLong_AsInt(arg);
    if (which == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_getitimer_impl(module, which);

exit:
    return return_value;
}

#endif /* defined(HAVE_GETITIMER) */

#if defined(PYPTHREAD_SIGMASK)

PyDoc_STRVAR(signal_pthread_sigmask__doc__,
"pthread_sigmask($module, how, mask, /)\n"
"--\n"
"\n"
"Fetch and/or change the signal mask of the calling thread.");

#define SIGNAL_PTHREAD_SIGMASK_METHODDEF    \
    {"pthread_sigmask", (PyCFunction)(void(*)(void))signal_pthread_sigmask, METH_FASTCALL, signal_pthread_sigmask__doc__},

static PyObject *
signal_pthread_sigmask_impl(PyObject *module, int how, sigset_t mask);

static PyObject *
signal_pthread_sigmask(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int how;
    sigset_t mask;

    if (!_PyArg_CheckPositional("pthread_sigmask", nargs, 2, 2)) {
        goto exit;
    }
    how = _PyLong_AsInt(args[0]);
    if (how == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!_Py_Sigset_Converter(args[1], &mask)) {
        goto exit;
    }
    return_value = signal_pthread_sigmask_impl(module, how, mask);

exit:
    return return_value;
}

#endif /* defined(PYPTHREAD_SIGMASK) */

#if defined(HAVE_SIGPENDING)

PyDoc_STRVAR(signal_sigpending__doc__,
"sigpending($module, /)\n"
"--\n"
"\n"
"Examine pending signals.\n"
"\n"
"Returns a set of signal numbers that are pending for delivery to\n"
"the calling thread.");

#define SIGNAL_SIGPENDING_METHODDEF    \
    {"sigpending", (PyCFunction)signal_sigpending, METH_NOARGS, signal_sigpending__doc__},

static PyObject *
signal_sigpending_impl(PyObject *module);

static PyObject *
signal_sigpending(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return signal_sigpending_impl(module);
}

#endif /* defined(HAVE_SIGPENDING) */

#if defined(HAVE_SIGWAIT)

PyDoc_STRVAR(signal_sigwait__doc__,
"sigwait($module, sigset, /)\n"
"--\n"
"\n"
"Wait for a signal.\n"
"\n"
"Suspend execution of the calling thread until the delivery of one of the\n"
"signals specified in the signal set sigset.  The function accepts the signal\n"
"and returns the signal number.");

#define SIGNAL_SIGWAIT_METHODDEF    \
    {"sigwait", (PyCFunction)signal_sigwait, METH_O, signal_sigwait__doc__},

static PyObject *
signal_sigwait_impl(PyObject *module, sigset_t sigset);

static PyObject *
signal_sigwait(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    sigset_t sigset;

    if (!_Py_Sigset_Converter(arg, &sigset)) {
        goto exit;
    }
    return_value = signal_sigwait_impl(module, sigset);

exit:
    return return_value;
}

#endif /* defined(HAVE_SIGWAIT) */

#if (defined(HAVE_SIGFILLSET) || defined(MS_WINDOWS))

PyDoc_STRVAR(signal_valid_signals__doc__,
"valid_signals($module, /)\n"
"--\n"
"\n"
"Return a set of valid signal numbers on this platform.\n"
"\n"
"The signal numbers returned by this function can be safely passed to\n"
"functions like `pthread_sigmask`.");

#define SIGNAL_VALID_SIGNALS_METHODDEF    \
    {"valid_signals", (PyCFunction)signal_valid_signals, METH_NOARGS, signal_valid_signals__doc__},

static PyObject *
signal_valid_signals_impl(PyObject *module);

static PyObject *
signal_valid_signals(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return signal_valid_signals_impl(module);
}

#endif /* (defined(HAVE_SIGFILLSET) || defined(MS_WINDOWS)) */

#if defined(HAVE_SIGWAITINFO)

PyDoc_STRVAR(signal_sigwaitinfo__doc__,
"sigwaitinfo($module, sigset, /)\n"
"--\n"
"\n"
"Wait synchronously until one of the signals in *sigset* is delivered.\n"
"\n"
"Returns a struct_siginfo containing information about the signal.");

#define SIGNAL_SIGWAITINFO_METHODDEF    \
    {"sigwaitinfo", (PyCFunction)signal_sigwaitinfo, METH_O, signal_sigwaitinfo__doc__},

static PyObject *
signal_sigwaitinfo_impl(PyObject *module, sigset_t sigset);

static PyObject *
signal_sigwaitinfo(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    sigset_t sigset;

    if (!_Py_Sigset_Converter(arg, &sigset)) {
        goto exit;
    }
    return_value = signal_sigwaitinfo_impl(module, sigset);

exit:
    return return_value;
}

#endif /* defined(HAVE_SIGWAITINFO) */

#if defined(HAVE_SIGTIMEDWAIT)

PyDoc_STRVAR(signal_sigtimedwait__doc__,
"sigtimedwait($module, sigset, timeout, /)\n"
"--\n"
"\n"
"Like sigwaitinfo(), but with a timeout.\n"
"\n"
"The timeout is specified in seconds, with floating point numbers allowed.");

#define SIGNAL_SIGTIMEDWAIT_METHODDEF    \
    {"sigtimedwait", (PyCFunction)(void(*)(void))signal_sigtimedwait, METH_FASTCALL, signal_sigtimedwait__doc__},

static PyObject *
signal_sigtimedwait_impl(PyObject *module, sigset_t sigset,
                         PyObject *timeout_obj);

static PyObject *
signal_sigtimedwait(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    sigset_t sigset;
    PyObject *timeout_obj;

    if (!_PyArg_CheckPositional("sigtimedwait", nargs, 2, 2)) {
        goto exit;
    }
    if (!_Py_Sigset_Converter(args[0], &sigset)) {
        goto exit;
    }
    timeout_obj = args[1];
    return_value = signal_sigtimedwait_impl(module, sigset, timeout_obj);

exit:
    return return_value;
}

#endif /* defined(HAVE_SIGTIMEDWAIT) */

#if defined(HAVE_PTHREAD_KILL)

PyDoc_STRVAR(signal_pthread_kill__doc__,
"pthread_kill($module, thread_id, signalnum, /)\n"
"--\n"
"\n"
"Send a signal to a thread.");

#define SIGNAL_PTHREAD_KILL_METHODDEF    \
    {"pthread_kill", (PyCFunction)(void(*)(void))signal_pthread_kill, METH_FASTCALL, signal_pthread_kill__doc__},

static PyObject *
signal_pthread_kill_impl(PyObject *module, unsigned long thread_id,
                         int signalnum);

static PyObject *
signal_pthread_kill(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    unsigned long thread_id;
    int signalnum;

    if (!_PyArg_CheckPositional("pthread_kill", nargs, 2, 2)) {
        goto exit;
    }
    if (!PyLong_Check(args[0])) {
        _PyArg_BadArgument("pthread_kill", "argument 1", "int", args[0]);
        goto exit;
    }
    thread_id = PyLong_AsUnsignedLongMask(args[0]);
    signalnum = _PyLong_AsInt(args[1]);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = signal_pthread_kill_impl(module, thread_id, signalnum);

exit:
    return return_value;
}

#endif /* defined(HAVE_PTHREAD_KILL) */

#if (defined(__linux__) && defined(__NR_pidfd_send_signal))

PyDoc_STRVAR(signal_pidfd_send_signal__doc__,
"pidfd_send_signal($module, pidfd, signalnum, siginfo=None, flags=0, /)\n"
"--\n"
"\n"
"Send a signal to a process referred to by a pid file descriptor.");

#define SIGNAL_PIDFD_SEND_SIGNAL_METHODDEF    \
    {"pidfd_send_signal", (PyCFunction)(void(*)(void))signal_pidfd_send_signal, METH_FASTCALL, signal_pidfd_send_signal__doc__},

static PyObject *
signal_pidfd_send_signal_impl(PyObject *module, int pidfd, int signalnum,
                              PyObject *siginfo, int flags);

static PyObject *
signal_pidfd_send_signal(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int pidfd;
    int signalnum;
    PyObject *siginfo = Py_None;
    int flags = 0;

    if (!_PyArg_CheckPositional("pidfd_send_signal", nargs, 2, 4)) {
        goto exit;
    }
    pidfd = _PyLong_AsInt(args[0]);
    if (pidfd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    signalnum = _PyLong_AsInt(args[1]);
    if (signalnum == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    siginfo = args[2];
    if (nargs < 4) {
        goto skip_optional;
    }
    flags = _PyLong_AsInt(args[3]);
    if (flags == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = signal_pidfd_send_signal_impl(module, pidfd, signalnum, siginfo, flags);

exit:
    return return_value;
}

#endif /* (defined(__linux__) && defined(__NR_pidfd_send_signal)) */

#ifndef SIGNAL_ALARM_METHODDEF
    #define SIGNAL_ALARM_METHODDEF
#endif /* !defined(SIGNAL_ALARM_METHODDEF) */

#ifndef SIGNAL_PAUSE_METHODDEF
    #define SIGNAL_PAUSE_METHODDEF
#endif /* !defined(SIGNAL_PAUSE_METHODDEF) */

#ifndef SIGNAL_SIGINTERRUPT_METHODDEF
    #define SIGNAL_SIGINTERRUPT_METHODDEF
#endif /* !defined(SIGNAL_SIGINTERRUPT_METHODDEF) */

#ifndef SIGNAL_SETITIMER_METHODDEF
    #define SIGNAL_SETITIMER_METHODDEF
#endif /* !defined(SIGNAL_SETITIMER_METHODDEF) */

#ifndef SIGNAL_GETITIMER_METHODDEF
    #define SIGNAL_GETITIMER_METHODDEF
#endif /* !defined(SIGNAL_GETITIMER_METHODDEF) */

#ifndef SIGNAL_PTHREAD_SIGMASK_METHODDEF
    #define SIGNAL_PTHREAD_SIGMASK_METHODDEF
#endif /* !defined(SIGNAL_PTHREAD_SIGMASK_METHODDEF) */

#ifndef SIGNAL_SIGPENDING_METHODDEF
    #define SIGNAL_SIGPENDING_METHODDEF
#endif /* !defined(SIGNAL_SIGPENDING_METHODDEF) */

#ifndef SIGNAL_SIGWAIT_METHODDEF
    #define SIGNAL_SIGWAIT_METHODDEF
#endif /* !defined(SIGNAL_SIGWAIT_METHODDEF) */

#ifndef SIGNAL_VALID_SIGNALS_METHODDEF
    #define SIGNAL_VALID_SIGNALS_METHODDEF
#endif /* !defined(SIGNAL_VALID_SIGNALS_METHODDEF) */

#ifndef SIGNAL_SIGWAITINFO_METHODDEF
    #define SIGNAL_SIGWAITINFO_METHODDEF
#endif /* !defined(SIGNAL_SIGWAITINFO_METHODDEF) */

#ifndef SIGNAL_SIGTIMEDWAIT_METHODDEF
    #define SIGNAL_SIGTIMEDWAIT_METHODDEF
#endif /* !defined(SIGNAL_SIGTIMEDWAIT_METHODDEF) */

#ifndef SIGNAL_PTHREAD_KILL_METHODDEF
    #define SIGNAL_PTHREAD_KILL_METHODDEF
#endif /* !defined(SIGNAL_PTHREAD_KILL_METHODDEF) */

#ifndef SIGNAL_PIDFD_SEND_SIGNAL_METHODDEF
    #define SIGNAL_PIDFD_SEND_SIGNAL_METHODDEF
#endif /* !defined(SIGNAL_PIDFD_SEND_SIGNAL_METHODDEF) */
/*[clinic end generated code: output=dff93c869101f043 input=a9049054013a1b77]*/

/*[clinic input]
module signal
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=b0301a3bde5fe9d3]*/

/*[python input]

class sigset_t_converter(CConverter):
    type = 'sigset_t'
    converter = '_Py_Sigset_Converter'

[python start generated code]*/
/*[python end generated code: output=da39a3ee5e6b4b0d input=b5689d14466b6823]*/

/*
   NOTES ON THE INTERACTION BETWEEN SIGNALS AND THREADS

   We want the following semantics:

   - only the main thread can set a signal handler
   - only the main thread runs the signal handler
   - signals can be delivered to any thread
   - any thread can get a signal handler

   I.e. we don't support "synchronous signals" like SIGFPE (catching
   this doesn't make much sense in Python anyway) nor do we support
   signals as a means of inter-thread communication, since not all
   thread implementations support that (at least our thread library
   doesn't).

   We still have the problem that in some implementations signals
   generated by the keyboard (e.g. SIGINT) are delivered to all
   threads (e.g. SGI), while in others (e.g. Solaris) such signals are
   delivered to one random thread. On Linux, signals are delivered to
   the main thread (unless the main thread is blocking the signal, for
   example because it's already handling the same signal).  Since we
   allow signals to be delivered to any thread, this works fine. The
   only oddity is that the thread executing the Python signal handler
   may not be the thread that received the signal.
*/

static volatile struct {
    _Py_atomic_int tripped;
    PyObject *func;
} Handlers[NSIG];

#ifdef MS_WINDOWS
#define INVALID_FD ((SOCKET_T)-1)

static volatile struct {
    SOCKET_T fd;
    int warn_on_full_buffer;
    int use_send;
} wakeup = {.fd = INVALID_FD, .warn_on_full_buffer = 1, .use_send = 0};
#else
#define INVALID_FD (-1)
static volatile struct {
    sig_atomic_t fd;
    int warn_on_full_buffer;
} wakeup = {.fd = INVALID_FD, .warn_on_full_buffer = 1};
#endif

/* Speed up sigcheck() when none tripped */
static _Py_atomic_int is_tripped;

static PyObject *DefaultHandler;
static PyObject *IgnoreHandler;
static PyObject *IntHandler;

#ifdef MS_WINDOWS
static HANDLE sigint_event = NULL;
#endif

#ifdef HAVE_GETITIMER
static PyObject *ItimerError;

/* auxiliary functions for setitimer */
static int
timeval_from_double(PyObject *obj, struct timeval *tv)
{
    if (obj == NULL) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
        return 0;
    }

    _PyTime_t t;
    if (_PyTime_FromSecondsObject(&t, obj, _PyTime_ROUND_CEILING) < 0) {
        return -1;
    }
    return _PyTime_AsTimeval(t, tv, _PyTime_ROUND_CEILING);
}

Py_LOCAL_INLINE(double)
double_from_timeval(struct timeval *tv)
{
    return tv->tv_sec + (double)(tv->tv_usec / 1000000.0);
}

static PyObject *
itimer_retval(struct itimerval *iv)
{
    PyObject *r, *v;

    r = PyTuple_New(2);
    if (r == NULL)
        return NULL;

    if(!(v = PyFloat_FromDouble(double_from_timeval(&iv->it_value)))) {
        Py_DECREF(r);
        return NULL;
    }

    PyTuple_SET_ITEM(r, 0, v);

    if(!(v = PyFloat_FromDouble(double_from_timeval(&iv->it_interval)))) {
        Py_DECREF(r);
        return NULL;
    }

    PyTuple_SET_ITEM(r, 1, v);

    return r;
}
#endif

static PyObject *
signal_default_int_handler(PyObject *self, PyObject *args)
{
    PyErr_SetNone(PyExc_KeyboardInterrupt);
    return NULL;
}

PyDoc_STRVAR(default_int_handler_doc,
"default_int_handler(...)\n\
\n\
The default handler for SIGINT installed by Python.\n\
It raises KeyboardInterrupt.");


static int
report_wakeup_write_error(void *data)
{
    PyObject *exc, *val, *tb;
    int save_errno = errno;
    errno = (int) (intptr_t) data;
    PyErr_Fetch(&exc, &val, &tb);
    PyErr_SetFromErrno(PyExc_OSError);
    PySys_WriteStderr("Exception ignored when trying to write to the "
                      "signal wakeup fd:\n");
    PyErr_WriteUnraisable(NULL);
    PyErr_Restore(exc, val, tb);
    errno = save_errno;
    return 0;
}

#ifdef MS_WINDOWS
static int
report_wakeup_send_error(void* data)
{
    PyObject *exc, *val, *tb;
    PyErr_Fetch(&exc, &val, &tb);
    /* PyErr_SetExcFromWindowsErr() invokes FormatMessage() which
       recognizes the error codes used by both GetLastError() and
       WSAGetLastError */
    PyErr_SetExcFromWindowsErr(PyExc_OSError, (int) (intptr_t) data);
    PySys_WriteStderr("Exception ignored when trying to send to the "
                      "signal wakeup fd:\n");
    PyErr_WriteUnraisable(NULL);
    PyErr_Restore(exc, val, tb);
    return 0;
}
#endif   /* MS_WINDOWS */

static void
trip_signal(int sig_num)
{
    unsigned char byte;
    int fd;
    Py_ssize_t rc;

    _Py_atomic_store_relaxed(&Handlers[sig_num].tripped, 1);

    /* Set is_tripped after setting .tripped, as it gets
       cleared in PyErr_CheckSignals() before .tripped. */
    _Py_atomic_store(&is_tripped, 1);

    /* Signals are always handled by the main interpreter */
    PyInterpreterState *interp = _PyRuntime.interpreters.main;

    /* Notify ceval.c */
    _PyEval_SignalReceived(interp);

    /* And then write to the wakeup fd *after* setting all the globals and
       doing the _PyEval_SignalReceived. We used to write to the wakeup fd
       and then set the flag, but this allowed the following sequence of events
       (especially on windows, where trip_signal may run in a new thread):

       - main thread blocks on select([wakeup.fd], ...)
       - signal arrives
       - trip_signal writes to the wakeup fd
       - the main thread wakes up
       - the main thread checks the signal flags, sees that they're unset
       - the main thread empties the wakeup fd
       - the main thread goes back to sleep
       - trip_signal sets the flags to request the Python-level signal handler
         be run
       - the main thread doesn't notice, because it's asleep

       See bpo-30038 for more details.
    */

#ifdef MS_WINDOWS
    fd = Py_SAFE_DOWNCAST(wakeup.fd, SOCKET_T, int);
#else
    fd = wakeup.fd;
#endif

    if (fd != INVALID_FD) {
        byte = (unsigned char)sig_num;
#ifdef MS_WINDOWS
        if (wakeup.use_send) {
            rc = send(fd, &byte, 1, 0);

            if (rc < 0) {
                int last_error = GetLastError();
                if (wakeup.warn_on_full_buffer ||
                    last_error != WSAEWOULDBLOCK)
                {
                    /* _PyEval_AddPendingCall() isn't signal-safe, but we
                       still use it for this exceptional case. */
                    _PyEval_AddPendingCall(interp,
                                           report_wakeup_send_error,
                                           (void *)(intptr_t) last_error);
                }
            }
        }
        else
#endif
        {
            /* _Py_write_noraise() retries write() if write() is interrupted by
               a signal (fails with EINTR). */
            rc = _Py_write_noraise(fd, &byte, 1);

            if (rc < 0) {
                if (wakeup.warn_on_full_buffer ||
                    (errno != EWOULDBLOCK && errno != EAGAIN))
                {
                    /* _PyEval_AddPendingCall() isn't signal-safe, but we
                       still use it for this exceptional case. */
                    _PyEval_AddPendingCall(interp,
                                           report_wakeup_write_error,
                                           (void *)(intptr_t)errno);
                }
            }
        }
    }
}

static void
signal_handler(int sig_num)
{
    int save_errno = errno;

    trip_signal(sig_num);

#ifndef HAVE_SIGACTION
#ifdef SIGCHLD
    /* To avoid infinite recursion, this signal remains
       reset until explicit re-instated.
       Don't clear the 'func' field as it is our pointer
       to the Python handler... */
    if (sig_num != SIGCHLD)
#endif
    /* If the handler was not set up with sigaction, reinstall it.  See
     * Python/pylifecycle.c for the implementation of PyOS_setsig which
     * makes this true.  See also issue8354. */
    PyOS_setsig(sig_num, signal_handler);
#endif

    /* Issue #10311: asynchronously executing signal handlers should not
       mutate errno under the feet of unsuspecting C code. */
    errno = save_errno;

#ifdef MS_WINDOWS
    if (sig_num == SIGINT)
        SetEvent(sigint_event);
#endif
}


#ifdef HAVE_ALARM

/*[clinic input]
signal.alarm -> long

    seconds: int
    /

Arrange for SIGALRM to arrive after the given number of seconds.
[clinic start generated code]*/

static long
signal_alarm_impl(PyObject *module, int seconds)
/*[clinic end generated code: output=144232290814c298 input=0d5e97e0e6f39e86]*/
{
    /* alarm() returns the number of seconds remaining */
    return (long)alarm(seconds);
}

#endif

#ifdef HAVE_PAUSE

/*[clinic input]
signal.pause

Wait until a signal arrives.
[clinic start generated code]*/

static PyObject *
signal_pause_impl(PyObject *module)
/*[clinic end generated code: output=391656788b3c3929 input=f03de0f875752062]*/
{
    Py_BEGIN_ALLOW_THREADS
    (void)pause();
    Py_END_ALLOW_THREADS
    /* make sure that any exceptions that got raised are propagated
     * back into Python
     */
    if (PyErr_CheckSignals())
        return NULL;

    Py_RETURN_NONE;
}

#endif

/*[clinic input]
signal.raise_signal

    signalnum: int
    /

Send a signal to the executing process.
[clinic start generated code]*/

static PyObject *
signal_raise_signal_impl(PyObject *module, int signalnum)
/*[clinic end generated code: output=e2b014220aa6111d input=e90c0f9a42358de6]*/
{
    int err;
    Py_BEGIN_ALLOW_THREADS
    _Py_BEGIN_SUPPRESS_IPH
    err = raise(signalnum);
    _Py_END_SUPPRESS_IPH
    Py_END_ALLOW_THREADS

    if (err) {
        return PyErr_SetFromErrno(PyExc_OSError);
    }
    Py_RETURN_NONE;
}

/*[clinic input]
signal.signal

    signalnum: int
    handler:   object
    /

Set the action for the given signal.

The action can be SIG_DFL, SIG_IGN, or a callable Python object.
The previous action is returned.  See getsignal() for possible return values.

*** IMPORTANT NOTICE ***
A signal handler function is called with two arguments:
the first is the signal number, the second is the interrupted stack frame.
[clinic start generated code]*/

static PyObject *
signal_signal_impl(PyObject *module, int signalnum, PyObject *handler)
/*[clinic end generated code: output=b44cfda43780f3a1 input=deee84af5fa0432c]*/
{
    PyObject *old_handler;
    void (*func)(int);
#ifdef MS_WINDOWS
    /* Validate that signalnum is one of the allowable signals */
    switch (signalnum) {
        case SIGABRT: break;
#ifdef SIGBREAK
        /* Issue #10003: SIGBREAK is not documented as permitted, but works
           and corresponds to CTRL_BREAK_EVENT. */
        case SIGBREAK: break;
#endif
        case SIGFPE: break;
        case SIGILL: break;
        case SIGINT: break;
        case SIGSEGV: break;
        case SIGTERM: break;
        default:
            PyErr_SetString(PyExc_ValueError, "invalid signal value");
            return NULL;
    }
#endif

    PyThreadState *tstate = _PyThreadState_GET();
    if (!_Py_ThreadCanHandleSignals(tstate->interp)) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "signal only works in main thread "
                         "of the main interpreter");
        return NULL;
    }
    if (signalnum < 1 || signalnum >= NSIG) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "signal number out of range");
        return NULL;
    }
    if (handler == IgnoreHandler) {
        func = SIG_IGN;
    }
    else if (handler == DefaultHandler) {
        func = SIG_DFL;
    }
    else if (!PyCallable_Check(handler)) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "signal handler must be signal.SIG_IGN, "
                         "signal.SIG_DFL, or a callable object");
        return NULL;
    }
    else {
        func = signal_handler;
    }

    /* Check for pending signals before changing signal handler */
    if (_PyErr_CheckSignalsTstate(tstate)) {
        return NULL;
    }
    if (PyOS_setsig(signalnum, func) == SIG_ERR) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    old_handler = Handlers[signalnum].func;
    Py_INCREF(handler);
    Handlers[signalnum].func = handler;

    if (old_handler != NULL) {
        return old_handler;
    }
    else {
        Py_RETURN_NONE;
    }
}


/*[clinic input]
signal.getsignal

    signalnum: int
    /

Return the current action for the given signal.

The return value can be:
  SIG_IGN -- if the signal is being ignored
  SIG_DFL -- if the default action for the signal is in effect
  None    -- if an unknown handler is in effect
  anything else -- the callable Python object used as a handler
[clinic start generated code]*/

static PyObject *
signal_getsignal_impl(PyObject *module, int signalnum)
/*[clinic end generated code: output=35b3e0e796fd555e input=ac23a00f19dfa509]*/
{
    PyObject *old_handler;
    if (signalnum < 1 || signalnum >= NSIG) {
        PyErr_SetString(PyExc_ValueError,
                        "signal number out of range");
        return NULL;
    }
    old_handler = Handlers[signalnum].func;
    if (old_handler != NULL) {
        Py_INCREF(old_handler);
        return old_handler;
    }
    else {
        Py_RETURN_NONE;
    }
}


/*[clinic input]
signal.strsignal

    signalnum: int
    /

Return the system description of the given signal.

The return values can be such as "Interrupt", "Segmentation fault", etc.
Returns None if the signal is not recognized.
[clinic start generated code]*/

static PyObject *
signal_strsignal_impl(PyObject *module, int signalnum)
/*[clinic end generated code: output=44e12e1e3b666261 input=b77914b03f856c74]*/
{
    char *res;

    if (signalnum < 1 || signalnum >= NSIG) {
        PyErr_SetString(PyExc_ValueError,
                "signal number out of range");
        return NULL;
    }

#ifndef HAVE_STRSIGNAL
    switch (signalnum) {
        /* Though being a UNIX, HP-UX does not provide strsignal(3). */
#ifndef MS_WINDOWS
        case SIGHUP:
            res = "Hangup";
            break;
        case SIGALRM:
            res = "Alarm clock";
            break;
        case SIGPIPE:
            res = "Broken pipe";
            break;
        case SIGQUIT:
            res = "Quit";
            break;
        case SIGCHLD:
            res = "Child exited";
            break;
#endif
        /* Custom redefinition of POSIX signals allowed on Windows. */
        case SIGINT:
            res = "Interrupt";
            break;
        case SIGILL:
            res = "Illegal instruction";
            break;
        case SIGABRT:
            res = "Aborted";
            break;
        case SIGFPE:
            res = "Floating point exception";
            break;
        case SIGSEGV:
            res = "Segmentation fault";
            break;
        case SIGTERM:
            res = "Terminated";
            break;
        default:
            Py_RETURN_NONE;
    }
#else
    errno = 0;
    res = strsignal(signalnum);

    if (errno || res == NULL || strstr(res, "Unknown signal") != NULL)
        Py_RETURN_NONE;
#endif

    return Py_BuildValue("s", res);
}

#ifdef HAVE_SIGINTERRUPT

/*[clinic input]
signal.siginterrupt

    signalnum: int
    flag:      int
    /

Change system call restart behaviour.

If flag is False, system calls will be restarted when interrupted by
signal sig, else system calls will be interrupted.
[clinic start generated code]*/

static PyObject *
signal_siginterrupt_impl(PyObject *module, int signalnum, int flag)
/*[clinic end generated code: output=063816243d85dd19 input=4160acacca3e2099]*/
{
    if (signalnum < 1 || signalnum >= NSIG) {
        PyErr_SetString(PyExc_ValueError,
                        "signal number out of range");
        return NULL;
    }
    if (siginterrupt(signalnum, flag)<0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_NONE;
}

#endif


static PyObject*
signal_set_wakeup_fd(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct _Py_stat_struct status;
    static char *kwlist[] = {
        "", "warn_on_full_buffer", NULL,
    };
    int warn_on_full_buffer = 1;
#ifdef MS_WINDOWS
    PyObject *fdobj;
    SOCKET_T sockfd, old_sockfd;
    int res;
    int res_size = sizeof res;
    PyObject *mod;
    int is_socket;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|$p:set_wakeup_fd", kwlist,
                                     &fdobj, &warn_on_full_buffer))
        return NULL;

    sockfd = PyLong_AsSocket_t(fdobj);
    if (sockfd == (SOCKET_T)(-1) && PyErr_Occurred())
        return NULL;
#else
    int fd, old_fd;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|$p:set_wakeup_fd", kwlist,
                                     &fd, &warn_on_full_buffer))
        return NULL;
#endif

    PyThreadState *tstate = _PyThreadState_GET();
    if (!_Py_ThreadCanHandleSignals(tstate->interp)) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "set_wakeup_fd only works in main thread "
                         "of the main interpreter");
        return NULL;
    }

#ifdef MS_WINDOWS
    is_socket = 0;
    if (sockfd != INVALID_FD) {
        /* Import the _socket module to call WSAStartup() */
        mod = PyImport_ImportModuleNoBlock("_socket");
        if (mod == NULL)
            return NULL;
        Py_DECREF(mod);

        /* test the socket */
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                       (char *)&res, &res_size) != 0) {
            int fd, err;

            err = WSAGetLastError();
            if (err != WSAENOTSOCK) {
                PyErr_SetExcFromWindowsErr(PyExc_OSError, err);
                return NULL;
            }

            fd = (int)sockfd;
            if ((SOCKET_T)fd != sockfd) {
                _PyErr_SetString(tstate, PyExc_ValueError, "invalid fd");
                return NULL;
            }

            if (_Py_fstat(fd, &status) != 0) {
                return NULL;
            }

            /* on Windows, a file cannot be set to non-blocking mode */
        }
        else {
            is_socket = 1;

            /* Windows does not provide a function to test if a socket
               is in non-blocking mode */
        }
    }

    old_sockfd = wakeup.fd;
    wakeup.fd = sockfd;
    wakeup.warn_on_full_buffer = warn_on_full_buffer;
    wakeup.use_send = is_socket;

    if (old_sockfd != INVALID_FD)
        return PyLong_FromSocket_t(old_sockfd);
    else
        return PyLong_FromLong(-1);
#else
    if (fd != -1) {
        int blocking;

        if (_Py_fstat(fd, &status) != 0)
            return NULL;

        blocking = _Py_get_blocking(fd);
        if (blocking < 0)
            return NULL;
        if (blocking) {
            _PyErr_Format(tstate, PyExc_ValueError,
                          "the fd %i must be in non-blocking mode",
                          fd);
            return NULL;
        }
    }

    old_fd = wakeup.fd;
    wakeup.fd = fd;
    wakeup.warn_on_full_buffer = warn_on_full_buffer;

    return PyLong_FromLong(old_fd);
#endif
}

PyDoc_STRVAR(set_wakeup_fd_doc,
"set_wakeup_fd(fd, *, warn_on_full_buffer=True) -> fd\n\
\n\
Sets the fd to be written to (with the signal number) when a signal\n\
comes in.  A library can use this to wakeup select or poll.\n\
The previous fd or -1 is returned.\n\
\n\
The fd must be non-blocking.");

/* C API for the same, without all the error checking */
int
PySignal_SetWakeupFd(int fd)
{
    int old_fd;
    if (fd < 0)
        fd = -1;

#ifdef MS_WINDOWS
    old_fd = Py_SAFE_DOWNCAST(wakeup.fd, SOCKET_T, int);
#else
    old_fd = wakeup.fd;
#endif
    wakeup.fd = fd;
    wakeup.warn_on_full_buffer = 1;
    return old_fd;
}


#ifdef HAVE_SETITIMER

/*[clinic input]
signal.setitimer

    which:    int
    seconds:  object
    interval: object(c_default="NULL") = 0.0
    /

Sets given itimer (one of ITIMER_REAL, ITIMER_VIRTUAL or ITIMER_PROF).

The timer will fire after value seconds and after that every interval seconds.
The itimer can be cleared by setting seconds to zero.

Returns old values as a tuple: (delay, interval).
[clinic start generated code]*/

static PyObject *
signal_setitimer_impl(PyObject *module, int which, PyObject *seconds,
                      PyObject *interval)
/*[clinic end generated code: output=65f9dcbddc35527b input=de43daf194e6f66f]*/
{
    struct itimerval new, old;

    if (timeval_from_double(seconds, &new.it_value) < 0) {
        return NULL;
    }
    if (timeval_from_double(interval, &new.it_interval) < 0) {
        return NULL;
    }

    /* Let OS check "which" value */
    if (setitimer(which, &new, &old) != 0) {
        PyErr_SetFromErrno(ItimerError);
        return NULL;
    }

    return itimer_retval(&old);
}

#endif


#ifdef HAVE_GETITIMER

/*[clinic input]
signal.getitimer

    which:    int
    /

Returns current value of given itimer.
[clinic start generated code]*/

static PyObject *
signal_getitimer_impl(PyObject *module, int which)
/*[clinic end generated code: output=9e053175d517db40 input=f7d21d38f3490627]*/
{
    struct itimerval old;

    if (getitimer(which, &old) != 0) {
        PyErr_SetFromErrno(ItimerError);
        return NULL;
    }

    return itimer_retval(&old);
}

#endif

#if defined(PYPTHREAD_SIGMASK) || defined(HAVE_SIGPENDING)
static PyObject*
sigset_to_set(sigset_t mask)
{
    PyObject *signum, *result;
    int sig;

    result = PySet_New(0);
    if (result == NULL)
        return NULL;

    for (sig = 1; sig < NSIG; sig++) {
        if (sigismember(&mask, sig) != 1)
            continue;

        /* Handle the case where it is a member by adding the signal to
           the result list.  Ignore the other cases because they mean the
           signal isn't a member of the mask or the signal was invalid,
           and an invalid signal must have been our fault in constructing
           the loop boundaries. */
        signum = PyLong_FromLong(sig);
        if (signum == NULL) {
            Py_DECREF(result);
            return NULL;
        }
        if (PySet_Add(result, signum) == -1) {
            Py_DECREF(signum);
            Py_DECREF(result);
            return NULL;
        }
        Py_DECREF(signum);
    }
    return result;
}
#endif

#ifdef PYPTHREAD_SIGMASK

/*[clinic input]
signal.pthread_sigmask

    how:  int
    mask: sigset_t
    /

Fetch and/or change the signal mask of the calling thread.
[clinic start generated code]*/

static PyObject *
signal_pthread_sigmask_impl(PyObject *module, int how, sigset_t mask)
/*[clinic end generated code: output=0562c0fb192981a8 input=85bcebda442fa77f]*/
{
    sigset_t previous;
    int err;

    err = pthread_sigmask(how, &mask, &previous);
    if (err != 0) {
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    /* if signals was unblocked, signal handlers have been called */
    if (PyErr_CheckSignals())
        return NULL;

    return sigset_to_set(previous);
}

#endif   /* #ifdef PYPTHREAD_SIGMASK */


#ifdef HAVE_SIGPENDING

/*[clinic input]
signal.sigpending

Examine pending signals.

Returns a set of signal numbers that are pending for delivery to
the calling thread.
[clinic start generated code]*/

static PyObject *
signal_sigpending_impl(PyObject *module)
/*[clinic end generated code: output=53375ffe89325022 input=e0036c016f874e29]*/
{
    int err;
    sigset_t mask;
    err = sigpending(&mask);
    if (err)
        return PyErr_SetFromErrno(PyExc_OSError);
    return sigset_to_set(mask);
}

#endif   /* #ifdef HAVE_SIGPENDING */


#ifdef HAVE_SIGWAIT

/*[clinic input]
signal.sigwait

    sigset: sigset_t
    /

Wait for a signal.

Suspend execution of the calling thread until the delivery of one of the
signals specified in the signal set sigset.  The function accepts the signal
and returns the signal number.
[clinic start generated code]*/

static PyObject *
signal_sigwait_impl(PyObject *module, sigset_t sigset)
/*[clinic end generated code: output=f43770699d682f96 input=a6fbd47b1086d119]*/
{
    int err, signum;

    Py_BEGIN_ALLOW_THREADS
    err = sigwait(&sigset, &signum);
    Py_END_ALLOW_THREADS
    if (err) {
        errno = err;
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    return PyLong_FromLong(signum);
}

#endif   /* #ifdef HAVE_SIGWAIT */


#if defined(HAVE_SIGFILLSET) || defined(MS_WINDOWS)

/*[clinic input]
signal.valid_signals

Return a set of valid signal numbers on this platform.

The signal numbers returned by this function can be safely passed to
functions like `pthread_sigmask`.
[clinic start generated code]*/

static PyObject *
signal_valid_signals_impl(PyObject *module)
/*[clinic end generated code: output=1609cffbcfcf1314 input=86a3717ff25288f2]*/
{
#ifdef MS_WINDOWS
#ifdef SIGBREAK
    PyObject *tup = Py_BuildValue("(iiiiiii)", SIGABRT, SIGBREAK, SIGFPE,
                                  SIGILL, SIGINT, SIGSEGV, SIGTERM);
#else
    PyObject *tup = Py_BuildValue("(iiiiii)", SIGABRT, SIGFPE, SIGILL,
                                  SIGINT, SIGSEGV, SIGTERM);
#endif
    if (tup == NULL) {
        return NULL;
    }
    PyObject *set = PySet_New(tup);
    Py_DECREF(tup);
    return set;
#else
    sigset_t mask;
    if (sigemptyset(&mask) || sigfillset(&mask)) {
        return PyErr_SetFromErrno(PyExc_OSError);
    }
    return sigset_to_set(mask);
#endif
}

#endif   /* #if defined(HAVE_SIGFILLSET) || defined(MS_WINDOWS) */


#if defined(HAVE_SIGWAITINFO) || defined(HAVE_SIGTIMEDWAIT)
static int initialized;
static PyStructSequence_Field struct_siginfo_fields[] = {
    {"si_signo",        "signal number"},
    {"si_code",         "signal code"},
    {"si_errno",        "errno associated with this signal"},
    {"si_pid",          "sending process ID"},
    {"si_uid",          "real user ID of sending process"},
    {"si_status",       "exit value or signal"},
    {"si_band",         "band event for SIGPOLL"},
    {0}
};

PyDoc_STRVAR(struct_siginfo__doc__,
"struct_siginfo: Result from sigwaitinfo or sigtimedwait.\n\n\
This object may be accessed either as a tuple of\n\
(si_signo, si_code, si_errno, si_pid, si_uid, si_status, si_band),\n\
or via the attributes si_signo, si_code, and so on.");

static PyStructSequence_Desc struct_siginfo_desc = {
    "signal.struct_siginfo",           /* name */
    struct_siginfo__doc__,       /* doc */
    struct_siginfo_fields,       /* fields */
    7          /* n_in_sequence */
};

static PyTypeObject SiginfoType;

static PyObject *
fill_siginfo(siginfo_t *si)
{
    PyObject *result = PyStructSequence_New(&SiginfoType);
    if (!result)
        return NULL;

    PyStructSequence_SET_ITEM(result, 0, PyLong_FromLong((long)(si->si_signo)));
    PyStructSequence_SET_ITEM(result, 1, PyLong_FromLong((long)(si->si_code)));
#ifdef __VXWORKS__
    PyStructSequence_SET_ITEM(result, 2, PyLong_FromLong(0L));
    PyStructSequence_SET_ITEM(result, 3, PyLong_FromLong(0L));
    PyStructSequence_SET_ITEM(result, 4, PyLong_FromLong(0L));
    PyStructSequence_SET_ITEM(result, 5, PyLong_FromLong(0L));
#else
    PyStructSequence_SET_ITEM(result, 2, PyLong_FromLong((long)(si->si_errno)));
    PyStructSequence_SET_ITEM(result, 3, PyLong_FromPid(si->si_pid));
    PyStructSequence_SET_ITEM(result, 4, _PyLong_FromUid(si->si_uid));
    PyStructSequence_SET_ITEM(result, 5,
                                PyLong_FromLong((long)(si->si_status)));
#endif
#ifdef HAVE_SIGINFO_T_SI_BAND
    PyStructSequence_SET_ITEM(result, 6, PyLong_FromLong(si->si_band));
#else
    PyStructSequence_SET_ITEM(result, 6, PyLong_FromLong(0L));
#endif
    if (PyErr_Occurred()) {
        Py_DECREF(result);
        return NULL;
    }

    return result;
}
#endif

#ifdef HAVE_SIGWAITINFO

/*[clinic input]
signal.sigwaitinfo

    sigset: sigset_t
    /

Wait synchronously until one of the signals in *sigset* is delivered.

Returns a struct_siginfo containing information about the signal.
[clinic start generated code]*/

static PyObject *
signal_sigwaitinfo_impl(PyObject *module, sigset_t sigset)
/*[clinic end generated code: output=1eb2f1fa236fdbca input=3d1a7e1f27fc664c]*/
{
    siginfo_t si;
    int err;
    int async_err = 0;

    do {
        Py_BEGIN_ALLOW_THREADS
        err = sigwaitinfo(&sigset, &si);
        Py_END_ALLOW_THREADS
    } while (err == -1
             && errno == EINTR && !(async_err = PyErr_CheckSignals()));
    if (err == -1)
        return (!async_err) ? PyErr_SetFromErrno(PyExc_OSError) : NULL;

    return fill_siginfo(&si);
}

#endif   /* #ifdef HAVE_SIGWAITINFO */

#ifdef HAVE_SIGTIMEDWAIT

/*[clinic input]
signal.sigtimedwait

    sigset: sigset_t
    timeout as timeout_obj: object
    /

Like sigwaitinfo(), but with a timeout.

The timeout is specified in seconds, with floating point numbers allowed.
[clinic start generated code]*/

static PyObject *
signal_sigtimedwait_impl(PyObject *module, sigset_t sigset,
                         PyObject *timeout_obj)
/*[clinic end generated code: output=59c8971e8ae18a64 input=87fd39237cf0b7ba]*/
{
    struct timespec ts;
    siginfo_t si;
    int res;
    _PyTime_t timeout, deadline, monotonic;

    if (_PyTime_FromSecondsObject(&timeout,
                                  timeout_obj, _PyTime_ROUND_CEILING) < 0)
        return NULL;

    if (timeout < 0) {
        PyErr_SetString(PyExc_ValueError, "timeout must be non-negative");
        return NULL;
    }

    deadline = _PyTime_GetMonotonicClock() + timeout;

    do {
        if (_PyTime_AsTimespec(timeout, &ts) < 0)
            return NULL;

        Py_BEGIN_ALLOW_THREADS
        res = sigtimedwait(&sigset, &si, &ts);
        Py_END_ALLOW_THREADS

        if (res != -1)
            break;

        if (errno != EINTR) {
            if (errno == EAGAIN)
                Py_RETURN_NONE;
            else
                return PyErr_SetFromErrno(PyExc_OSError);
        }

        /* sigtimedwait() was interrupted by a signal (EINTR) */
        if (PyErr_CheckSignals())
            return NULL;

        monotonic = _PyTime_GetMonotonicClock();
        timeout = deadline - monotonic;
        if (timeout < 0)
            break;
    } while (1);

    return fill_siginfo(&si);
}

#endif   /* #ifdef HAVE_SIGTIMEDWAIT */


#if defined(HAVE_PTHREAD_KILL)

/*[clinic input]
signal.pthread_kill

    thread_id:  unsigned_long(bitwise=True)
    signalnum:  int
    /

Send a signal to a thread.
[clinic start generated code]*/

static PyObject *
signal_pthread_kill_impl(PyObject *module, unsigned long thread_id,
                         int signalnum)
/*[clinic end generated code: output=7629919b791bc27f input=1d901f2c7bb544ff]*/
{
    int err;

    if (PySys_Audit("signal.pthread_kill", "ki", thread_id, signalnum) < 0) {
        return NULL;
    }

    err = pthread_kill((pthread_t)thread_id, signalnum);
    if (err != 0) {
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    /* the signal may have been send to the current thread */
    if (PyErr_CheckSignals())
        return NULL;

    Py_RETURN_NONE;
}

#endif   /* #if defined(HAVE_PTHREAD_KILL) */


#if defined(__linux__) && defined(__NR_pidfd_send_signal)
/*[clinic input]
signal.pidfd_send_signal

    pidfd: int
    signalnum: int
    siginfo: object = None
    flags: int = 0
    /

Send a signal to a process referred to by a pid file descriptor.
[clinic start generated code]*/

static PyObject *
signal_pidfd_send_signal_impl(PyObject *module, int pidfd, int signalnum,
                              PyObject *siginfo, int flags)
/*[clinic end generated code: output=2d59f04a75d9cbdf input=2a6543a1f4ac2000]*/

{
    if (siginfo != Py_None) {
        PyErr_SetString(PyExc_TypeError, "siginfo must be None");
        return NULL;
    }
    if (syscall(__NR_pidfd_send_signal, pidfd, signalnum, NULL, flags) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_NONE;
}
#endif



/* List of functions defined in the module -- some of the methoddefs are
   defined to nothing if the corresponding C function is not available. */
static PyMethodDef signal_methods[] = {
    {"default_int_handler", signal_default_int_handler, METH_VARARGS, default_int_handler_doc},
    SIGNAL_ALARM_METHODDEF
    SIGNAL_SETITIMER_METHODDEF
    SIGNAL_GETITIMER_METHODDEF
    SIGNAL_SIGNAL_METHODDEF
    SIGNAL_RAISE_SIGNAL_METHODDEF
    SIGNAL_STRSIGNAL_METHODDEF
    SIGNAL_GETSIGNAL_METHODDEF
    {"set_wakeup_fd", (PyCFunction)(void(*)(void))signal_set_wakeup_fd, METH_VARARGS | METH_KEYWORDS, set_wakeup_fd_doc},
    SIGNAL_SIGINTERRUPT_METHODDEF
    SIGNAL_PAUSE_METHODDEF
    SIGNAL_PIDFD_SEND_SIGNAL_METHODDEF
    SIGNAL_PTHREAD_KILL_METHODDEF
    SIGNAL_PTHREAD_SIGMASK_METHODDEF
    SIGNAL_SIGPENDING_METHODDEF
    SIGNAL_SIGWAIT_METHODDEF
    SIGNAL_SIGWAITINFO_METHODDEF
    SIGNAL_SIGTIMEDWAIT_METHODDEF
#if defined(HAVE_SIGFILLSET) || defined(MS_WINDOWS)
    SIGNAL_VALID_SIGNALS_METHODDEF
#endif
    {NULL, NULL}           /* sentinel */
};


PyDoc_STRVAR(module_doc,
"This module provides mechanisms to use signal handlers in Python.\n\
\n\
Functions:\n\
\n\
alarm() -- cause SIGALRM after a specified time [Unix only]\n\
setitimer() -- cause a signal (described below) after a specified\n\
               float time and the timer may restart then [Unix only]\n\
getitimer() -- get current value of timer [Unix only]\n\
signal() -- set the action for a given signal\n\
getsignal() -- get the signal action for a given signal\n\
pause() -- wait until a signal arrives [Unix only]\n\
default_int_handler() -- default SIGINT handler\n\
\n\
signal constants:\n\
SIG_DFL -- used to refer to the system default handler\n\
SIG_IGN -- used to ignore the signal\n\
NSIG -- number of defined signals\n\
SIGINT, SIGTERM, etc. -- signal numbers\n\
\n\
itimer constants:\n\
ITIMER_REAL -- decrements in real time, and delivers SIGALRM upon\n\
               expiration\n\
ITIMER_VIRTUAL -- decrements only when the process is executing,\n\
               and delivers SIGVTALRM upon expiration\n\
ITIMER_PROF -- decrements both when the process is executing and\n\
               when the system is executing on behalf of the process.\n\
               Coupled with ITIMER_VIRTUAL, this timer is usually\n\
               used to profile the time spent by the application\n\
               in user and kernel space. SIGPROF is delivered upon\n\
               expiration.\n\
\n\n\
*** IMPORTANT NOTICE ***\n\
A signal handler function is called with two arguments:\n\
the first is the signal number, the second is the interrupted stack frame.");

static struct PyModuleDef signalmodule = {
    PyModuleDef_HEAD_INIT,
    "_signal",
    module_doc,
    -1,
    signal_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit__signal(void)
{
    PyObject *m, *d;
    int i;

    /* Create the module and add the functions */
    m = PyModule_Create(&signalmodule);
    if (m == NULL)
        return NULL;

#if defined(HAVE_SIGWAITINFO) || defined(HAVE_SIGTIMEDWAIT)
    if (!initialized) {
        if (PyStructSequence_InitType2(&SiginfoType, &struct_siginfo_desc) < 0)
            return NULL;
    }
    Py_INCREF((PyObject*) &SiginfoType);
    PyModule_AddObject(m, "struct_siginfo", (PyObject*) &SiginfoType);
    initialized = 1;
#endif

    /* Add some symbolic constants to the module */
    d = PyModule_GetDict(m);

    DefaultHandler = PyLong_FromVoidPtr((void *)SIG_DFL);
    if (!DefaultHandler ||
        PyDict_SetItemString(d, "SIG_DFL", DefaultHandler) < 0) {
        goto finally;
    }

    IgnoreHandler = PyLong_FromVoidPtr((void *)SIG_IGN);
    if (!IgnoreHandler ||
        PyDict_SetItemString(d, "SIG_IGN", IgnoreHandler) < 0) {
        goto finally;
    }

    if (PyModule_AddIntMacro(m, NSIG))
        goto finally;

#ifdef SIG_BLOCK
    if (PyModule_AddIntMacro(m, SIG_BLOCK))
         goto finally;
#endif
#ifdef SIG_UNBLOCK
    if (PyModule_AddIntMacro(m, SIG_UNBLOCK))
         goto finally;
#endif
#ifdef SIG_SETMASK
    if (PyModule_AddIntMacro(m, SIG_SETMASK))
         goto finally;
#endif

    IntHandler = PyDict_GetItemString(d, "default_int_handler");
    if (!IntHandler)
        goto finally;
    Py_INCREF(IntHandler);

    _Py_atomic_store_relaxed(&Handlers[0].tripped, 0);
    for (i = 1; i < NSIG; i++) {
        void (*t)(int);
        t = PyOS_getsig(i);
        _Py_atomic_store_relaxed(&Handlers[i].tripped, 0);
        if (t == SIG_DFL)
            Handlers[i].func = DefaultHandler;
        else if (t == SIG_IGN)
            Handlers[i].func = IgnoreHandler;
        else
            Handlers[i].func = Py_None; /* None of our business */
        Py_INCREF(Handlers[i].func);
    }
    if (Handlers[SIGINT].func == DefaultHandler) {
        /* Install default int handler */
        Py_INCREF(IntHandler);
        Py_SETREF(Handlers[SIGINT].func, IntHandler);
        PyOS_setsig(SIGINT, signal_handler);
    }

#ifdef SIGHUP
    if (PyModule_AddIntMacro(m, SIGHUP))
         goto finally;
#endif
#ifdef SIGINT
    if (PyModule_AddIntMacro(m, SIGINT))
         goto finally;
#endif
#ifdef SIGBREAK
    if (PyModule_AddIntMacro(m, SIGBREAK))
         goto finally;
#endif
#ifdef SIGQUIT
    if (PyModule_AddIntMacro(m, SIGQUIT))
         goto finally;
#endif
#ifdef SIGILL
    if (PyModule_AddIntMacro(m, SIGILL))
         goto finally;
#endif
#ifdef SIGTRAP
    if (PyModule_AddIntMacro(m, SIGTRAP))
         goto finally;
#endif
#ifdef SIGIOT
    if (PyModule_AddIntMacro(m, SIGIOT))
         goto finally;
#endif
#ifdef SIGABRT
    if (PyModule_AddIntMacro(m, SIGABRT))
         goto finally;
#endif
#ifdef SIGEMT
    if (PyModule_AddIntMacro(m, SIGEMT))
         goto finally;
#endif
#ifdef SIGFPE
    if (PyModule_AddIntMacro(m, SIGFPE))
         goto finally;
#endif
#ifdef SIGKILL
    if (PyModule_AddIntMacro(m, SIGKILL))
         goto finally;
#endif
#ifdef SIGBUS
    if (PyModule_AddIntMacro(m, SIGBUS))
         goto finally;
#endif
#ifdef SIGSEGV
    if (PyModule_AddIntMacro(m, SIGSEGV))
         goto finally;
#endif
#ifdef SIGSYS
    if (PyModule_AddIntMacro(m, SIGSYS))
         goto finally;
#endif
#ifdef SIGPIPE
    if (PyModule_AddIntMacro(m, SIGPIPE))
         goto finally;
#endif
#ifdef SIGALRM
    if (PyModule_AddIntMacro(m, SIGALRM))
         goto finally;
#endif
#ifdef SIGTERM
    if (PyModule_AddIntMacro(m, SIGTERM))
         goto finally;
#endif
#ifdef SIGUSR1
    if (PyModule_AddIntMacro(m, SIGUSR1))
         goto finally;
#endif
#ifdef SIGUSR2
    if (PyModule_AddIntMacro(m, SIGUSR2))
         goto finally;
#endif
#ifdef SIGCLD
    if (PyModule_AddIntMacro(m, SIGCLD))
         goto finally;
#endif
#ifdef SIGCHLD
    if (PyModule_AddIntMacro(m, SIGCHLD))
         goto finally;
#endif
#ifdef SIGPWR
    if (PyModule_AddIntMacro(m, SIGPWR))
         goto finally;
#endif
#ifdef SIGIO
    if (PyModule_AddIntMacro(m, SIGIO))
         goto finally;
#endif
#ifdef SIGURG
    if (PyModule_AddIntMacro(m, SIGURG))
         goto finally;
#endif
#ifdef SIGWINCH
    if (PyModule_AddIntMacro(m, SIGWINCH))
         goto finally;
#endif
#ifdef SIGPOLL
    if (PyModule_AddIntMacro(m, SIGPOLL))
         goto finally;
#endif
#ifdef SIGSTOP
    if (PyModule_AddIntMacro(m, SIGSTOP))
         goto finally;
#endif
#ifdef SIGTSTP
    if (PyModule_AddIntMacro(m, SIGTSTP))
         goto finally;
#endif
#ifdef SIGCONT
    if (PyModule_AddIntMacro(m, SIGCONT))
         goto finally;
#endif
#ifdef SIGTTIN
    if (PyModule_AddIntMacro(m, SIGTTIN))
         goto finally;
#endif
#ifdef SIGTTOU
    if (PyModule_AddIntMacro(m, SIGTTOU))
         goto finally;
#endif
#ifdef SIGVTALRM
    if (PyModule_AddIntMacro(m, SIGVTALRM))
         goto finally;
#endif
#ifdef SIGPROF
    if (PyModule_AddIntMacro(m, SIGPROF))
         goto finally;
#endif
#ifdef SIGXCPU
    if (PyModule_AddIntMacro(m, SIGXCPU))
         goto finally;
#endif
#ifdef SIGXFSZ
    if (PyModule_AddIntMacro(m, SIGXFSZ))
         goto finally;
#endif
#ifdef SIGRTMIN
    if (PyModule_AddIntMacro(m, SIGRTMIN))
         goto finally;
#endif
#ifdef SIGRTMAX
    if (PyModule_AddIntMacro(m, SIGRTMAX))
         goto finally;
#endif
#ifdef SIGINFO
    if (PyModule_AddIntMacro(m, SIGINFO))
         goto finally;
#endif

#ifdef ITIMER_REAL
    if (PyModule_AddIntMacro(m, ITIMER_REAL))
         goto finally;
#endif
#ifdef ITIMER_VIRTUAL
    if (PyModule_AddIntMacro(m, ITIMER_VIRTUAL))
         goto finally;
#endif
#ifdef ITIMER_PROF
    if (PyModule_AddIntMacro(m, ITIMER_PROF))
         goto finally;
#endif

#if defined (HAVE_SETITIMER) || defined (HAVE_GETITIMER)
    ItimerError = PyErr_NewException("signal.ItimerError",
            PyExc_OSError, NULL);
    if (!ItimerError ||
        PyDict_SetItemString(d, "ItimerError", ItimerError) < 0) {
        goto finally;
    }
#endif

#ifdef CTRL_C_EVENT
    if (PyModule_AddIntMacro(m, CTRL_C_EVENT))
         goto finally;
#endif

#ifdef CTRL_BREAK_EVENT
    if (PyModule_AddIntMacro(m, CTRL_BREAK_EVENT))
         goto finally;
#endif

#ifdef MS_WINDOWS
    /* Create manual-reset event, initially unset */
    sigint_event = CreateEvent(NULL, TRUE, FALSE, FALSE);
#endif

    if (PyErr_Occurred()) {
        Py_DECREF(m);
        m = NULL;
    }

  finally:
    return m;
}

static void
finisignal(void)
{
    int i;
    PyObject *func;

    for (i = 1; i < NSIG; i++) {
        func = Handlers[i].func;
        _Py_atomic_store_relaxed(&Handlers[i].tripped, 0);
        Handlers[i].func = NULL;
        if (func != NULL && func != Py_None &&
            func != DefaultHandler && func != IgnoreHandler)
            PyOS_setsig(i, SIG_DFL);
        Py_XDECREF(func);
    }

    Py_CLEAR(IntHandler);
    Py_CLEAR(DefaultHandler);
    Py_CLEAR(IgnoreHandler);
#ifdef HAVE_GETITIMER
    Py_CLEAR(ItimerError);
#endif
}


/* Declared in pyerrors.h */
int
PyErr_CheckSignals(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (!_Py_ThreadCanHandleSignals(tstate->interp)) {
        return 0;
    }

    return _PyErr_CheckSignalsTstate(tstate);
}


/* Declared in cpython/pyerrors.h */
int
_PyErr_CheckSignalsTstate(PyThreadState *tstate)
{
    if (!_Py_atomic_load(&is_tripped)) {
        return 0;
    }

    /*
     * The is_tripped variable is meant to speed up the calls to
     * PyErr_CheckSignals (both directly or via pending calls) when no
     * signal has arrived. This variable is set to 1 when a signal arrives
     * and it is set to 0 here, when we know some signals arrived. This way
     * we can run the registered handlers with no signals blocked.
     *
     * NOTE: with this approach we can have a situation where is_tripped is
     *       1 but we have no more signals to handle (Handlers[i].tripped
     *       is 0 for every signal i). This won't do us any harm (except
     *       we're gonna spent some cycles for nothing). This happens when
     *       we receive a signal i after we zero is_tripped and before we
     *       check Handlers[i].tripped.
     */
    _Py_atomic_store(&is_tripped, 0);

    PyObject *frame = (PyObject *)tstate->frame;
    if (!frame) {
        frame = Py_None;
    }

    for (int i = 1; i < NSIG; i++) {
        if (!_Py_atomic_load_relaxed(&Handlers[i].tripped)) {
            continue;
        }
        _Py_atomic_store_relaxed(&Handlers[i].tripped, 0);

        PyObject *arglist = Py_BuildValue("(iO)", i, frame);
        PyObject *result;
        if (arglist) {
            result = _PyObject_Call(tstate, Handlers[i].func, arglist, NULL);
            Py_DECREF(arglist);
        }
        else {
            result = NULL;
        }
        if (!result) {
            /* On error, re-schedule a call to _PyErr_CheckSignalsTstate() */
            _Py_atomic_store(&is_tripped, 1);
            return -1;
        }

        Py_DECREF(result);
    }

    return 0;
}



int
_PyErr_CheckSignals(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_CheckSignalsTstate(tstate);
}


/* Simulate the effect of a signal.SIGINT signal arriving. The next time
   PyErr_CheckSignals is called,  the Python SIGINT signal handler will be
   raised.

   Missing signal handler for the SIGINT signal is silently ignored. */
void
PyErr_SetInterrupt(void)
{
    if ((Handlers[SIGINT].func != IgnoreHandler) &&
        (Handlers[SIGINT].func != DefaultHandler)) {
        trip_signal(SIGINT);
    }
}

void
PyOS_InitInterrupts(void)
{
    PyObject *m = PyImport_ImportModule("_signal");
    if (m) {
        Py_DECREF(m);
    }
}

void
PyOS_FiniInterrupts(void)
{
    finisignal();
}


// The caller doesn't have to hold the GIL
int
_PyOS_InterruptOccurred(PyThreadState *tstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    if (!_Py_ThreadCanHandleSignals(tstate->interp)) {
        return 0;
    }

    if (!_Py_atomic_load_relaxed(&Handlers[SIGINT].tripped)) {
        return 0;
    }

    _Py_atomic_store_relaxed(&Handlers[SIGINT].tripped, 0);
    return 1;
}


// The caller must to hold the GIL
int
PyOS_InterruptOccurred(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyOS_InterruptOccurred(tstate);
}


#ifdef HAVE_FORK
static void
_clear_pending_signals(void)
{
    if (!_Py_atomic_load(&is_tripped)) {
        return;
    }

    _Py_atomic_store(&is_tripped, 0);
    for (int i = 1; i < NSIG; ++i) {
        _Py_atomic_store_relaxed(&Handlers[i].tripped, 0);
    }
}

void
_PySignal_AfterFork(void)
{
    /* Clear the signal flags after forking so that they aren't handled
     * in both processes if they came in just before the fork() but before
     * the interpreter had an opportunity to call the handlers.  issue9535. */
    _clear_pending_signals();
}
#endif   /* HAVE_FORK */


int
_PyOS_IsMainThread(void)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    return _Py_ThreadCanHandleSignals(interp);
}

#ifdef MS_WINDOWS
void *_PyOS_SigintEvent(void)
{
    /* Returns a manual-reset event which gets tripped whenever
       SIGINT is received.

       Python.h does not include windows.h so we do cannot use HANDLE
       as the return type of this function.  We use void* instead. */
    return sigint_event;
}
#endif
