
To relocate to RAM:
west build -b rd_rw612_bga -p always samples/boards/rd_rw612_bga/hello_world_program_location \
-DEXTRA_DTC_OVERLAY_FILE=ram_program.overlay -DCONFIG_NXP_RW_ROM_RAMLOADER=y

The output should be hello world with the address of the main function (in either FLASH or RAM depending on the configuration)