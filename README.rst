==========
obs-gphoto
==========

Allows connect DSLR cameras with obs-studio through gPhoto on Linux. At now tested only Canon cameras.

.. image:: https://img.shields.io/badge/Donate-PayPal-blue.svg
    :target: https://www.paypal.me/AeternusAtterratio
.. image:: https://img.shields.io/badge/Donate-YaMoney-orange.svg
    :target: https://money.yandex.ru/to/410011005689134


------
v0.3.0
------

MODULES
=======
gPhoto live preview capture
---------------------------
   Allows capture cameras preview like video.

Timelapse photo capture
-----------------------
   Allows capture photo with some intervals(if interval set to 0 work only manual capture) or manual with hotkey and camera capture button, to show work in progress on good picture quality, or to compile timelapse video in future.

REQUIREMENTS
============

* *obs-studio*
* *libgphoto >= 2.5.10*
* *libmagickcore*
* *libudev(optional)*

INSTALLATION
============

For ArchLinux:
--------------

    there is a `package in AUR`_.
        .. _`package in AUR`: https://aur.archlinux.org/packages/obs-gphoto/

For installation from source:
-----------------------------
* :code:`cmake . -DSYSTEM_INSTALL=0` for local installation or :code:`cmake . -DSYSTEM_INSTALL=1` for system installation;
* :code:`make`;
* :code:`make install`.