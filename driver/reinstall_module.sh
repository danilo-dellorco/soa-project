sudo rm /dev/mflow-dev*
sudo rmmod multiflow-driver.ko
make clean
make all
sudo insmod multiflow-driver.ko
make clean