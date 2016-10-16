PLUGIN_NAME = dynamic_clamp_file

RTXI_INCLUDES = 

HEADERS = dynamic-clamp-file.h\

SOURCES = dynamic-clamp-file.cpp \
          moc_dynamic-clamp-file.cpp\

LIBS = -lqwt -lrtplot

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
