PLUGIN_NAME = dynamic_clamp_file

RTXI_INCLUDES=/usr/local/lib/rtxi_includes

HEADERS = dynamic-clamp-file.h\
          $(RTXI_INCLUDES)/scatterplot.h\
          $(RTXI_INCLUDES)/incrementalplot.h\
          $(RTXI_INCLUDES)/basicplot.h\
          $(RTXI_INCLUDES)/scrollzoomer.h\
          $(RTXI_INCLUDES)/scrollbar.h\
          $(RTXI_INCLUDES)/plotdialog.h\

SOURCES = dynamic-clamp-file.cpp \
          moc_dynamic-clamp-file.cpp\
          $(RTXI_INCLUDES)/scatterplot.cpp\
          $(RTXI_INCLUDES)/incrementalplot.cpp\
          $(RTXI_INCLUDES)/basicplot.cpp\
          $(RTXI_INCLUDES)/scrollzoomer.cpp\
          $(RTXI_INCLUDES)/scrollbar.cpp\
          $(RTXI_INCLUDES)/plotdialog.cpp\
          $(RTXI_INCLUDES)/moc_scatterplot.cpp\
          $(RTXI_INCLUDES)/moc_incrementalplot.cpp\
          $(RTXI_INCLUDES)/moc_basicplot.cpp\
          $(RTXI_INCLUDES)/moc_scrollzoomer.cpp\
          $(RTXI_INCLUDES)/moc_scrollbar.cpp\
          $(RTXI_INCLUDES)/moc_plotdialog.cpp\

LIBS = -lqwt

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
