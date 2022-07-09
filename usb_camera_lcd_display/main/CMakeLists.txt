
idf_component_register(
    SRCS 
        main.cpp
    INCLUDE_DIRS 
        .
    REQUIRES driver LovyanGFX spiffs bus uvc_stream decoder_ijg
)