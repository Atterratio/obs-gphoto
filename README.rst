==========
obs-gphoto
==========

Allows capture DSLR cameras preview through gPhoto in obs-studio on Linux. At now tested only Canon cameras.

.. image:: https://img.shields.io/badge/Donate-PayPal-blue.svg
   :target: https://www.paypal.me/AeternusAtterratio
.. image:: https://img.shields.io/badge/Donate-YaMoney-orange.svg
   :target: https://money.yandex.ru/to/410011005689134


------
v0.0.1
------

REQUIREMENTS
============

* *obs-studio*
* *libgphoto >= 2.5.10*
* *libmagickcore*
* *libudev(optional)*

INSTALLATION
============

* :code:`cmake . -DCMAKE_BUILD_TYPE=Release` for local installation or :code:`cmake . -DCMAKE_BUILD_TYPE=Release -DSYSTEM_INSTALL=1` for system installation;
* :code:`make`;
* :code:`make install`;