/*
 * lab8 grading fragment. tools/inject.py splices it into dev_stdin_read() in
 * kern/fs/devs/dev_stdin.c at grading time. The fragment copies the commands of lab8_script
 * (hello, exit, forktest, yield) into the caller's buffer as console input, as many bytes as fit
 * in it. When the script has been handed out, the fragment falls through and the real console is
 * read as usual.
 */
    {
        static const char lab8_script[] =
            "hello\n"
            "exit\n"
            "forktest\n"
            "yield\n";
        static size_t lab8_pos = 0;
        size_t lab8_n = 0;

        while (lab8_n < len && lab8_pos < sizeof(lab8_script) - 1) {
            buf[lab8_n ++] = lab8_script[lab8_pos ++];
        }
        if (lab8_n != 0) {
            return lab8_n;
        }
        /* the script has been handed out: continue with the real console */
    }
