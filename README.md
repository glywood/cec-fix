# cec-fix
Some Raspberry Pi code that fixes my HDMI-CEC issues.

My setup is:
- LG TV (an old one; model 60pa6500)
- Yamaha YAS-108 sound bar, connected to an HDMI input on the TV
- Roku Ultra, connected to the HDMI input on the sound bar

I really just wanted the power and volume buttons on the Roku remote to work in the most reasonable way, i.e. pressing the power button turns on/off both the TV and sound bar, and pressing the volume buttons control the sound bar. Roku explicitly doesn't support receivers or soundbars, so this turned out to be a much bigger pain than expected.

The closest I could come was to put the Roku remote in HDMI-CEC mode, which allowed the volume buttons to work correctly, and the Roku was able to turn on the TV. This left two missing features: the Roku could not turn off the TV (this appears to be a limitation of all LG TVs' SimpLink implementations), and turning the TV on via the Roku would leave the soundbar powered off.

I did some research, and I found out that my TV has a serial port on the back panel that can be used to issue control commands, and the manual even has good documentation for how to use it. Some quick experimentation showed that it is possible to turn the TV off using the serial port. I also did some research into HDMI-CEC and found that it uses a shared bus between all devices, so I thought it should be possible to have a Raspberry Pi intercept whatever commands the Roku was sending to the TV and either replay them on the TV's serial port or retarget them to the soundbar. Easy, right?

So I went and put together this hack. I used the following parts:
- Raspberry Pi Zero W
- SD card with Raspbian
- Power adapter and cable
- Official RPi Case
- USB OTG cable
- Mini HDMI to HDMI cable
- USB to RS-232 serial adapter
- RS-232 serial null modem cable

I hooked it all up and plugged the RPi into a second HDMI port on the back of the TV. I fussed with cec-client and libcec for a while, but eventually abandoned that idea because it's too focused on implementing a well-behaved device. I wanted to hide anonymously and hijack traffic, which it didn't really let me do. :)

Even the lower-level Broadcom APIs don't really give me the level of control that I want, but I did manage to get something working here, by telling the underlying framework code that the RPi is the TV, which allowed it to intercept traffic that was targeting my actual TV. (I can't see anything the TV sends to the other devices, only messages going to the TV.) This allows me to see the Roku's power on and off messages at least, and I can send messages from the "fake TV" to the soundbar.

Download/compile/run on the raspberry pi itself, as it references some firmware libraries that are only available on the device. Just type 'make' to build it, then './cec-fix' to run.
