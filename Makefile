PLUGIN_NAME = DClampFile

HEADERS = DClampFile.h\
          include/basicplot.h\
          include/incrementalplot.h\
          include/scatterplot.h\
          include/scrollbar.h\
          include/scrollzoomer.h\
          include/plotdialog.h\

SOURCES = DClampFile.cpp \
          moc_DClampFile.cpp\
          include/basicplot.cpp\
          include/scatterplot.cpp\
          include/incrementalplot.cpp\
          include/scrollbar.cpp\
          include/scrollzoomer.cpp\
          include/plotdialog.cpp\
          include/moc_scatterplot.cpp\
          include/moc_incrementalplot.cpp\
          include/moc_scrollbar.cpp\
          include/moc_scrollzoomer.cpp\
          include/moc_basicplot.cpp\
          include/moc_plotdialog.cpp

LIBS = -lqwt

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
