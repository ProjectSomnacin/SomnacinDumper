#!/bin/sh
make && /bin/cp payload_abl0.h ../../PicoModchip/payloads/payload_abl0.h && make -C ../../PicoModchip/bb && picotool load -f ../../PicoModchip/bb/PicoModchip.uf2 && picotool reboot -f
