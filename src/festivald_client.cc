/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1996,1997                         */
/*           Sergio Oller Moreno, Barcelona, Spain (c) 2018              */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/* Author : Sergio Oller, inspired on the festival client implementation */
/*          by Alan W Black.                                             */
/*                                                                       */
/* Client program used to send comands/data to a festival server         */
/*                                                                       */
/*=======================================================================*/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <EST_Option.h>
#include <EST_String.h>
#include <EST_Wave.h>
#include <EST_cmd_line.h> /* parse_cmd_line */
#include <EST_cutils.h>   /* streq */
#include <EST_io_aux.h>   /* make_tmp_filename() */

using namespace std;

typedef FILE* SERVER_FD;

static void copy_to_server(FILE* fdin, SERVER_FD serverfd);
static void ttw_file(SERVER_FD serverfd, const EST_String& file);
static void client_accept_waveform(SERVER_FD fd);
static void client_accept_s_expr(SERVER_FD fd);
static void new_state(int c, int& state, int& bdepth);

static EST_String output_filename = "-";
static EST_String output_type = "riff";
static EST_String tts_mode = "nil";
static EST_String prolog = "";
static int withlisp = FALSE;
static EST_String aucommand = "";
static int async_mode = FALSE;

#define DEFAULT_SOCKET_PATH "festivald.socket"

int festival_socket_client(const char* socket_path, int* f_socket) {
    if (socket_path == NULL) {
        std::cerr << "Path to socket missing" << std::endl;
        return -1;
    }

    struct sockaddr_un sa;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket(): " << strerror(errno) << std::endl;
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path));
    errno = 0;
    if (connect(fd, (sockaddr*)&sa, sizeof(sa)) != 0) {
        std::cerr << "socket: connect to " << sa.sun_path
                  << " failed: " << strerror(errno) << std::endl;
        return -1;
    }
    *f_socket = fd;
    return 0;
}

int main(int argc, char** argv) {
    EST_Option al;
    EST_StrList files;
    const char* socket_path;
    EST_String server;
    FILE* infd;

    parse_command_line(
        argc, argv,
        EST_String("Usage:\n") +
            "festivald_client <options> <file0> <file1> ...\n" +
            "Access to festivald server process\n" +
            "--socket <string>   path to festivald file socket\n" +
            "--output <string>   file to save output waveform to\n" +
            "--otype <string> {riff}\n" +
            "                    output type for waveform\n" +
            "--prolog <string>   filename containing commands to be sent\n" +
            "                    to the server before standard commands\n" +
            "                    (useful when using --ttw)\n" +
            "--async             Asynchronous mode, server may send back\n" +
            "                    multiple waveforms per text file\n" +
            "--ttw               Text to waveform: take text from first\n" +
            "                    arg or stdin get server to return\n" +
            "                    waveform(s) stored in output or operated\n" +
            "                    on by aucommand.\n" +
            "--withlisp          Output lisp replies from server.\n" +
            "--tts_mode <string> TTS mode for file (default is "
            "fundamental).\n" +
            "--aucommand <string>\n" +
            "                    command to be applied to each\n" +
            "                    waveform retruned from server.  Use $FILE\n" +
            "                    in string to refer to waveform file\n",
        files, al);

    if (al.present("--socket"))
        socket_path = al.val("--socket");
    else
        socket_path = DEFAULT_SOCKET_PATH;

    if (al.present("--tts_mode"))
        tts_mode = al.val("--tts_mode");

    if (al.present("--output"))
        output_filename = al.val("--output");
    if (al.present("--otype")) {
        output_type = al.val("--otype");
        if (!output_type.matches(RXalphanum)) {
            cerr << "festivald_client: invalid output type \"" << output_type
                 << "\"" << endl;
            return 1;
        }
    }

    // Specify what to do with received waveform
    if (al.present("--aucommand"))
        aucommand = al.val("--aucommand");

    if (al.present("--withlisp"))
        withlisp = TRUE;

    if (al.present("--async"))
        async_mode = TRUE;
    else
        async_mode = FALSE;

    // Connect to socket:
    int fd = -1;
    if (festival_socket_client(socket_path, &fd) < 0) {
        return -1;
    }
    // Open socket file descriptor
    FILE* serverfd = fdopen(fd, "wb");

    if (al.present("--prolog")) {
        FILE* pfd = fopen(al.val("--prolog"), "rb");
        if (pfd == NULL) {
            cerr << "festivald_client: can't open prolog file \""
                 << al.val("--prolog") << "\"" << endl;
            return 1;
        }
        copy_to_server(pfd, serverfd);
        fclose(pfd);
    }

    if (al.present("--ttw"))
        ttw_file(serverfd, files.nth(0));
    else {
        if ((files.length() == 0) || (files.nth(0) == "-"))
            copy_to_server(stdin, serverfd);
        else {
            if ((infd = fopen(files.nth(0), "rb")) == NULL) {
                cerr << "festivald_client: can't open \"" << files.nth(0)
                     << "\"\n";
                return 1;
            }
            copy_to_server(infd, serverfd);
        }
    }

    return 0;
}

static void ttw_file(SERVER_FD serverfd, const EST_String& file) {
    // text to waveform file.  This includes the tts wraparounds for
    // the text in file and outputs a waveform in output_filename
    // This is done as *one* waveform.  This is designed for short
    // dialog type examples.  If you need spooling this isn't the
    // way to do it
    EST_String tmpfile = make_tmp_filename();
    FILE *fd, *tfd;
    int c;

    if ((fd = fopen(tmpfile, "wb")) == NULL) {
        cerr << "festivald_client: can't open tmpfile \"" << tmpfile << "\"\n";
        exit(-1);
    }
    // Here we ask for NIST because its a byte order aware headered format
    // the eventual desired format might be unheadered and if we asked the
    // the server for that we wouldn't know if it required byte swap or
    // not.  The returned wave data from the server is actually saved
    // to a file and read in by EST_Wave::load so NIST is a safe option
    // Of course when the wave is saved by the client the requested
    // format is respected.
    fprintf(fd, "(Parameter.set 'Wavefiletype 'nist)\n");
    if (async_mode) { // In async mode we need to set up tts_hooks to send back
                      // the waves
        fprintf(fd, "(tts_return_to_client)\n");
        fprintf(fd, "(tts_text \"\n");
    } else // do it in one go
        fprintf(fd, "(tts_textall \"\n");
    if (file == "-")
        tfd = stdin;
    else if ((tfd = fopen(file, "rb")) == NULL) {
        cerr << "festivald_client: can't open text file \"" << file << "\"\n";
        exit(-1);
    }

    while ((c = getc(tfd)) != EOF) {
        if ((c == '"') || (c == '\\'))
            putc('\\', fd);
        putc(c, fd);
    }
    if (file != "-")
        fclose(tfd);

    fprintf(fd, "\" \"%s\")\n", (const char*)tts_mode);

    fclose(fd);

    // Now send the file to the server
    if ((fd = fopen(tmpfile, "rb")) == NULL) {
        cerr << "festivald_client: tmpfile \"" << tmpfile
             << "\" mysteriously disappeared\n";
        exit(-1);
    }
    copy_to_server(fd, serverfd);
    fclose(fd);
    unlink(tmpfile);
}

static void copy_to_server(FILE* fdin, SERVER_FD serverfd) {
    // Open a connection and copy everything from fdin to server
    int c, n;
    int state = 0;
    int bdepth = 0;
    char ack[4];

    while ((c = getc(fdin)) != EOF) {
        putc(c, serverfd);
        new_state(c, state, bdepth);

        if (state == 1) {
            state = 0;
            fflush(serverfd);
            do {
                for (n = 0; n < 3;)
                    n += read(fileno(serverfd), ack + n, 3 - n);
                ack[3] = '\0';
                if (streq(ack, "WV\n")) // I've been sent a waveform
                    client_accept_waveform(serverfd);
                else if (streq(ack, "LP\n")) // I've been sent an s-expr
                {
                    client_accept_s_expr(serverfd);
                } else if (streq(ack, "ER\n")) {
                    cerr << "festival server error: reset to top level\n";
                    break;
                }
            } while (!streq(ack, "OK\n"));
        }
    }
}

static void new_state(int c, int& state, int& bdepth) {
    // FSM (plus depth) to detect end of s-expr

    if (state == 0) {
        if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
            state = 0;
        else if (c == '\\') // escaped character
            state = 2;
        else if (c == ';')
            state = 3; // comment
        else if (c == '"')
            state = 4; // quoted string
        else if (c == '(') {
            bdepth++;
            state = 5;
        } else
            state = 5; // in s-expr
    } else if (state == 2)
        state = 5; // escaped character
    else if (state == 3) {
        if (c == '\n')
            state = 5;
        else
            state = 3;
    } else if (state == 4) {
        if (c == '\\')
            state = 6;
        else if (c == '"')
            state = 5;
        else
            state = 4;
    } else if (state == 6)
        state = 4;
    else if (state == 5) {
        if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r')) {
            if (bdepth == 0)
                state = 1;
            else
                state = 5;
        } else if (c == '\\') // escaped character
            state = 2;
        else if (c == ';')
            state = 3; // comment
        else if (c == '"')
            state = 4; // quoted string
        else if (c == '(') {
            bdepth++;
            state = 5;
        } else if (c == ')') {
            bdepth--;
            state = 5;
        } else
            state = 5; // in s-expr
    } else             // shouldn't get here
        state = 5;
}

static void client_accept_waveform(SERVER_FD fd) {
    // Read a waveform from fd.  The waveform will be passed
    // using
    EST_String tmpfile = make_tmp_filename();
    EST_Wave sig;

    // Have to copy this to a temporary file, then load it.
    socket_receive_file(fileno(fd), tmpfile);
    sig.load(tmpfile);
    if (aucommand != "") {
        // apply the command to this file
        EST_String tmpfile2 = make_tmp_filename();
        sig.save(tmpfile2, output_type);
        char* command =
            walloc(char, 1024 + tmpfile2.length() + aucommand.length());
        sprintf(command, "FILE=\"%s\"; %s", (const char*)tmpfile2,
                (const char*)aucommand);
        if (system(command) != 0) {
            cerr << "festivald_client: The command applied to waveform "
                    "returned an error"
                 << endl;
        }
        unlink(tmpfile2);
    } else if (output_filename == "")
        cerr << "festivald_client: ignoring received waveform, no output file"
             << endl;
    else
        sig.save(output_filename, output_type);
    unlink(tmpfile);
}

static void client_accept_s_expr(SERVER_FD fd) {
    // Read an s-expression.  Inefficeintly into a file
    EST_String tmpfile = make_tmp_filename();
    FILE* tf;
    int c;

    // Have to copy this to a temporary file, then load it.
    socket_receive_file(fileno(fd), tmpfile);

    if (withlisp) {
        if ((tf = fopen(tmpfile, "rb")) == NULL) {
            cerr << "festivald_client: lost an s_expr tmp file" << endl;
        } else {
            while ((c = getc(tf)) != EOF)
                putc(c, stdout);
            fclose(tf);
        }
    }

    unlink(tmpfile);
}
