#!/bin/sh
cd /root/SC1000/updater/tarball
rm /root/SC1000/updater/sc.tar
cp /root/SC1000/software/xwax .
cp /root/SC1000/software/scsettings.txt .
cp /root/SC1000/software/scsettings.txt ../
tar -cf /root/SC1000/updater/sc.tar *
#cp /root/SC1000/updater/xwax "/media/root/5A07-BBBC"
#cp /root/SC1000/updater/sc.tar "/media/root/5A07-BBBC"
#cp /root/SC1000/software/scsettings.txt "/media/root/SC1000STICK"
