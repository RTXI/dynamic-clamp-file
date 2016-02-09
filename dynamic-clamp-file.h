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
 * DClampFile applies a conductance waveform that has already been saved in 
 * ASCII format. It uses the current real-time period to determine the length 
 * of the trial, sampling one row from the ASCII file at each time step. If 
 * you use it with the Spike Detector module, you can view a raster plot in 
 * real-time of spike times for each trial.
 */

#include <default_gui_model.h>
#include <scatterplot.h>
#include <plotdialog.h>
#include <basicplot.h>
//#include <RTXIprintfilter.h>
#include <cstdlib>

class DClamp : public DefaultGUIModel {

	Q_OBJECT

	public:
		DClamp(void);
		virtual~DClamp(void);

		void execute(void);
		void customizeGUI(void);

	public slots:

	signals:
		void setPlotRange(double newminamp, double newmaxamp, double minHz, double maxHz);
		void newDataPoint(double newx, double newy, QwtSymbol::Style s);
		void saveImage(QString fileName);

	protected:
		virtual void update(DefaultGUIModel::update_flags_t);

	private:
		// inputs, states, calculated values
		double systime;
		double Vm;
		double dt;
		double length;
	
		// parameters
		double wait;
		double Ihold;
		double repeat;
		double Erev;
		double gain;
		QString gFile;
	
		// options
		bool plotRaster;
	
		// bookkeeping
		double totaltime;
		std::vector<double> wave; // conductance waveform
		double spktime;
		int trial;
		long long count;
		int spikecount;
		double spikestate;
		int idx;
	
		// plotting
		double yrangemin;
		double yrangemax;
	
		// QT components
		ScatterPlot *rplot;

		// Clamp functions
		void initParameters();
		void bookkeep();

		// Functions and parameters for saving data to file without using data recorder
		bool OpenFile(QString);
		QFile dataFile;
		QDataStream stream;

	private slots:
		void togglePlot(bool);
		void print();
		void exportSVG();
		void loadFile();
		void loadFile(QString);
		void previewFile();
		void adjustPlot();
};
