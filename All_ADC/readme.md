for running this

west build -p -b nucleo_n657x0_q /home/shri/nuc_projects \
  -- -DDTC_OVERLAY_FILE=/home/shri/nuc_projects/boards/nucleo_n657x0_q.overlay

west build -p always -b nucleo_n657x0_q .

screen /dev/ttyACM0 115200