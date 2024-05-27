# DWZoomScale
A simple Wayland and specifically Wayfire application to quickly zoom in/out your screen.
Wayfire has the zoom plugin and if it's activated then you simply need to press down the Super (Meta) key and use your mouse wheel to zoom in/out the screen.
This application simply places a small 3x3 button on the top left corner of the screen and whenever you need to quicly zoom in the screen, you simply move your mouse to the top left corner and scroll up or down to zoom in or out respectively.
It works by simulating the Super key while you scroll up/down with the mouse wheel.
It was tested on Debian 12 on Wayfire.

# What you can do with DWZoomScale
   1. Quickly zoom in/out the screen.

   to build the project you need to install the following libs:

		make
		pkgconf
		libwayland-dev
		libxkbcommon-dev

   on Debian run the following command:

		sudo apt install make pkgconf libwayland-dev libxkbcommon-dev

# Installation/Usage
  1. Open a terminal and run:
 
		 chmod +x ./configure
		 ./configure

  2. if all went well then run:

		 make
		 sudo make install
		 
		 (if you just want to test it then run: make run)
		
  3. Run the application:
  
		 dwzoomscale

# Support

   My Libera IRC support channel: #linuxfriends

   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org

   Email: nicolas.dio@protonmail.com
