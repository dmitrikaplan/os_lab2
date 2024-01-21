#!/bin/zsh
sudo java -jar read_address.jar
make clean
make
sudo rmmod driver
sudo insmod ./driver.ko
sudo chmod 777 /dev/iostat_device
make clean
