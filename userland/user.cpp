#include <iostream>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "../common.h"

#define REQUEST _IOWR('a','a',Message*)

static struct Driver {
    Message storage[MESSAGE_STORAGE_SIZE];
    int fd;
    int length;
    Driver(){
        length = 0;
        fd = open("/dev/iostat_device", O_RDWR);
        if (fd < 0) {
            throw std::runtime_error("Could not open driver");
        }
    }
    int64_t read(){
        length = ioctl(fd, REQUEST, &storage);
        std::cout << "Количество устройств: " << length << std::endl;
        for (int64_t i = 0; i < length; ++i){
            std::cout << "Устройство: " << storage[i].name << std::endl
            << "Чтений: " << storage[i].rd << std::endl
            << "Прочтено сектров: " << storage[i].rd_sectors << std::endl
            << "Прочтено: " << storage[i].rd_sectors / 2 / 1024 << "MB" << std::endl
            << "Записей: " << storage[i].wr << std::endl
            << "Записано сектров: " << storage[i].wr_sectors  << std::endl
            << "Записано: " << storage[i].wr_sectors / 2 / 1024 << "MB" << std::endl
            << std::endl;
        }
        return length;
    }
    ~Driver(){
        close(fd);
    }
} driver;

int main(int argc, char *argv[]) {
    driver.read();
    return 0;
}
