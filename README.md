# electron-r410-setup

A Particle project named electron-r410-setup

## Background
Getting Cat M1 to work in the EU can be a bit tricky.  Network availability may vary per country and the technology is being refined while it it being rolled out.  Particle has done a great job on rolling out Cat M1 support in the US.  In Europe, Particle has elected to use Cat 1 instead of Cat M1 due to its maturity with respect to technology and having roaming agreements in place.

However, Cat M1 is being rolled out in Europe and it is very doable to get Particle's hardware to work here as well, but it does require you to customize the setup of the hardware a bit to fit the settings for your region.

This project is my simplistic toolbox that I developed to familiarize myself with the technology before deploying thousands of Cat M1 devices here in Norway using Particle devices during 2020/2021.

This project was originally developed while using a special version of the Particle Electron featuring a uBlox R410M modem, with external SIM support.  I have mostly used deviceOS 1.5.2 as part of the deployments I have worked on, as the Cat M1 support in there seems solid for Electron. Earlier versions seemed to lack solid support for LTE Cat M1.

This project will most likely work on the Boron as well, as long as it is configured for the use of an external sim.

This project was originally developed using the old Particle IDE, which was based on Atom.

I have made use of a library called CellularHelper, which Rickkas7 from Particle has kindly made available on github.  

https://github.com/rickkas7/CellularHelper

I used his code as a starting point for my own customizations to add the extra functionality needed to support Cat M1 modems in Europe, using a 3rd party SIM.

Enjoy.


