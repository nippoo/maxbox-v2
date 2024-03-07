# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.c"
				   "led.c"
				   "touch.c"
				   "vehicle.c"
				   "telemetry.c"
				   "rc522.c"
				   "owb.c"
				   "owb_rmt.c"
				   "lp50xx.c"
				   "ltr303.c"
				   "sim7600.c"
				   "lorawan.c"
				   "wifi.c"
				   "http.c"
				   "flash.c"
				   "state.c")
				   
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE  -Wno-format)