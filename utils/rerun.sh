
pulseaudio -k;
sleep 3;
make -C ..;
sudo ./remove  hda
#sudo ./remove aloop
sudo ./insert hda
#sudo ./insert aloop
#pulseaudio --start

#paplay -v ~/AURAL/Audio/maxwork.wav

#pulseaudio --start; sleep 2;
#paplay --raw ~/AURAL/Audio/viol-1mn.wav

#amixer cset numid=12 on # make sure spdif is on
#mplayer -ac hwac3 -ao alsa:device=hw=0.1 ~/Bjorn_Lynne-The_Fairy_Woods.ac3

#aplay -Dhw:0,1 ~/AURAL/Audio/viol-1mn.wav
#make -C ..; pulseaudio -k; sudo ./remove usb; sudo ./insert usb; pulseaudio --start; sleep 2;
