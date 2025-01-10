# Grading Report

Preliminary Grade: 66/100 (Late)

Assignment Grade: 49.50/100

## Submission and Compilation

Good clean compilation.

This section: 20/20

## Correctness Tests

### Checking for major errors (seg faults, invalid memory access, leaks)

Serious Valgrind error (most likely bad ptr) - Results (truncated to 12 lines):

```
==195862== Use of uninitialised value of size 8
==195862==    at 0x10A24E: add_player (player.c:42)
==195862==    by 0x10A1D4: player_init (player.c:28)
==195862==    by 0x1097BD: handle_conn (arena.c:72)
==195862==    by 0x109A01: main (arena.c:125)
==195862==
==195858== 4,096 bytes in 1 blocks are still reachable in loss record 1 of 1
==195858==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==195858==    by 0x48FAC23: _IO_file_doallocate (filedoalloc.c:101)
==195858==    by 0x4909D5F: _IO_doallocbuf (genops.c:347)
==195858==    by 0x4908FDF: _IO_file_overflow@@GLIBC_2.2.5 (fileops.c:744)
==195858==    by 0x4907754: _IO_new_file_xsputn (fileops.c:1243)
```

You are using `fork` to create a new process, when you should be using
`pthread_create` to create a thread. New processes are very inefficient,
and don't share the same memory space. You're not going to be able to
do things that rely on shared data structures, like getting a list of
users.

This section: 7/25

### Basic network communication test

Communicating properly

This section: 15/15

### Basic single-player, two logins test

Good

This section: 5/5

### Two players logging in under different names

Good

This section: 5/5

### Attempt to login with a taken name

Should have gotten an error on the second login - Results:

```
OK
```

This section: 0/10

## Code Quality (Style, Robustness, and Design)

The `setvbuf` call should not be in the while loop. Do it once after the FILE
object is created.

You should always check the return value of standard functions (like
dup and fdopen) to make sure they didn't fail for some reason.

Lots of problems with the way you tried to make a player list. This design
just isn't going to work.

This section: 14/20

