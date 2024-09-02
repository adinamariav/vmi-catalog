# vmi-catalog
Catalog for applications that make use of VMI (Virtual Machine Introspection) using a custom [fork](https://github.com/adinamariav/libvmi-unikraft) of `libvmi` with Unikraft support.

## Table of Contents

- [Setting up KVM-VMI](#setting-up-kvm-vmi)
- [Installing and configuring LibVMI](#installing-and-configuring-libvmi)
- [Running a Unikraft App with Libvirt](#running-a-unikraft-app-with-libvirt)

## Setting up KVM-VMI

The KVM-VMI setup is essential for enabling `libvmi` to introspect your virtual machines. Please follow the detailed setup instructions provided in the [KVM-VMI setup guide](https://kvm-vmi.github.io/kvm-vmi/master/setup.html).

Make sure `KVM-VMI`  and all its components are functioning correctly before proceeding to the next steps.

## Installing and configuring LibVMI

Once KVM-VMI is set up, proceed to install the  [`libvmi` fork tailored for Unikraft](https://github.com/adinamariav/libvmi-unikraft/).

   ```bash
   git clone https://github.com/adinamariav/libvmi-unikraft.git
   cd libvmi-unikraft
   mkdir build
   cd build
   cmake ..
   make
   sudo make install
   ```


## Running a Unikraft app with Libvirt

For VMI to work on a Unikraft applications, you need to start the Unikraft application using `libvirt`. Follow the detailed steps in this [guide](https://hackmd.io/tQMqDat7S5m3vSQE0XcN0w) to ensure it is properly configured and running. Do not forget to add the VMI-specific modifications to the `XML` file detailed [here](https://kvm-vmi.github.io/kvm-vmi/master/setup.html#preparing-a-domain).