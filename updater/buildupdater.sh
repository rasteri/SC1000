#!/bin/sh
cd /root/SC1000/updater/tarball
rm /root/SC1000/updater/sc.tar
cp /root/SC1000/software/xwax .
tar -cf /root/SC1000/updater/sc.tar *
cp /root/SC1000/updater/xwax "/media/root/NEW VOLUME"
cp /root/SC1000/updater/sc.tar "/media/root/NEW VOLUME"
