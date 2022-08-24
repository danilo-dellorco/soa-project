sudo rm /dev/test*
sudo rmmod test.ko
make clean
make all
sudo insmod test.ko