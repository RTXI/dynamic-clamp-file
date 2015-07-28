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

#include <dynamic-clamp-file.h>
#include <iostream>
#include <basicplot.h>
#include <main_window.h>
#include <math.h>
#include <algorithm>
#include <QPainter>
#include <QSvgGenerator>
#include <QFileInfo>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <sys/stat.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics_double.h>

extern "C" Plugin::Object * createRTXIPlugin(void) {
	return new DClamp();
}

// inputs, outputs, parameters for default_gui_model
static DefaultGUIModel::variable_t vars[] = 
{
	{ "Vm (mV)", "Membrane Potential", DClamp::INPUT, },
	{ "Spike State", "Spike State", DClamp::INPUT, },
	{ "Command", "Command", DClamp::OUTPUT, },
	{ "Length (s)", "Length of trial is computed from the real-time period and the size of your conductance waveform file",	DClamp::STATE, },
	{ "File Name", "ASCII file containing conductance waveform with values in siemens",	DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Reversal Potential (mV)", "Reversal Potential (mV) for artificial conductance", DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Gain", "Gain to multiply conductance values by", DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Wait time (s)", "Time to wait between trials of applied artifical conductance", DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Holding Current (s)", "Current to inject while waiting between trials", DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Repeat", "Number of trials", DClamp::PARAMETER | DClamp::DOUBLE, },
	{ "Time (s)", "Time (s)", DClamp::STATE, }, 
};

// some necessary variable DO NOT EDIT
static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// Default constructor
DClamp::DClamp(void) : DefaultGUIModel("Dynamic Clamp", ::vars, ::num_vars) {
	setWhatsThis(
		"<p><b>Dynamic Clamp:</b></p><p>This module applies a conductance waveform that "
		"has already been saved in ASCII format. It uses the current real-time period to "
		"determine the length of the trial, sampling one row from the ASCII file at each time step."
		" If you use it with the SpikeDetect module, you can view a raster plot in real-time of"
		"spike times for each trial.</p>");
	initParameters();
	createGUI(vars, num_vars); // this is required to create the GUI
	update(INIT);
	customizeGUI();
	refresh(); // this is required to update the GUI with parameter and state values
	QTimer::singleShot(0, this, SLOT(resizeMe()));
	printf("Loaded Dynamic Clamp:\n");
}

void DClamp::customizeGUI(void) {
	
	QGridLayout *customLayout = DefaultGUIModel::getLayout();
	
	// create custom GUI components
	QGroupBox *plotBox = new QGroupBox("Raster Plot:");
	QHBoxLayout *plotBoxLayout = new QHBoxLayout;
	plotBox->setLayout(plotBoxLayout);
	QPushButton *clearButton = new QPushButton("&Clear");
	clearButton->setToolTip("Clear");
	QPushButton *savePlotButton = new QPushButton("Screenshot");
	savePlotButton->setToolTip("Save a screenshot");
	QPushButton *printButton = new QPushButton("Print");
	printButton->setToolTip("Print the plot");
	plotBoxLayout->addWidget(clearButton);
	plotBoxLayout->addWidget(savePlotButton);
	plotBoxLayout->addWidget(printButton);
	QObject::connect(clearButton, SIGNAL(clicked()), this, SLOT(adjustPlot()));
	QObject::connect(savePlotButton, SIGNAL(clicked()), this, SLOT(exportSVG()));
	QObject::connect(printButton, SIGNAL(clicked()), this, SLOT(print()));
	
	rplot = new ScatterPlot(this);
	QObject::connect(clearButton, SIGNAL(clicked()), rplot, SLOT(clear()));
	rplot->setMinimumSize(540, 300);

	customLayout->addWidget(plotBox, 0, 1);
	customLayout->addWidget(rplot, 1, 1, 11, 1);

	QGroupBox *fileBox = new QGroupBox("File:");
	QHBoxLayout *fileBoxLayout = new QHBoxLayout;
	fileBox->setLayout(fileBoxLayout);
	QPushButton *loadBttn = new QPushButton("Load File");
	QPushButton *previewBttn = new QPushButton("Preview File");
	fileBoxLayout->addWidget(loadBttn);
	fileBoxLayout->addWidget(previewBttn);
	QObject::connect(loadBttn, SIGNAL(clicked()), this, SLOT(loadFile()));
	QObject::connect(previewBttn, SIGNAL(clicked()), this, SLOT(previewFile()));
	customLayout->addWidget(fileBox, 0, 0);

	QGroupBox *optionBox = new QGroupBox;
	QHBoxLayout *optionRow1 = new QHBoxLayout;
	optionBox->setLayout(optionRow1);
	QCheckBox *plotCheckBox = new QCheckBox("Raster Plot");
	optionRow1->addWidget(plotCheckBox);
	plotCheckBox->setChecked(true);
	QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), rplot, SLOT(setShown(bool)));
	QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), plotBox, SLOT(setShown(bool)));
	QObject::connect(plotCheckBox, SIGNAL(toggled(bool)), this, SLOT(togglePlot(bool)));
	plotCheckBox->setToolTip("Show/Hide raster plot");
	customLayout->addWidget(optionBox, 2, 0);

	QObject::connect(pauseButton, SIGNAL(toggled(bool)), savePlotButton, SLOT(setEnabled(bool)));
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), printButton, SLOT(setEnabled(bool)));
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), modifyButton, SLOT(setEnabled(bool)));
	pauseButton->setToolTip("Start/Stop dynamic clamp protocol");
	modifyButton->setToolTip("Commit changes to parameter values");
	unloadButton->setToolTip("Close plugin");
	
	QObject::connect(this, SIGNAL(newDataPoint(double,double,QwtSymbol::Style)), rplot, SLOT(appendPoint(double,double,QwtSymbol::Style)));
	QObject::connect(this, SIGNAL(setPlotRange(double, double, double, double)), rplot, SLOT(setAxes(double, double, double, double)));

	setLayout(customLayout);
}

DClamp::~DClamp(void) {}

void DClamp::execute(void) {
	Vm = input(0);
	systime = count * dt; // current time, s

	if (plotRaster == true) {
		if (input(1) == 1) {
			emit newDataPoint(systime - (totaltime * trial), double(trial + 1), QwtSymbol::VLine);
		}
	}

	if (trial < repeat) {
		output(0) = -1 * wave[idx++] * (Vm - Erev) * gain;
	}
	else { // all cycles are done
		pause(true);
	} // end if trial
	count++; // increment count to measure time
	if (systime > totaltime * (trial + 1)) {
		trial++;
		idx = 0;
	}
}

void DClamp::update(DClamp::update_flags_t flag) {
	switch (flag) {
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
		break;

	default:
		break;
	}
}

// custom functions 

void DClamp::initParameters() {
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

void DClamp::bookkeep() {
	trial = 0;
	count = 0;
	systime = 0;
	spikestate = 0;
	spktime = 0;
	idx = 0;
	totaltime = length + wait;
}

void DClamp::print() {
#if 1
	QPrinter printer;
#else
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFileName("/tmp/DClamp.pdf");
#endif

	QString docName = rplot->title().text();
	if (!docName.isEmpty()) {
		docName.replace(QRegExp(QString::fromLatin1("\n")), tr(" -- "));
		printer.setDocName(docName);
	}

	printer.setCreator("RTXI");
	printer.setOrientation(QPrinter::Landscape);

	QPrintDialog dialog(&printer);
	if ( dialog.exec() ) {
/*
		RTXIPrintFilter filter;
		if (printer.colorMode() == QPrinter::GrayScale) {
			int options = QwtPlotPrintFilter::PrintAll;
			filter.setOptions(options);
			filter.color(QColor(29, 100, 141), QwtPlotPrintFilter::CanvasBackground); // change to white
			filter.color(Qt::white, QwtPlotPrintFilter::CurveSymbol); // change to black
		}
		rplot->print(printer, filter);
*/
	}
}

void DClamp::exportSVG() {
	QString fileName = "DClamp.svg";

#ifdef QT_SVG_LIB
#ifndef QT_NO_FILEDIALOG
	fileName = QFileDialog::getSaveFileName(this, "Export File Name", QString(),"SVG Documents (*.svg)");
#endif
	if ( !fileName.isEmpty() ) {
		QSvgGenerator generator;
		generator.setFileName(fileName);
		generator.setSize(QSize(800, 600));
		rplot->print(generator);
	}
#endif
}

void DClamp::togglePlot(bool on) {
	plotRaster = on;
	adjustSize();
}

void DClamp::loadFile() {
	QFileDialog* fd = new QFileDialog(this, "Conductance waveform file");
	fd->setFileMode(QFileDialog::AnyFile);
	fd->setViewMode(QFileDialog::Detail);
	QString fileName;
	if (fd->exec() == QDialog::Accepted) {
		fileName = (fd->selectedFiles()).takeFirst();
//		printf("Loading new file: %s\n", fileName.toStdString());
		std::cout<<"Loading new file: "<<fileName.toStdString()<<std::endl;
		setParameter("File Name", fileName);
		wave.clear();
		QFile file(fileName);
		if (file.open(QIODevice::ReadOnly)) {
			QTextStream stream(&file);
			double value;
			while (!stream.atEnd()) {
				stream >> value;
				wave.push_back(value);
			}
		}
		length = wave.size() * dt;
		setState("Length (s)", length); // initialized in s, display in s
		adjustPlot();
		// pad waveform with holding current to account for wait between trials
		for (int i = 0; i < wait / dt; i++) {
			wave.push_back(Ihold);
		}
	}
	else {
		setParameter("File Name", "No file loaded.");
	}
}

void DClamp::loadFile(QString fileName) {
	if (fileName == "No file loaded.") {
		return;
	} else {
//		printf("Loading new file: %s\n", fileName.toStdString());
		std::cout<<"Loading new file: "<<fileName.toStdString()<<std::endl;
		wave.clear();
		QFile file(fileName);
		if (file.open(QIODevice::ReadOnly)) {
			QTextStream stream(&file);
			double value;
			while (!stream.atEnd()) {
				stream >> value;
				wave.push_back(value);
			}
		}
		length = wave.size() * dt;
		setState("Length (s)", length); // initialized in s, display in s
		adjustPlot();
		
		// pad waveform with holding current to account for wait between trials
		for (int i = 0; i < wait / dt; i++) {
			wave.push_back(Ihold);
		}
	}
}

void DClamp::adjustPlot() {
	if (repeat > 10) emit setPlotRange(0, length, 0, repeat + 1);
	else emit setPlotRange(0, length, 0, 11);
}

void DClamp::previewFile() {
	double* time = new double[static_cast<int> (wave.size())];
	double* yData = new double[static_cast<int> (wave.size())];
	for (int i = 0; i < wave.size(); i++) {
		time[i] = dt * i;
		yData[i] = wave[i];
	}
	PlotDialog *preview = new PlotDialog(this, "Preview Conductance Waveform",
	time, yData, wave.size());

	preview->show();
}

bool DClamp::OpenFile(QString FName) {
	dataFile.setFileName(FName);
	if (dataFile.exists()) {
		switch (QMessageBox::warning(this, "Dynamic Clamp", 
		                             tr("This file already exists: %1.\n").arg(FName), 
		                             "Overwrite", "Append", "Cancel", 0, 2)) {
		case 0: // overwrite
			dataFile.remove();
			if (!dataFile.open(QIODevice::Unbuffered | QIODevice::WriteOnly)) return false;
			break;

		case 1: // append
			if (!dataFile.open(QIODevice::Unbuffered | QIODevice::WriteOnly | QIODevice::Append)) return false;
			break;

		case 2: // cancel
			return false;
			break;
		}
	}
	else {
		if (!dataFile.open(QIODevice::Unbuffered | QIODevice::WriteOnly)) return false;
	}
	stream.setDevice(&dataFile);
//	stream.setPrintableData(false); // write binary
//	printf("File opened: %s\n", FName.toStdString());
	std::cout<<"File opened: "<<FName.toStdString()<<std::endl;
	return true;
}
