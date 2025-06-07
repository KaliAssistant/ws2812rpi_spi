/*
 * FILE:          src/ws2812rpi_pipe.cpp
 * Author:        KaliAssistant <work.kaliassistant.github@gmail.com>
 * URL:           https://github.com/KaliAssistant/ws2812rpi_spi
 * Licence        GNU/GPLv3.0
 *
 */


#include <iostream>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <sys/select.h>
#include "../include/INIReader.h"

#define DEFAULT_TMPFS_PATH "/mnt/rpisdrtx/ws2812rpi_spi/"
#define DEFAULT_CONF_PATH "/etc/ws2812rpi_spi.conf"

std::string SETTINGS_TMPFS_PATH = DEFAULT_TMPFS_PATH;

volatile sig_atomic_t got_sigpipe = 0;

void sigpipe_handler(int signum) {
    got_sigpipe = 1; // just set a flag
}


int main(int argc, char **argv) {
    // Handle SIGPIPE
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGINT, sigpipe_handler);
    signal(SIGTERM, sigpipe_handler);
    signal(SIGKILL, sigpipe_handler);

    int opt;
    char* arg_conf_path = nullptr;
    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
            case 'c':
                arg_conf_path = optarg;
                break;
            case '?':
                std::cerr << "E: Got unknown option. use -h to get help.\n";
                return 1;
            default:
                std::cerr << "EEE+: Got unknown parse returns: " << opt << std::endl;
                abort();
        }
    }

    for (int i = optind; i < argc; i++) {
        std::cerr << "W+: Non-option argument: " << argv[i] << std::endl;
    }

    std::ifstream defaultconf_file(DEFAULT_CONF_PATH);
    if (defaultconf_file.is_open()) {
        arg_conf_path = DEFAULT_CONF_PATH;
        defaultconf_file.close();
    }

    if (arg_conf_path) {
        INIReader reader(arg_conf_path);
        if (reader.ParseError() < 0) {
          std::cerr << "E: Can't load config \"" << arg_conf_path << std::endl;
          return 1;
        }
        
        SETTINGS_TMPFS_PATH = reader.Get("TMPFS", "Path", DEFAULT_TMPFS_PATH);
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 2;  // wait up to 2 seconds
    timeout.tv_usec = 0;

    int ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &timeout);
    if (ret == -1) {
        perror("EEE+: select");
        return 1;
    } else if (ret == 0) {
        std::cerr << "W+: No stdin input. Exiting.\n";
        return 0;
    }

    // stdin is ready, copy to stdout
    char buffer[4096];
    ssize_t bytes;
    std::cerr << "I+: Stdin detected. Passing to stdout...\n";
    std::ofstream ledmode(SETTINGS_TMPFS_PATH+"/ledmode", std::ios::out);
    if (ledmode.is_open()) {
        ledmode << "auto";
        ledmode.close();
    }
    std::ofstream ledstatus(SETTINGS_TMPFS_PATH+"/status", std::ios::out);
    if (ledstatus.is_open()) {
        ledstatus << "tx";
        ledstatus.close();
    }


    while ((bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        if (got_sigpipe) break;
        ssize_t written = write(STDOUT_FILENO, buffer, bytes);
        if (written != bytes) {
            std::cerr << "E: Error writing to stdout\n";
            break;
        }
    }
    std::ofstream ledstatus_end(SETTINGS_TMPFS_PATH+"/status", std::ios::out);
    if (ledstatus_end.is_open()) {
        ledstatus_end << "idle";
        ledstatus_end.close();
    }
    return 0;
}

