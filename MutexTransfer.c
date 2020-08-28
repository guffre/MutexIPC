#include <windows.h>
#include <stdio.h>

#define TRANSMISSION_SPEED 50
#define MUTEXNAME "ThisIsReal"

// global for debug output
BOOL verbose = FALSE;

// function defs
char *MutexRecv(LPCSTR, DWORD);
VOID MutexSend(LPCSTR, char*, DWORD, DWORD);
HANDLE SyncSend(LPCSTR, DWORD);
HANDLE SyncRecv(LPCSTR, DWORD);
void itob(char);


int main(int argc, char **argv) {
    // Default to zero arguments means receive-mode
    if (argc <= 1) {
        char *data = MutexRecv(MUTEXNAME, TRANSMISSION_SPEED);
        printf("Received: %s\n", data);
        free(data);
    }
    else {
        char *to_send = argv[1];
        int length = strlen(argv[1]);
        // Extra arguments are just treated as a flag to turn verbose mode on
        if (argc > 2) {
            verbose = TRUE;
        }
        MutexSend(MUTEXNAME, to_send, length, TRANSMISSION_SPEED);
    }
}

VOID MutexSend(LPCSTR lpcName, char *data, DWORD len, DWORD transmission_speed) {
    HANDLE hMutex = NULL;
    BOOL locked = FALSE;
    // First bit is always sent as a 1, so just resend the first byte and drop it on the receiver side
    BOOL skip_first_byte = TRUE;

    // Synchronize between the sender and receiver
    hMutex = SyncSend(lpcName, transmission_speed);
    printf("[+] Synchronized.\n");

    // Loop through sending all the bits in the data. Null data is fine
    for (DWORD index = 0; index < len; index++) {
        // char to binary
        char c = data[index];
        // This is where the lock gets flipped if the binary is a 0 or a 1.
        // ReleaseMutex for a "1", Lock for a "0"
        for (int i = 7; i >= 0; i--) {
            if (c & (1 << i)) {
                if (locked) {
                    ReleaseMutex(hMutex);
                    locked = FALSE;
                }
            }
            else {
                if (!locked) {
                    WaitForSingleObject(hMutex, 0);
                    locked = TRUE;
                }
            }
            Sleep(transmission_speed);
        }
        
        if (skip_first_byte) {
            index = -1;
            skip_first_byte = FALSE;
        }
        if (verbose) {
            itob(c);
            printf("\t[%c]\n", c);
        }
    }
    // Make sure to lock the mutex before exiting. This is how the receiver knows transmission is over
    WaitForSingleObject(hMutex, transmission_speed);
}

char *MutexRecv(LPCSTR lpcName, DWORD transmission_speed) {
    DWORD result;
    // The first bit and byte is always "extra"
    BOOL skip_first_bit  = TRUE;
    BOOL skip_first_byte = TRUE;
    HANDLE hMutex = NULL;
    int bit_index = 7;
    unsigned int byt_index = 0;
    unsigned int len = 1024;
    char *buf = calloc(len, 1);

    // Synchronize between the sender and receiver
    hMutex = SyncRecv(lpcName, transmission_speed);
    printf("[+] Synchronized.\n");

    // Loop forever to pick up data. The break condition happens when the sender is finished sending data
    while (TRUE) {
        // First get the actual bit by checking state of the mutex
        DWORD result = WaitForSingleObject(hMutex, 0);
        if (result == WAIT_ABANDONED)
            break;
        char bit = (result == 0);
        if (bit)
            ReleaseMutex(hMutex);
        
        // Make sure the buffer can hold all the data.
        if (byt_index >= len) {
            len *= 2;
            (char *)buf  = (char *)realloc(buf, len);
        }
        
        // Shove the bit into the current byte
        buf[byt_index] = (buf[byt_index] | (bit << bit_index));

        // Skip the first bit, its always wrong
        if (skip_first_bit) {
            skip_first_bit = FALSE;
        }
        else {
            // bit index counts down to -1
            bit_index = bit_index - 1;
            if (bit_index < 0) {
                if (verbose) {
                    itob(buf[byt_index]);
                    printf("\t[%c]\n", buf[byt_index]);
                }
                // Reset bit index to first bit
                bit_index = 7;
                if (skip_first_byte) {
                    buf[byt_index] = '\0';
                    skip_first_byte = FALSE;
                }
                else {
                    byt_index += 1;
                }
            }
        }
        Sleep(transmission_speed);
    }
    if (verbose) {
        for(int i = 0; i < byt_index; i++) {
            itob(buf[i]);
            printf(" [%c]\n", buf[i]);
        }
    }
    return buf;
}

/*
    Handles synchronization of the sender
    Creates the mutex and waits for the sender to consequently lock it
*/
HANDLE SyncSend(LPCSTR lpcName, DWORD t) {
    // Create the mutex and then wait for it to become locked
    // Locked mutex indicates the receiver is ready to go
    HANDLE hMutex = CreateMutexA(NULL, TRUE, lpcName);
    DWORD result = 0;
    do {
        Sleep(10);
        ReleaseMutex(hMutex);
        result = WaitForSingleObject(hMutex, 1);
    } while (result == 0);

    // The receiver should be within ~30ms of this time
    DWORD current = GetTickCount();
    DWORD millisecondsTillThousand = 1000 - (current % 1000);
    DWORD startup = current + millisecondsTillThousand + 1000;
    while (GetTickCount() < startup) {
        Sleep(1);
    }
    return hMutex;
}

/*
    Handles synchronization of the receiver
    Waits until the mutex exists (sender is responsible for creation)

    Explanation of math:
    After getting a handle, waits until ("transmission speed" / 2) before the next (millisecond + 1000)
    Example: sync happens at GetTickCount == 348324
    The next millisecond is 349000, +1000 is 350000
    startup time will subtract (transmission time / 2) from that calculated millisecond
    This is so there is a perfect "one-half" overlap between mutex probes (before drift)
*/
HANDLE SyncRecv(LPCSTR lpcName, DWORD t) {
    HANDLE hMutex = NULL;
    DWORD result;

    // The sender creates the mutex. Escaping this loop means the sender is ready
    while (hMutex == NULL) {
        hMutex = OpenMutexA(SYNCHRONIZE, FALSE, lpcName);
        Sleep(100);
    }

    // Lock the mutex, indicating to the sender that we are ready to receive
    // SyncSend has a 10ms sleep. This should be plenty of time but is not guaranteed
    result = WaitForSingleObject(hMutex, INFINITE);
    Sleep(30); 
    ReleaseMutex(hMutex);

    // Sender should be very close to our current time (within our ~30ms sleep)
    DWORD current = GetTickCount();
    DWORD millisecondsTillThousand = 1000 - (current % 1000);
    DWORD startup = (current + millisecondsTillThousand + 1000) - (t/2);
    while (GetTickCount() < startup) {
        Sleep(1);
    }
    return hMutex;
}

void itob(char c) { 
    unsigned i; 
    for (i = 1 << 7; i > 0; i = i / 2) {
        char result = (c & i) ? '1': '0';
        printf("%c", result); 
    }
}
