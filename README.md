# MutexIPC
Using a mutex (and only a mutex) for IPC between processes. Technically they are shared memory!

# Why
I have no answer for this other than mutex goes brrrrr

# No really
Mutexes are exposed by the Object Manager in Windows. They exist in the Global namespace, along with Memory Maps, the clipboard, atom tables and the like.

That basically means they can be used to communicate between processes, but no one seems to have ever used them in this fashion for some reason. I decided to fix this, and mainly thought it would be hilarious.

# Example
This program just demonstrates usage with strings using argv, but it can be used to send arbitrary data.
Here's what it looks like:
![Gif of program being used for IPC](https://i.imgur.com/eVRyLwL.gif)
