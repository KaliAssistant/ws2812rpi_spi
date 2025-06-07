#include <iostream>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include "../include/INIReader.h"
#include "../include/bcm2835.h"


#define DEFAULT_POWER_PIN RPI_V2_GPIO_P1_36 // = 16,  /*!< Version 2, Pin P1-36 */
#define DEFAULT_TMPFS_PATH "/mnt/rpisdrtx/ws2812rpi_spi/"
#define DEFAULT_CONF_PATH "/etc/ws2812rpi_spi.conf"
#define DEFAULT_CONF_SET_LEDMODE "auto"
#define DEFAULT_CONF_SET_STATUS "off"
#define DEFAULT_CONF_SET_COLOR888 "#000000"
#define DEFAULT_CONF_SET_STATUS_OFF_COLOR "#000000"
#define DEFAULT_CONF_SET_STATUS_IDLE_COLOR "#FFFF00"
#define DEFAULT_CONF_SET_STATUS_IDLE_STEP 256
#define DEFAULT_CONF_SET_STATUS_IDLE_STEP_US 500
#define DEFAULT_CONF_SET_STATUS_IDLE_DELAY_MS 5
#define DEFAULT_CONF_SET_STATUS_TX_COLOR "#0000FF"
#define DEFAULT_CONF_SET_STATUS_ERROR_COLOR "#FF0000"
#define DEFAULT_CONF_SET_STATUS_ERROR_TP_MS 500
#define DEFAULT_CONF_SET_STATUS_ERROR_DUTY 0.05
#define DEFAULT_CONF_SET_BCM2835_SPICLK_DIVIDER 75
#define DEFAULT_CONF_SET_LEDMODE_MANUAL_DELAY_MS 10



unsigned long SETTINGS_POWER_PIN = DEFAULT_POWER_PIN;
std::string SETTINGS_TMPFS_PATH = DEFAULT_TMPFS_PATH;
std::string SETTINGS_CONF_SET_LEDMODE = DEFAULT_CONF_SET_LEDMODE;
std::string SETTINGS_CONF_SET_STATUS = DEFAULT_CONF_SET_STATUS;
std::string SETTINGS_CONF_SET_COLOR888 = DEFAULT_CONF_SET_COLOR888;
std::string SETTINGS_CONF_SET_STATUS_OFF_COLOR = DEFAULT_CONF_SET_STATUS_OFF_COLOR;
std::string SETTINGS_CONF_SET_STATUS_IDLE_COLOR = DEFAULT_CONF_SET_STATUS_IDLE_COLOR;
unsigned long SETTINGS_CONF_SET_STATUS_IDLE_STEP = DEFAULT_CONF_SET_STATUS_IDLE_STEP;
uint64_t SETTINGS_CONF_SET_STATUS_IDLE_STEP_US = DEFAULT_CONF_SET_STATUS_IDLE_STEP_US;
uint64_t SETTINGS_CONF_SET_STATUS_IDLE_DELAY_MS = DEFAULT_CONF_SET_STATUS_IDLE_DELAY_MS;
std::string SETTINGS_CONF_SET_STATUS_TX_COLOR = DEFAULT_CONF_SET_STATUS_TX_COLOR;
std::string SETTINGS_CONF_SET_STATUS_ERROR_COLOR = DEFAULT_CONF_SET_STATUS_ERROR_COLOR;
uint64_t SETTINGS_CONF_SET_STATUS_ERROR_TP_MS = DEFAULT_CONF_SET_STATUS_ERROR_TP_MS;
double SETTINGS_CONF_SET_STATUS_ERROR_DUTY = DEFAULT_CONF_SET_STATUS_ERROR_DUTY;
unsigned long SETTINGS_CONF_SET_BCM2835_SPICLK_DIVIDER = DEFAULT_CONF_SET_BCM2835_SPICLK_DIVIDER;
uint64_t SETTINGS_CONF_SET_LEDMODE_MANUAL_DELAY_MS = DEFAULT_CONF_SET_LEDMODE_MANUAL_DELAY_MS;

uint8_t __SETTINGS_CONF_SET_STATUR_OFF_COLOR_R;
uint8_t __SETTINGS_CONF_SET_STATUR_OFF_COLOR_G;
uint8_t __SETTINGS_CONF_SET_STATUR_OFF_COLOR_B;
uint8_t __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_R;
uint8_t __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_G;
uint8_t __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_B;
uint8_t __SETTINGS_CONF_SET_STATUR_TX_COLOR_R;
uint8_t __SETTINGS_CONF_SET_STATUR_TX_COLOR_G;
uint8_t __SETTINGS_CONF_SET_STATUR_TX_COLOR_B;
uint8_t __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_R;
uint8_t __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_G;
uint8_t __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_B;



// Lookup table for 1 WS2812 bit → 3 SPI bits
const uint8_t ws2812_lookup[2] = { 0b100, 0b110 };

// Encode one byte into 24 SPI bits (3 bits per 1 bit)
std::vector<uint8_t> encode_byte(uint8_t byte) {
    std::vector<uint8_t> bits;
    for (int i = 7; i >= 0; --i) {
        uint8_t bit = (byte >> i) & 0x01;
        uint8_t pattern = ws2812_lookup[bit];
        for (int j = 2; j >= 0; --j)
            bits.push_back((pattern >> j) & 0x01);
    }
    return bits;
}

// Encode a GRB color into SPI byte stream
std::vector<uint8_t> encode_color(uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> bits;
    auto g_bits = encode_byte(g);
    auto r_bits = encode_byte(r);
    auto b_bits = encode_byte(b);
    bits.insert(bits.end(), g_bits.begin(), g_bits.end());
    bits.insert(bits.end(), r_bits.begin(), r_bits.end());
    bits.insert(bits.end(), b_bits.begin(), b_bits.end());

    std::vector<uint8_t> spi_data;
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; ++j) {
            byte <<= 1;
            if (i + j < bits.size())
                byte |= bits[i + j];
        }
        spi_data.push_back(byte);
    }
    return spi_data;
}

bool isHexDigit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c));
}

uint8_t hexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // should never happen if validated
}

bool parseHexByte(const std::string& str, size_t pos, uint8_t& out) {
    if (pos + 1 >= str.size()) return false;
    if (!isHexDigit(str[pos]) || !isHexDigit(str[pos + 1])) return false;

    out = (hexCharToByte(str[pos]) << 4) | hexCharToByte(str[pos + 1]);
    return true;
}

bool hexToRGB(const std::string& color, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (color.length() != 7 || color[0] != '#') return false;

    return parseHexByte(color, 1, r) &&
           parseHexByte(color, 3, g) &&
           parseHexByte(color, 5, b);
}


// Send GRB to WS2812 via SPI
void ws2812_send(uint8_t r, uint8_t g, uint8_t b) {
    auto data = encode_color(r, g, b);
    bcm2835_spi_writenb(reinterpret_cast<char*>(data.data()), data.size());
    bcm2835_delayMicroseconds(80);  // Latch/reset (>50µs)
}

void led_off() {
    ws2812_send(__SETTINGS_CONF_SET_STATUR_OFF_COLOR_R, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_G, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_B);
    bcm2835_gpio_write(SETTINGS_POWER_PIN, LOW);
    bcm2835_delay(100);
}

void led_idle(uint32_t delay_ms, uint32_t step, uint64_t step_us) {
    if (step == 0) {
        led_off();
        return;
    }
    uint8_t ri = 0;
    uint8_t gi = 0;
    uint8_t bi = 0;
    float stepi_R = (float)__SETTINGS_CONF_SET_STATUR_IDLE_COLOR_R / (float)step;
    float stepi_G = (float)__SETTINGS_CONF_SET_STATUR_IDLE_COLOR_G / (float)step;
    float stepi_B = (float)__SETTINGS_CONF_SET_STATUR_IDLE_COLOR_B / (float)step;

    for (uint32_t i = 0; i < step; ++i) {

        ri = (uint8_t)fminf(255.0f, (float)i * stepi_R);
        gi = (uint8_t)fminf(255.0f, (float)i * stepi_G);
        bi = (uint8_t)fminf(255.0f, (float)i * stepi_B);

        ws2812_send(ri, gi, bi);
        bcm2835_delayMicroseconds(step_us);
    }

    bcm2835_delay(delay_ms);

    for (int32_t i = step; i > 0; --i) {

        ri = (uint8_t)fminf(255.0f, (float)i * stepi_R);
        gi = (uint8_t)fminf(255.0f, (float)i * stepi_G);
        bi = (uint8_t)fminf(255.0f, (float)i * stepi_B);

        ws2812_send(ri, gi, bi);
        bcm2835_delayMicroseconds(step_us);
    }

    bcm2835_delay(delay_ms);
}


void led_tx() {
    ws2812_send(__SETTINGS_CONF_SET_STATUR_TX_COLOR_R, __SETTINGS_CONF_SET_STATUR_TX_COLOR_G, __SETTINGS_CONF_SET_STATUR_TX_COLOR_B);
    bcm2835_delay(100);
}

void led_error(uint32_t tp_ms, float duty) {
    ws2812_send(__SETTINGS_CONF_SET_STATUR_ERROR_COLOR_R, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_G, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_B);
    bcm2835_delay(tp_ms*duty);
    ws2812_send(0, 0, 0);
    bcm2835_delay(tp_ms-(tp_ms*duty));
}

std::string read_led_mode() {
    std::ifstream file(SETTINGS_TMPFS_PATH+"/ledmode");
    std::string mode;
    if (file.is_open())
        std::getline(file, mode);
    else
        mode = DEFAULT_CONF_SET_LEDMODE;
    return mode;
}

std::string read_ctl_color888() {
    std::ifstream file(SETTINGS_TMPFS_PATH+"/color888");
    std::string color;
    if (file.is_open())
        std::getline(file, color);
    else
        color = DEFAULT_CONF_SET_COLOR888;
    return color;
}

std::string read_led_status() {
    std::ifstream file(SETTINGS_TMPFS_PATH+"/status");
    std::string status;
    if (file.is_open())
        std::getline(file, status);
    else
        status = DEFAULT_CONF_SET_STATUS;
    return status;
}


void help_usage(char* exec_name) {
    std::cout << \
    "Usage: " << exec_name << " [Options]\n\n"\
    "Options: \n"\
    "\t-c <Config file path> | User config file (Default : /etc/ws2812rpi_spi.conf)\n"\
    "\t-h\t| Show this help\n\n"\
    "ws2812rpi_spi - Control WS2812b Neo Pixel on Raspberry Pi BCM2835 using SPI0\n"\
    "KaliAssistant <work.kaliassistant.github@gmail.com>\n"\
    "Github: git@github.com:KaliAssistant/ws2812rpi_spi.git\n";
}


int main(int argc, char **argv) {
    int opt;
    char* arg_conf_path = nullptr;
    while ((opt = getopt(argc, argv, "hc:")) != -1) {
        switch (opt) {
            case 'h':
                help_usage(argv[0]);
                return 1;
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

    std:: ifstream defaultconf_file(DEFAULT_CONF_PATH);
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
        SETTINGS_POWER_PIN = reader.GetUnsigned("GPIO", "PowerPin", DEFAULT_POWER_PIN);
        SETTINGS_CONF_SET_BCM2835_SPICLK_DIVIDER = reader.GetUnsigned("GPIO", "BCM2835_spiclk_div", DEFAULT_CONF_SET_BCM2835_SPICLK_DIVIDER);
        SETTINGS_CONF_SET_LEDMODE = reader.Get("Default", "LedMode", DEFAULT_CONF_SET_LEDMODE);
        SETTINGS_CONF_SET_STATUS = reader.Get("Default", "Status", DEFAULT_CONF_SET_STATUS);
        SETTINGS_CONF_SET_COLOR888 = reader.Get("Default", "Color888", DEFAULT_CONF_SET_COLOR888);
        SETTINGS_CONF_SET_LEDMODE_MANUAL_DELAY_MS = reader.GetUnsigned64("Default", "LedMode_manual_delay_ms", DEFAULT_CONF_SET_LEDMODE_MANUAL_DELAY_MS);
        SETTINGS_CONF_SET_STATUS_OFF_COLOR = reader.Get("User", "Status_off_color", DEFAULT_CONF_SET_STATUS_OFF_COLOR);
        SETTINGS_CONF_SET_STATUS_IDLE_COLOR = reader.Get("User", "Status_idle_color", DEFAULT_CONF_SET_STATUS_IDLE_COLOR);
        SETTINGS_CONF_SET_STATUS_IDLE_STEP = reader.GetUnsigned("User", "Status_idle_step", DEFAULT_CONF_SET_STATUS_IDLE_STEP);
        SETTINGS_CONF_SET_STATUS_IDLE_STEP_US = reader.GetUnsigned64("User", "Status_idle_step_us", DEFAULT_CONF_SET_STATUS_IDLE_STEP_US);
        SETTINGS_CONF_SET_STATUS_IDLE_DELAY_MS = reader.GetUnsigned64("User", "Status_idle_delay_ms", DEFAULT_CONF_SET_STATUS_IDLE_DELAY_MS);
        SETTINGS_CONF_SET_STATUS_TX_COLOR = reader.Get("User", "Status_tx_color", DEFAULT_CONF_SET_STATUS_TX_COLOR);
        SETTINGS_CONF_SET_STATUS_ERROR_COLOR = reader.Get("User", "Status_error_color", DEFAULT_CONF_SET_STATUS_ERROR_COLOR);
        SETTINGS_CONF_SET_STATUS_ERROR_TP_MS = reader.GetUnsigned64("User", "Status_error_tp_ms", DEFAULT_CONF_SET_STATUS_ERROR_TP_MS);
        SETTINGS_CONF_SET_STATUS_ERROR_DUTY = reader.GetReal("User", "Status_error_duty", DEFAULT_CONF_SET_STATUS_ERROR_DUTY);


        if (!hexToRGB(SETTINGS_CONF_SET_STATUS_OFF_COLOR, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_R, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_G, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_B))
            hexToRGB(DEFAULT_CONF_SET_STATUS_OFF_COLOR, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_R, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_G, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_B);

        if (!hexToRGB(SETTINGS_CONF_SET_STATUS_IDLE_COLOR, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_R, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_G, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_B))
            hexToRGB(DEFAULT_CONF_SET_STATUS_IDLE_COLOR, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_R, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_G, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_B);

        if (!hexToRGB(SETTINGS_CONF_SET_STATUS_TX_COLOR, __SETTINGS_CONF_SET_STATUR_TX_COLOR_R, __SETTINGS_CONF_SET_STATUR_TX_COLOR_G, __SETTINGS_CONF_SET_STATUR_TX_COLOR_B))
            hexToRGB(DEFAULT_CONF_SET_STATUS_TX_COLOR, __SETTINGS_CONF_SET_STATUR_TX_COLOR_R, __SETTINGS_CONF_SET_STATUR_TX_COLOR_G, __SETTINGS_CONF_SET_STATUR_TX_COLOR_B);

        if (!hexToRGB(SETTINGS_CONF_SET_STATUS_ERROR_COLOR, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_R, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_G, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_B))
            hexToRGB(DEFAULT_CONF_SET_STATUS_ERROR_COLOR, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_R, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_G, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_B);
    }
    else {
        hexToRGB(DEFAULT_CONF_SET_STATUS_OFF_COLOR, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_R, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_G, __SETTINGS_CONF_SET_STATUR_OFF_COLOR_B);
        hexToRGB(DEFAULT_CONF_SET_STATUS_IDLE_COLOR, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_R, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_G, __SETTINGS_CONF_SET_STATUR_IDLE_COLOR_B);
        hexToRGB(DEFAULT_CONF_SET_STATUS_TX_COLOR, __SETTINGS_CONF_SET_STATUR_TX_COLOR_R, __SETTINGS_CONF_SET_STATUR_TX_COLOR_G, __SETTINGS_CONF_SET_STATUR_TX_COLOR_B);
        hexToRGB(DEFAULT_CONF_SET_STATUS_ERROR_COLOR, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_R, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_G, __SETTINGS_CONF_SET_STATUR_ERROR_COLOR_B);
    }




    if (!bcm2835_init()) {
        std::cerr << "Failed to init bcm2835\n";
        return 1;
    }
    if (!bcm2835_spi_begin()) {
        std::cerr << "Failed to init SPI\n";
        return 1;
    }

    bcm2835_gpio_fsel(SETTINGS_POWER_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(SETTINGS_POWER_PIN, HIGH);
    bcm2835_spi_setClockDivider(SETTINGS_CONF_SET_BCM2835_SPICLK_DIVIDER);  // 250/75 -> ~3.3_ MHz (Working on my rpi02w ws2812b)
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    while (true) {
        bcm2835_gpio_write(SETTINGS_POWER_PIN, HIGH);

        std::string led_mode = read_led_mode();
        std::string status = read_led_status();
        std::string led_maunal_color = read_ctl_color888();

        if (led_mode == "auto") {
            if (status == "idle")              
                led_idle(SETTINGS_CONF_SET_STATUS_IDLE_DELAY_MS, SETTINGS_CONF_SET_STATUS_IDLE_STEP, SETTINGS_CONF_SET_STATUS_IDLE_STEP_US);
            else if (status == "tx")
                led_tx();
            else if (status == "error")  
                led_error(SETTINGS_CONF_SET_STATUS_ERROR_TP_MS, SETTINGS_CONF_SET_STATUS_ERROR_DUTY);
            else if (status == "off")
                led_off();
            else
                led_off();
        }
        else if (led_mode == "manual") {
            uint8_t r, g, b;
            if (hexToRGB(led_maunal_color, r, g, b))
            {
                ws2812_send(r, g, b);
                bcm2835_delay(SETTINGS_CONF_SET_LEDMODE_MANUAL_DELAY_MS);
            }
            else {
                led_off();
            } 
        }
        else {
            led_off();
        }
    }

    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}

