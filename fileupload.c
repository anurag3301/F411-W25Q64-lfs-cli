#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/select.h>
#define DEBUG   1

enum FIO_ERR{
    TIMEOUT_ERR = 0xF1F1F1F1,
    EXISTS_ERR = 0xF2F2F2F2,
    CREATE_ERR = 0xF3F3F3F3,
    NOEXSITS_ERR = 0xF4F4F4F4,
    NOFILE_ERR = 0xF5F5F5F5,
};

static uint8_t our_xor = 0;
static uint32_t packet_count = 0;

static inline size_t min(size_t a, size_t b){
    return (a < b) ? a : b;
}

int open_serial(const char *port, int baud){
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    /* Control flags: 8N1, no modem control, enable receiver */
    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;

    /* Input flags: raw binary, no newline translation */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR | ISTRIP);

    /* Output flags: raw, no post-processing */
    tty.c_oflag &= ~(OPOST | ONLCR);

    /* Local flags: raw mode, no echo */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    /* Blocking read, one byte minimum */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) < 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    return fd;
}

/* Write exactly n bytes — write() can return short */
int write_all(int fd, const uint8_t *buf, size_t n){
    size_t written = 0;
    while (written < n) {
        ssize_t r = write(fd, buf + written, n - written);
        if (r < 0) { perror("write"); return -1; }
        written += r;
    }
    return 0;
}

int write_all_slow(int fd, const uint8_t *buf, size_t n){
    for(size_t i=0; i<n; i++){
        write(fd, buf+i, 1);
        tcdrain(fd);
        usleep(2000);
    }
    return 0;
}

/* Read exactly n bytes — blocks until all arrive */
int read_all(int fd, uint8_t *buf, size_t n){
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r < 0) { perror("read"); return -1; }
        got += r;
    }
    return 0;
}

void dump_buf(uint8_t buf[], ssize_t size){
    printf("[");
    for(ssize_t i=0; i<size; i++){
        printf("%x, ", buf[i]);
    }
    printf("]\n");
}

static uint32_t crc33_stm32(const uint8_t *data, size_t len){
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i += 4) {
        /* Little-endian word — matches how STM32 reads from memory into CRC->DR */
        uint32_t word = ((uint32_t)data[i+3] << 24)
                      | ((uint32_t)data[i+2] << 16)
                      | ((uint32_t)data[i+1] <<  8)
                      | ((uint32_t)data[i+0] <<  0);

        for (int b = 0; b < 32; b++) {
            if ((crc ^ word) & 0x80000000U) {
                crc = (crc << 1) ^ 0x04C11DB7U;
            } else {
                crc = (crc << 1);
            }
            word <<= 1;
        }
    }
    return crc;
}


static bool recv_packet_from_mcu(uint8_t *packet, int fd){
    if (read_all(fd, packet, 128) < 0) return false;
    packet_count++;
    ((uint32_t *)packet)[32] = packet_count;
    uint32_t our_crc = crc33_stm32(packet, 132);
    return write_all(fd, (uint8_t *)&our_crc, 4) == 0;
}

static bool send_packet(uint8_t *packet, int fd){
    uint32_t board_crc, our_crc;

    packet_count++;
    uint32_t *words = (uint32_t *)packet;
    words[32] = packet_count; 

    write_all(fd, (uint8_t *)packet, 128);
    read_all(fd, (uint8_t *)&board_crc, 4);
    our_crc = crc33_stm32(packet, 128+4);
    if(DEBUG){
        printf("SENT PACKET %ld: ", packet_count);
        dump_buf(packet, 128+4);
        printf("Board CRC : 0x%08X\n", board_crc);
        printf("Our CRC   : 0x%08X\n", our_crc);
    }
    if(board_crc != our_crc){
        if(board_crc == TIMEOUT_ERR){
            printf("\nTerminated due to timeout!\n");
        }
        else if(board_crc == EXISTS_ERR){
            printf("\nTerminated due to file exists!\n");
        }
        else if(board_crc == CREATE_ERR){
            printf("\nTerminated due to coundnt crate file!\n");
        }
        else if(board_crc == NOEXSITS_ERR){
            printf("\nTerminated due to file doesnt exsits!\n");
        }
        else if(board_crc == NOFILE_ERR){
            printf("\nTerminated due to specified path is not regular file!\n");
        }
        else{
            printf("\nTerminated due to integrity mismatch, maybe retry!\n");
        }
        return false;
    }
    return true;
}

void initiate_coms(int fd, const char* cmd){
    write_all_slow(fd, "\n", 1);
    tcdrain(fd);
    printf("\n||| START IGNORE RECEIVE BYTES |||\n");
    uint8_t prev = 0, curr = 0;
    for(;;){
        read(fd, &curr, 1);
        printf("%c", curr);
        if(prev == '#' && curr == ' ') break;
        prev = curr;
    }
    tcflush(fd, TCIFLUSH);

    write_all_slow(fd, (uint8_t *)cmd, strlen(cmd));
    tcdrain(fd);

    uint8_t window[4] = {0};
    for(;;){
        uint8_t b;
        read(fd, &b, 1);
        printf("%c", b);
        window[0] = window[1];
        window[1] = window[2];
        window[2] = window[3];
        window[3] = b;
        if(window[0]==0x95 && window[1]==0x54 &&
           window[2]==0x95 && window[3]==0x54) break;
    }
    tcflush(fd, TCIFLUSH);
    printf("\n||| END IGNORE RECEIVE BYTES |||\n");
}

uint32_t getFileSize(const char* path){
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}


int send_file(int ufd, char *filename){
    initiate_coms(ufd, "receive\n");
    printf("\nSending packets!!\n");
    int ffd = open(filename, O_RDONLY);
    if (ffd < 0) {
        perror("open");
        return 1;
    }

    uint8_t packet[128+4] = {0};
    uint32_t *words = (uint32_t*)packet;
    words[0] = getFileSize(filename);
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    sprintf(packet+4, "%s", basename);
    if(!send_packet(packet, ufd)) return 1;

    ssize_t n;
    while ((n = read(ffd, packet, 128)) > 0) {
        if(!send_packet(packet, ufd)) return 1;
    }

    close(ffd);
    return 0;
}


int receive_file(int ufd){
    initiate_coms(ufd, "send\n");
    char path[512] = {0};
    uint8_t packet[128+4] = {0};
    printf("Enter filepath: ");
    scanf("%511s", path);
    printf("Requesting file [%s]\n", path);
    size_t len = strlen(path) + 1;  // include '\0'
    for (int i = 0; i < 4; i++) {
        memset(packet, 0, sizeof(packet));
        size_t offset = i * 128;
        if (offset < len) {
            memcpy(packet, path + offset, min(128, len - offset));
        }
        if (!send_packet(packet, ufd)) return 1;
    }

    printf("\nReceiving packets!!\n");

    uint8_t meta_packet[128+4] = {0};
    if (!recv_packet_from_mcu(meta_packet, ufd)) return 1;
    uint32_t filesize = ((uint32_t *)meta_packet)[0];
    char filename[101];
    memcpy(filename, meta_packet + 4, 100);
    filename[100] = '\0';

    char *local_name = strrchr(path, '/');
    local_name = local_name ? local_name + 1 : path;
    printf("Receiving: %s (%u bytes) -> ./%s\n", filename, filesize, local_name);

    int out_fd = open(local_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) { perror("open output"); return 1; }

    uint32_t remaining = filesize;
    uint8_t data_packet[128+4] = {0};
    while (remaining > 0) {
        if (!recv_packet_from_mcu(data_packet, ufd)) break;
        size_t to_write = remaining < 128 ? remaining : 128;
        write(out_fd, data_packet, to_write);
        remaining -= to_write;
    }

    close(out_fd);
    printf("Saved: ./%s\n", local_name);
    return 0;
}

int main(int argc, char* argv[]){
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *port = NULL;
    const char *upload_path = NULL;
    bool do_download = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "--upload") == 0 && i + 1 < argc) {
            upload_path = argv[++i];
        } else if (strcmp(argv[i], "--download") == 0) {
            do_download = true;
        } else {
            fprintf(stderr, "Usage: %s --port <dev> --upload <path> | --download\n", argv[0]);
            return 1;
        }
    }

    if (!port || (!upload_path && !do_download)) {
        fprintf(stderr, "Usage: %s --port <dev> --upload <path> | --download\n", argv[0]);
        return 1;
    }

    int ufd = open_serial(port, B115200);
    if (ufd < 0) return 1;
    printf("Connected to: %s\n", port);

    int ret;
    if (upload_path) {
        ret = send_file(ufd, (char *)upload_path);
        if (ret == 0) printf("\nFile sent successfully\n");
    } else {
        ret = receive_file(ufd);
    }

    close(ufd);
    return ret;
}
