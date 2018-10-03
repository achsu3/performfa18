#!/bin/sh

sudo rmmod queue
sudo dmesg -C
sudo insmod queue.ko
dmesg
