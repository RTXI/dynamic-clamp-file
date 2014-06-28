/*
 Copyright (C) 2011 Georgia Institute of Technology

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

/*
 * DClampFile applies a conductance waveform that has already been saved in ASCII format. It uses the current
 * real-time period to determine the length of the trial, sampling one row from the ASCII file at each time step.
 * If you use it with the SpikeDetect module, you can view a raster plot in real-time of spike times for each trial.
 */

#include <DClampFile.h>
#include <../include/basicplot.h>
//#include <main_window.h>
#include <math.h>
#include <algorithm>
#include <qhbox.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qgridview.h>
#include <qhbuttongroup.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qvalidator.h>
#include <qdatetime.h>
#include <qfile.h>
#include <qpicture.h>
#include <qregexp.h>
#include <qprinter.h>
#include <qprintdialog.h>
#include <qpainter.h>
#if QT_VERSION >= 0x040300
#ifdef QT_SVG_LIB
#include <qsvggenerator.h>
#endif
#endif
#if QT_VERSION >= 0x040000
#include <qprintdialog.h>
#include <qfileinfo.h>
#else
#include <qwt-qt3/qwt_painter.h>
#endif
#include <qwt-qt3/qwt_array.h>
#include <sys/stat.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics_double.h>

extern "C" Plugin::Object *
createRTXIPlugin(void)
{
  return new DClamp();
}

// inputs, outputs, parameters for default_gui_model
static DefaultGUIModel::variable_t
    vars[] =
      {
            { "Vm (mV)", "Membrane Potential", DClamp::INPUT, },
            { "Spike State", "Spike State", DClamp::INPUT, },
            { "Command", "Command", DClamp::OUTPUT, },
            {
                "Length (s)",
                "Length of trial is computed from the real-time period and the size of your conductance waveform file",
                DClamp::STATE, },
            {
                "File Name",
                "ASCII file containing conductance waveform with values in siemens",
                DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Reversal Potential (mV)",
                "Reversal Potential (mV) for artificial conductance",
                DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Gain", "Gain to multiply conductance values by",
                DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Wait time (s)",
                "Time to wait between trials of applied artifical conductance",
                DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Holding Current (s)",
                "Current to inject while waiting between trials",
                DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Repeat", "Number of trials", DClamp::PARAMETER | DClamp::DOUBLE, },
            { "Time (s)", "Time (s)", DClamp::STATE, }, };

// some necessary variable DO NOT EDIT
static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// Default constructor
DClamp::DClamp(void) :
  DefaultGUIModel("Dynamic Clamp", ::vars, ::num_vars)
{
  QWhatsThis::add(
      this,
      "<p><b>Dynamic Clamp:</b></p><p>This module applies a conductance waveform that "
      "has already been saved in ASCII format. It uses the current real-time period to "
      "determine the length of the trial, sampling one row from the ASCII file at each time step."
      " If you use it with the SpikeDetect module, you can view a raster plot in real-time of"
      "spike times for each trial.</p>");
  initParameters();
  createGUI(vars, num_vars); // this is required to create the GUI
  update(INIT);
  refresh(); // this is required to update the GUI with parameter and state values
  printf("Loaded Dynamic Clamp:\n");

}

void
DClamp::createGUI(DefaultGUIModel::variable_t *var, int size)
{
  setMinimumSize(200, 300);

  QBoxLayout *layout = new QHBoxLayout(this); // overall GUI layout

  // create custom GUI components
  QBoxLayout *rightlayout = new QVBoxLayout();
  QHButtonGroup *plotBox = new QHButtonGroup("Raster Plot:", this);
  QPushButton *clearButton = new QPushButton("&Clear", plotBox);
  QPushButton *savePlotButton = new QPushButton("Save Screenshot", plotBox);
  QPushButton *printButton = new QPushButton("Print", plotBox);

  rplot = new ScatterPlot(this);

  QObject::connect(clearButton, SIGNAL(clicked()), rplot, SLOT(clear()));
  QObject::connect(clearButton, SIGNAL(clicked()), this, SLOT(adjustPlot()));
  QObject::connect(savePlotButton, SIGNAL(clicked()), this, SLOT(exportSVG()));
  QObject::connect(printButton, SIGNAL(clicked()), this, SLOT(print()));
  QToolTip::add(clearButton, "Clear");
  QToolTip::add(savePlotButton, "Save screenshot");
  QToolTip::add(printButton, "Print plot");

  rightlayout->addWidget(plotBox);
  rightlayout->addWidget(rplot);

  rplot->setMinimumSize(540, 300);

  QBoxLayout *leftlayout = new QVBoxLayout();

  QHButtonGroup *fileBox = new QHButtonGroup("File:", this);
  QPushButton *loadBttn = new QPushButton("Load File", fileBox);
  QPushButton *previewBttn = new QPushButton("Preview File", fileBox);
  QObject::connect(loadBttn, SIGNAL(clicked()), this, SLOT(loadFile()));
  QObject::connect(previewBttn, SIGNAL(clicked()), this, SLOT(previewFile()));

  QVBox *optionBox = new QVBox(this);
  QHBox *optionRow1 = new QHBox(optionBox);
  QCheckBox *plotCheckBox = new QCheckBox("Raster Plot", optionRow1);
  plotCheckBox->setChecked(true);
  QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), rplot,
      SLOT(setShown(bool)));
  QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), plotBox,
      SLOT(setShown(bool)));
  QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), this,
      SLOT(togglePlot(bool)));
  QToolTip::add(plotCheckBox, "Show/Hide raster plot");

  QHBox *utilityBox = new QHBox(this);
  pauseButton = new QPushButton("Pause", utilityBox);
  pauseButton->setToggleButton(true);
  QObject::connect(pauseButton, SIGNAL(toggled(bool)), this,
      SLOT(pause(bool)));
  QObject::connect(pauseButton, SIGNAL(toggled(bool)), savePlotButton,
      SLOT(setEnabled(bool)));
  QObject::connect(pauseButton, SIGNAL(toggled(bool)), printButton,
      SLOT(setEnabled(bool)));
  QPushButton *modifyButton = new QPushButton("Modify", utilityBox);
  QObject::connect(modifyButton, SIGNAL(clicked(void)), this,
      SLOT(modify(void)));
  QPushButton *unloadButton = new QPushButton("Unload", utilityBox);
  QObject::connect(unloadButton, SIGNAL(clicked(void)), this,
      SLOT(exit(void)));
  QObject::connect(pauseButton, SIGNAL(toggled(bool)), modifyButton,
      SLOT(setEnabled(bool)));
  QToolTip::add(pauseButton, "Start/Stop dynamic clamp protocol");
  QToolTip::add(modifyButton, "Commit changes to parameter values");
  QToolTip::add(unloadButton, "Close plugin");
  QObject::connect(this,
      SIGNAL(newDataPoint(double,double,QwtSymbol::Style)), rplot,
      SLOT(appendPoint(double,double,QwtSymbol::Style)));
  QObject::connect(this,
      SIGNAL(setPlotRange(double, double, double, double)), rplot,
      SLOT(setAxes(double, double, double, double)));

  // add custom GUI components to layout above default_gui_model components
  leftlayout->addWidget(fileBox);

  // create default_gui_model GUI DO NOT EDIT
  QScrollView *sv = new QScrollView(this);
  sv->setResizePolicy(QScrollView::AutoOne);
  sv->setHScrollBarMode(QScrollView::AlwaysOff);
  leftlayout->addWidget(sv);

  QWidget *viewport = new QWidget(sv->viewport());
  sv->addChild(viewport);
  QGridLayout *scrollLayout = new QGridLayout(viewport, 1, 2);

  size_t nstate = 0, nparam = 0, nevent = 0, ncomment = 0;
    for (size_t i = 0; i < num_vars; i++)
      {
        if (vars[i].flags & (PARAMETER | STATE | EVENT | COMMENT))
          {
            param_t param;

            param.label = new QLabel(vars[i].name, viewport);
            scrollLayout->addWidget(param.label, parameter.size(), 0);
            param.edit = new DefaultGUILineEdit(viewport);
            scrollLayout->addWidget(param.edit, parameter.size(), 1);

            QToolTip::add(param.label, vars[i].description);
            QToolTip::add(param.edit, vars[i].description);

            if (vars[i].flags & PARAMETER)
              {
                if (vars[i].flags & DOUBLE)
                  {
                    param.edit->setValidator(new QDoubleValidator(param.edit));
                    param.type = PARAMETER | DOUBLE;
                  }
                else if (vars[i].flags & UINTEGER)
                  {
                    QIntValidator *validator = new QIntValidator(param.edit);
                    param.edit->setValidator(validator);
                    validator->setBottom(0);
                    param.type = PARAMETER | UINTEGER;
                  }
                else if (vars[i].flags & INTEGER)
                  {
                    param.edit->setValidator(new QIntValidator(param.edit));
                    param.type = PARAMETER | INTEGER;
                  }
                else
                  param.type = PARAMETER;
                param.index = nparam++;
                param.str_value = new QString;
              }
            else if (vars[i].flags & STATE)
              {
                param.edit->setReadOnly(true);
                param.edit->setPaletteForegroundColor(Qt::darkGray);
                param.type = STATE;
                param.index = nstate++;
              }
            else if (vars[i].flags & EVENT)
              {
                param.edit->setReadOnly(true);
                param.type = EVENT;
                param.index = nevent++;
              }
            else if (vars[i].flags & COMMENT)
              {
                param.type = COMMENT;
                param.index = ncomment++;
              }

            parameter[vars[i].name] = param;
          }
      }

  // end default_gui_model GUI DO NOT EDIT

  // add custom components to layout below default_gui_model components
  leftlayout->addWidget(optionBox);
  leftlayout->addWidget(utilityBox);
  layout->addLayout(leftlayout);
  layout->addLayout(rightlayout);
  leftlayout->setResizeMode(QLayout::Fixed);
  layout->setStretchFactor(rightlayout, 4);
  layout->setResizeMode(QLayout::FreeResize);

  // set GUI refresh rate
  QTimer *timer = new QTimer(this);
  timer->start(1000);
  QObject::connect(timer, SIGNAL(timeout(void)), this, SLOT(refresh(void)));
  show();


}

DClamp::~DClamp(void)
{
}

void
DClamp::execute(void)
{
  Vm = input(0);
  systime = count * dt; // current time, s
  if (plotRaster == true)
    {
      if (input(1) == 1)
        emit newDataPoint(systime - (totaltime * trial), double(trial + 1),
            QwtSymbol::VLine);
    }

  if (trial < repeat)
    {
      output(0) = -1 * wave[idx++] * (Vm - Erev) * gain;
    }
  else
    { // all cycles are done
      pause(true);
    } // end if trial
  count++; // increment count to measure time
  if (systime > totaltime * (trial + 1))
    {
      trial++;
      idx = 0;
    }
}

void
DClamp::update(DClamp::update_flags_t flag)
{
  switch (flag)
    {
  case INIT:
    setState("Length (s)", length); // initialized in s, display in s
    setParameter("File Name", gFile);
    setParameter("Reversal Potential (mV)", QString::number(Erev * 1000)); // convert from V to mV
    setParameter("Gain", QString::number(gain));
    setParameter("Wait time (s)", QString::number(wait));
    setParameter("Holding Current (s)", QString::number(Ihold * 1e-9)); // convert from A to nA
    setParameter("Repeat", QString::number(repeat)); // initially 1
    setState("Time (s)", systime);
    break;
  case MODIFY:
    gFile = getParameter("File Name");
    Erev = getParameter("Reversal Potential (mV)").toDouble() / 1000; // convert from mV to V
    gain = getParameter("Gain").toDouble();
    wait = getParameter("Wait time (s)").toDouble();
    Ihold = getParameter("Holding Current (s)").toDouble() * 1e9; // convert from nA to A
    repeat = getParameter("Repeat").toDouble();
    loadFile(gFile);
    bookkeep();
    break;
  case PAUSE:
    output(0) = 0; // stop command in case pause occurs in the middle of command
    printf("Protocol paused.\n");
    break;
  case UNPAUSE:
    bookkeep();
    printf("Starting protocol.\n");
    break;
  case PERIOD:
    dt = RT::System::getInstance()->getPeriod() * 1e-9;
    printf("New real-time period: %f\n", dt);
    loadFile(gFile);
  default:
    break;
    }
}

// custom functions 

void
DClamp::initParameters()
{
  length = 1; // seconds
  repeat = 1;
  Ihold = 0; // Amps
  wait = 1; // seconds
  gain = 1;
  Erev = -.070; // V
  dt = RT::System::getInstance()->getPeriod() * 1e-9; // s
  yrangemin = 0;
  yrangemax = 10;
  gFile = "No file loaded.";
  togglePlot(true);
  bookkeep();
}

void
DClamp::bookkeep()
{
  trial = 0;
  count = 0;
  systime = 0;
  spikestate = 0;
  spktime = 0;
  idx = 0;
  totaltime = length + wait;
}

void
DClamp::print()
{
#if 1
  QPrinter printer;
#else
  QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION < 0x040000
  printer.setOutputToFile(true);
  printer.setOutputFileName("/tmp/DClamp.ps");
  printer.setColorMode(QPrinter::Color);
#else
  printer.setOutputFileName("/tmp/DClamp.pdf");
#endif
#endif

  QString docName = rplot->title().text();
  if (!docName.isEmpty())
    {
      docName.replace(QRegExp(QString::fromLatin1("\n")), tr(" -- "));
      printer.setDocName(docName);
    }

  printer.setCreator("RTXI");
  printer.setOrientation(QPrinter::Landscape);

#if QT_VERSION >= 0x040000
  QPrintDialog dialog(&printer);
  if ( dialog.exec() )
    {
#else
  if (printer.setup())
    {
#endif
      RTXIPrintFilter filter;
      if (printer.colorMode() == QPrinter::GrayScale)
        {
          int options = QwtPlotPrintFilter::PrintAll;
          filter.setOptions(options);
          filter.color(QColor(29, 100, 141),
              QwtPlotPrintFilter::CanvasBackground); // change to white
          filter.color(Qt::white, QwtPlotPrintFilter::CurveSymbol); // change to black
        }
      rplot->print(printer, filter);
    }
}

void
DClamp::exportSVG()
{
  QString fileName = "DClamp.svg";

#if QT_VERSION < 0x040000

#ifndef QT_NO_FILEDIALOG
  fileName = QFileDialog::getSaveFileName("DClamp.svg",
      "SVG Documents (*.svg)", this);
#endif
  if (!fileName.isEmpty())
    {
      // enable workaround for Qt3 misalignments
      QwtPainter::setSVGMode(true);
      QPicture picture;
      QPainter p(&picture);
      rplot->print(&p, QRect(0, 0, 800, 600));
      p.end();
      picture.save(fileName, "svg");
    }

#elif QT_VERSION >= 0x040300

#ifdef QT_SVG_LIB
#ifndef QT_NO_FILEDIALOG
  fileName = QFileDialog::getSaveFileName(
      this, "Export File Name", QString(),
      "SVG Documents (*.svg)");
#endif
  if ( !fileName.isEmpty() )
    {
      QSvgGenerator generator;
      generator.setFileName(fileName);
      generator.setSize(QSize(800, 600));
      rplot->print(generator);
    }
#endif
#endif
}

void
DClamp::togglePlot(bool on)
{
  plotRaster = on;
  adjustSize();
}

void
DClamp::loadFile()
{
  QFileDialog* fd = new QFileDialog(this, "Conductance waveform file", TRUE);
  fd->setMode(QFileDialog::AnyFile);
  fd->setViewMode(QFileDialog::Detail);
  QString fileName;
  if (fd->exec() == QDialog::Accepted)
    {
      fileName = fd->selectedFile();
      printf("Loading new file: %s\n", fileName.latin1());
      setParameter("File Name", fileName);
      wave.clear();
      QFile file(fileName);
      if (file.open(IO_ReadOnly))
        {
          QTextStream stream(&file);
          double value;
          while (!stream.atEnd())
            {
              stream >> value;
              wave.push_back(value);
            }
        }
      length = wave.size() * dt;
      setState("Length (s)", length); // initialized in s, display in s
      adjustPlot();
      // pad waveform with holding current to account for wait between trials
      for (int i = 0; i < wait / dt; i++)
        {
          wave.push_back(Ihold);
        }
    }
  else
    {
      setParameter("File Name", "No file loaded.");
    }
}

void
DClamp::loadFile(QString fileName)
{
  if (fileName == "No file loaded.")
    {
      return;
    }
  else
    {
      printf("Loading new file: %s\n", fileName.latin1());
      wave.clear();
      QFile file(fileName);
      if (file.open(IO_ReadOnly))
        {
          QTextStream stream(&file);
          double value;
          while (!stream.atEnd())
            {
              stream >> value;
              wave.push_back(value);
            }
        }
      length = wave.size() * dt;
      setState("Length (s)", length); // initialized in s, display in s
      adjustPlot();
      // pad waveform with holding current to account for wait between trials
      for (int i = 0; i < wait / dt; i++)
        {
          wave.push_back(Ihold);
        }
    }
}

void
DClamp::adjustPlot()
{
  if (repeat > 10)
    emit setPlotRange(0, length, 0, repeat + 1);
  else
    emit setPlotRange(0, length, 0, 11);
}

void
DClamp::previewFile()
{
  double* time = new double[static_cast<int> (wave.size())];
  double* yData = new double[static_cast<int> (wave.size())];
  for (int i = 0; i < wave.size(); i++)
    {
      time[i] = dt * i;
      yData[i] = wave[i];
    }
  PlotDialog *preview = new PlotDialog(this, "Preview Conductance Waveform",
      time, yData, wave.size());

  preview->show();

}

bool
DClamp::OpenFile(QString FName)
{
  dataFile.setName(FName);
  if (dataFile.exists())
    {
      switch (QMessageBox::warning(this, "Dynamic Clamp", tr(
          "This file already exists: %1.\n").arg(FName), "Overwrite", "Append",
          "Cancel", 0, 2))
        {
      case 0: // overwrite
        dataFile.remove();
        if (!dataFile.open(IO_Raw | IO_WriteOnly))
          {
            return false;
          }
        break;
      case 1: // append
        if (!dataFile.open(IO_Raw | IO_WriteOnly | IO_Append))
          {
            return false;
          }
        break;
      case 2: // cancel
        return false;
        break;
        }
    }
  else
    {
      if (!dataFile.open(IO_Raw | IO_WriteOnly))
        return false;
    }
  stream.setDevice(&dataFile);
  stream.setPrintableData(false); // write binary
  printf("File opened: %s\n", FName.latin1());
  return true;
}

