#include <iostream>
#include <fcntl.h>      // open()
#include <termios.h>    // termios, tcgetattr(), tcsetattr()
#include <unistd.h>     // read(), write(), close()
#include <cstring>      // memset()
#include <errno.h>

int set_interface_attribs(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error from tcgetattr: " << strerror(errno) << std::endl;
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                      // disable break processing
    tty.c_lflag = 0;                             // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                             // no remapping, no delays
    tty.c_cc[VMIN]  = 1;                         // read blocks until 1 byte is available
    tty.c_cc[VTIME] = 1;                         // 0.1 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);             // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);           // no parity
    tty.c_cflag &= ~CSTOPB;                      // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                     // no hardware flow control

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr: " << strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

int main() {
    const char* portname = "/dev/ttyUSB0";  // 시리얼 포트 경로
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Error opening " << portname << ": " << strerror(errno) << std::endl;
        return 1;
    }

    // Baudrate 설정 (예: B115200)
    if (set_interface_attribs(fd, B115200) != 0) {
        close(fd);
        return 1;
    }

    std::cout << "Serial port " << portname << " opened at 115200 baud." << std::endl;

    // 송신 예
    // 여기에 deg로 각도 보내기 ESP에서 파싱 코드 작성할것  예) 30
    const char* msg = "Hello, serial world!\n";
    int n_written = write(fd, msg, strlen(msg));
    if (n_written < 0) {
        std::cerr << "Error writing: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Sent " << n_written << " bytes: " << msg;
    }

    // 수신 예제
    // 여기서 OK 받을것
    char buf[100];
    int n_read = read(fd, buf, sizeof(buf));
    if (n_read > 0) {
        std::cout << "Received " << n_read << " bytes: ";
        std::cout.write(buf, n_read);
        std::cout << std::endl;
    } else if (n_read == 0) {
        std::cout << "No data received (timeout)" << std::endl;
    } else {
        std::cerr << "Error reading: " << strerror(errno) << std::endl;
    }

    close(fd);
    return 0;
}

