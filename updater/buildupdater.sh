#!/bin/sh
cd /root/SC1000/updater/tarball
rm /root/SC1000/updater/sc.tar
cp /root/SC1000/software/xwax .
tar -cf /root/SC1000/updater/sc.tar *
cp /root/SC1000/updater/xwax "/media/root/SC1000STICK"
cp /root/SC1000/updater/sc.tar "/media/root/SC1000STICK"
#cp /root/SC1000/software/scsettings.txt "/media/root/SC1000STICK"
