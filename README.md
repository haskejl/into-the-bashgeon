# Into the Bashgeon
The code contained here is the start of a game for learning common command line utilities.

## Kernel Requirements
When you run `cat /proc/sys/kernel/unprivileged_userns_clone`, the result must be 1. If it's not, your kernel is configured so that the game can't create an isolated environment and won't run.

***Warning***: While I implemented namespace separation to separate out most of the filesystem and make the parts actually on your computer read only, I'm still an amateur. So, keep that in mind when executing commands. Currently /dev is known to be read/write even though it shouldn't be in the long-run.
