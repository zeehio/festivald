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
/*             Author :  Alan W Black                                    */
/*             Date   :  December 1996                                   */
/*-----------------------------------------------------------------------*/
/*=======================================================================*/
/* Copyright Sergio Oller, 2018 */
/*                                                                       */
/* A festival server on a socket file                                    */
/*                                                                       */

// Standard includes
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

// POSIX includes
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// systemd includes
#ifdef WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

// speech-tools and festival includes:
#include <EST_String.h>
#include <EST_cmd_line.h>
#include <festival.h>
#include <siod.h> /* repl_from_socket */

#define DEFAULT_MAX_CLIENTS 10
#define FESTIVALD_HEAP_SIZE 10000000
#define DEFAULT_SOCKET_PATH "festivald.socket"

static const char* festivald_version = "0.1";

static int festivald(int* f_socket, const char* socket_path,
                     bool* socket_created);
static int festival_accept_connections(int fd, int max_clients);
static void log_message(int client, const char* message);

/* Handles the command line arguments, initializes festival and calls the
 * socket accept/create function */
int main(int argc, char** argv) {
    int load_init_files = 1;
    EST_Option al;
    EST_StrList extra_args; // Needed by API, speech tools but ignored
    long int heap_size = 0;
    int max_clients = DEFAULT_MAX_CLIENTS;
    const char* socket_path = DEFAULT_SOCKET_PATH;
    parse_command_line(
        argc, argv,
        EST_String("Usage:\n") + "festivald  <options>\n" + "festivald " +
            festivald_version + "(with festival " + festival_version + ")\n" +
            "-q            Load no default setup files\n" +
            "--libdir <string>\n" +
            "              Set library directory pathname\n" +
            "--socket <string>\n" + "              Socket path. It should be "
                                    "in a directory with restricted "
                                    "permissions\n" +
#ifdef WITH_SYSTEMD
            "              (Unused if systemd passes the socket)\n" +
#endif
            "--max-clients <int> {10}\n" + "              Max. number of "
                                           "clients allowed to connect to the "
                                           "server\n" +
            "--heap <int> {10000000}\n" +
            "              Set size of Lisp heap, should not normally need\n" +
            "              to be changed from its default\n" +
            "-v            Display version number and exit\n" +
            "--version     Display version number and exit\n",
        extra_args, al);

    if ((al.present("-v")) || (al.present("--version"))) {
        std::cout << "festivald version " << festivald_version
                  << "with festival version " << festival_version << std::endl;
        return 0;
    }

    if (al.present("--libdir"))
        festival_libdir = wstrdup(al.val("--libdir"));
    else if (getenv("FESTIVALD_LIBDIR") != 0)
        festival_libdir = getenv("FESTIVALD_LIBDIR");
    // Default value is already set by festival

    // Set heap
    if (al.present("--heap"))
        heap_size = al.ival("--heap");
    else if (getenv("FESTIVALD_HEAP_SIZE") != 0)
        heap_size = strtol(getenv("FESTIVALD_HEAP_SIZE"), NULL, 10);
    else
        heap_size = FESTIVALD_HEAP_SIZE;

    // Validate heap
    if (heap_size <= 0)
        heap_size = FESTIVALD_HEAP_SIZE;

    // Set load init files
    if (al.present("-q"))
        load_init_files = 0;
    else if (getenv("FESTIVALD_LOAD_INIT") != 0)
        load_init_files = strtol(getenv("FESTIVALD_LOAD_INIT"), NULL, 10);
    else
        load_init_files = 1;

    // Validate load_init_files
    if (load_init_files != 0)
        load_init_files = 1;

    // Set max_clients
    if (al.present("--max-clients"))
        max_clients = al.ival("--max-clients");
    else if (getenv("FESTIVALD_MAX_CLIENTS") != 0)
        max_clients = strtol(getenv("FESTIVALD_MAX_CLIENTS"), NULL, 10);
    else
        max_clients = DEFAULT_MAX_CLIENTS;

    // Validate max_clients
    if (max_clients < 0)
        max_clients = DEFAULT_MAX_CLIENTS;

    if (al.present("--socket"))
        socket_path = al.val("--socket");
    else if (getenv("FESTIVALD_SOCKET_PATH") != 0)
        socket_path = getenv("FESTIVALD_SOCKET_PATH");
    else
        socket_path = DEFAULT_SOCKET_PATH;

    festival_initialize(load_init_files, heap_size);
    /* Gets the socket from systemd or creates one at the socket path */
    int f_socket = -1;
    bool socket_created = false;
    if (festivald(&f_socket, socket_path, &socket_created) < 0) {
        if (socket_created) {
            unlink(socket_path);
        }
        return 1;
    }
    int retval = festival_accept_connections(f_socket, max_clients);
    if (socket_created) {
        unlink(socket_path);
    }
    if (f_socket != -1) {
        close(f_socket);
    }
    return retval;
}

static int festivald_nosystemd(int* f_socket, const char* socket_path,
                               bool* socket_created) {
    int fd;
    *socket_created = false;
    if (socket_path == NULL) {
        std::cerr << "Path to socket missing" << std::endl;
        return -1;
    }
    union {
        struct sockaddr sa;
        struct sockaddr_un un;
    } sa;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket(): " << strerror(errno) << std::endl;
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.un.sun_family = AF_UNIX;
    strncpy(sa.un.sun_path, socket_path, sizeof(sa.un.sun_path));
    unlink(socket_path);
    if (bind(fd, &sa.sa, sizeof(sa)) < 0) {
        std::cerr << "bind(): " << strerror(errno) << std::endl;
        if (fd != -1)
            close(fd);
        return -1;
    }
    *socket_created = true;

    if (listen(fd, SOMAXCONN) < 0) {
        std::cerr << "listen(): " << strerror(errno) << std::endl;
        if (fd != -1)
            close(fd);
        return -1;
    }
    *f_socket = fd;
    return 0;
}

/* Gets the socket from systemd or creates one at the socket path.
 * Writes the socket fd into f_socket
 * Returns 0 if ok. Returns <0 on error.
 */
static int festivald(int* f_socket, const char* socket_path,
                     bool* socket_created) {
    *socket_created = false;
#ifdef WITH_SYSTEMD
    int n;
    // Tries the systemd socket activation, otherwise it creates the socket
    n = sd_listen_fds(0);
    if (n > 1) {
        std::cerr << "Too many file descriptors received." << std::endl;
        return -1;
    } else if (n == 1) {
        *f_socket = SD_LISTEN_FDS_START + 0;
    } else {
        return festivald_nosystemd(f_socket, socket_path, socket_created);
    }
    return 0;
#else
    return festivald_nosystemd(f_socket, socket_path, socket_created);
#endif
}

static int festival_accept_connections(int fd, int max_clients) {
    int fd1, statusp;
    int client_name = 0, num_clients = 0;
    pid_t pid;

    while (1) // never exits except by signals
    {
        if ((fd1 = accept(fd, 0, 0)) < 0) {
            std::cerr << "socket: accept failed";
            return 1;
        }
        client_name++;
        num_clients++;

        // Fork new image of festival and call interpreter
        if (num_clients > max_clients) {
            log_message(client_name, "failed: too many clients");
            num_clients--;
        } else if ((pid = fork()) == 0) {
            ft_server_socket = fd1;
            log_message(client_name, "connected");
            repl_from_socket(fd1);
            log_message(client_name, "disconnected");
            exit(0);
        } else if (pid < 0) {
            log_message(client_name, "failed to fork new client");
            num_clients--;
        }

        while ((num_clients > 0) && (waitpid(0, &statusp, WNOHANG) != 0))
            num_clients--;
        close(fd1);
    }
    return 0;
}

static void log_message(int client, const char* message) {
    if (client == 0) {
        std::cerr << "server: ";
    } else {
        std::cerr << "client[" << client << "]: ";
    }
    std::cerr << message << std::endl;
}
