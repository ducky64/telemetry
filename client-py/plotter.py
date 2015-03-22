from __future__ import print_function
import ast
from collections import deque
import csv
import datetime 
import sys

if sys.version_info.major < 3:
  from Tkinter import *
  import tkSimpleDialog as simpledialog
else:
  from tkinter import *
  import tkinter.simpledialog as simpledialog
  
from matplotlib import pyplot as plt
import matplotlib.animation as animation
import numpy as np
import serial

from telemetry.parser import TelemetrySerial, DataPacket, HeaderPacket, NumericData, NumericArray

class BasePlot(object):
  """Base class / interface definition for telemetry plotter plots with a
  dependent variable vs. an scrolling independent variable (like time).
  """
  def __init__(self, subplot, indep_def, dep_def, indep_span):
    """Constructor.
    
    Arguments:
    subplot -- matplotlib subplot to draw stuff onto
    indep_def -- TelemetryData of the independent variable
    dep_def -- TelemetryData of the dependent variable
    indep_span -- the span of the independent variable to keep track of
    """
    self.indep_span = indep_span
    self.indep_id = indep_def.data_id
    self.dep_id = dep_def.data_id
    
    self.dep_def = dep_def
    
    if dep_def.limits[0] != dep_def.limits[1]:
      self.limits = dep_def.limits
    else:
      self.limits = None
      
    self.subplot = subplot
    
  def update_from_packet(self, packet):
    """Updates my internal data structures from a received packet. Should not do
    rendering - this may be called multiple times per visible update.
    """
    raise NotImplementedError 

  def update_show(self, packet):
    """Render my data. This is separated from update_from_packet to allow
    multiple packets to be processed while doing only one time-consuming render. 
    """
    raise NotImplementedError

  def get_name(self):
    return self.dep_def.display_name

  def get_dep_def(self):
    return self.dep_def

class NumericPlot(BasePlot):
  """A plot of a single numeric dependent variable vs. a single independent
  variable
  """
  def __init__(self, subplot, indep_def, dep_def, indep_span):
    super(NumericPlot, self).__init__(subplot, indep_def, dep_def, indep_span)
    self.line, = subplot.plot([0])
    
    self.indep_data = deque()
    self.dep_data = deque()
  
  def update_from_packet(self, packet):
    assert isinstance(packet, DataPacket)
    indep_val = packet.get_data_by_id(self.indep_id)
    dep_val = packet.get_data_by_id(self.dep_id)
    
    if indep_val is not None and dep_val is not None:
      self.indep_data.append(indep_val)
      self.dep_data.append(dep_val)
      
      indep_cutoff = indep_val - self.indep_span
      
      while self.indep_data[0] < indep_cutoff or self.indep_data[0] > indep_val:
        self.indep_data.popleft()
        self.dep_data.popleft()
        
  def update_show(self):
      self.line.set_xdata(self.indep_data)
      self.line.set_ydata(self.dep_data)
      
      if self.limits is not None: 
        minlim = self.limits[0]
        maxlim = self.limits[1] 
      else:
        if not self.dep_data:
          return
        minlim = min(self.dep_data)
        maxlim = max(self.dep_data)
        if minlim < 0 and maxlim < 0:
          maxlim = 0
        elif minlim > 0 and maxlim > 0:
          minlim = 0
      rangelim = maxlim - minlim
      minlim -= rangelim / 20 # TODO make variable padding
      maxlim += rangelim / 20
      self.subplot.set_ylim(minlim, maxlim)

class WaterfallPlot(BasePlot):
  def __init__(self, subplot, indep_def, dep_def, indep_span):
    super(WaterfallPlot, self).__init__(subplot, indep_def, dep_def, indep_span)
    self.count = dep_def.count
    
    self.x_mesh = [0] * (self.count + 1)
    self.x_mesh = np.array([self.x_mesh])
    
    self.y_array = range(0, self.count + 1)
    self.y_array = list(map(lambda x: x - 0.5, self.y_array))
    self.y_mesh = np.array([self.y_array])
    
    self.data_array = None
    self.indep_data = deque()
    self.quad = None
  
  def update_from_packet(self, packet):
    assert isinstance(packet, DataPacket)
    indep_val = packet.get_data_by_id(self.indep_id)
    dep_val = packet.get_data_by_id(self.dep_id)
    
    if indep_val is not None and dep_val is not None:
      self.x_mesh = np.vstack([self.x_mesh, np.array([[indep_val] * (self.count + 1)])])
      self.y_mesh = np.vstack([self.y_mesh, np.array(self.y_array)])
      if self.data_array is None:
        self.data_array = np.array([dep_val])
      else:
        self.data_array = np.vstack([self.data_array, dep_val])
        
      indep_cutoff = indep_val - self.indep_span
      
      while self.x_mesh[0][0] < indep_cutoff or self.x_mesh[0][0] > indep_val:
        self.x_mesh = np.delete(self.x_mesh, (0), axis=0)
        self.y_mesh = np.delete(self.y_mesh, (0), axis=0)
        self.data_array = np.delete(self.data_array, (0), axis=0)

  def update_show(self):
    if self.quad is not None:
      self.quad.remove()
      del self.quad
    
    if self.limits is not None:
      self.quad = self.subplot.pcolorfast(self.x_mesh, self.y_mesh, self.data_array,
          cmap='gray', vmin=self.limits[0], vmax=self.limits[1],
          interpolation='None')
    else:  
      self.quad = self.subplot.pcolorfast(self.x_mesh, self.y_mesh, self.data_array,
          cmap='gray', interpolation='None')

plot_registry = {}
plot_registry[NumericData] = NumericPlot
plot_registry[NumericArray] = WaterfallPlot


def subplots_from_header(packet, figure, indep_def, indep_span=10000):
  """Instantiate subplots and plots from a received telemetry HeaderPacket.
  The default implementation creates a new plot for each dependent variable,
  but you can customize it to do better things.
  
  Arguments:
  packet -- HeaderPacket object
  figure -- matplotlib figure to draw on 
  indep_name -- internal_name of the independent variable.
  indep_span -- span of the independent variable to display.
  
  Returns: a dict of matplotlib subpolots to list of contained BasePLot objects. 
  """
  assert isinstance(packet, HeaderPacket)
  
  if indep_def is None:
    print("No independent variable")
    return [], []

  data_defs = []
  for _, data_def in reversed(sorted(packet.get_data_defs().items())):
    if data_def != indep_def:
      data_defs.append(data_def)

  plots_dict = {}

  for plot_idx, data_def in enumerate(data_defs):
    ax = figure.add_subplot(len(data_defs), 1, len(data_defs)-plot_idx)
    ax.set_title("%s: %s (%s)"           
        % (data_def.internal_name, data_def.display_name, data_def.units))
    if plot_idx != 0:
      plt.setp(ax.get_xticklabels(), visible=False)
    
    assert data_def.__class__ in plot_registry, "Unable to handle TelemetryData type %s" % data_def.__class__.__name__ 
    plot = plot_registry[data_def.__class__](ax, indep_def, data_def, 
                                             indep_span)
    plots_dict[ax] = [plot]
    
    print("Found dependent data %s" % data_def.internal_name)

  return plots_dict

class CsvLogger(object):
  def __init__(self, name, header_packet):
    csv_header = []
    internal_name_dict = {}
    display_name_dict = {}
    units_dict = {}
        
    for _, data_def in sorted(header_packet.get_data_defs().items()):
      csv_header.append(data_def.data_id)
      internal_name_dict[data_def.data_id] = data_def.internal_name
      display_name_dict[data_def.data_id] = data_def.display_name
      units_dict[data_def.data_id] = data_def.units
    csv_header.append("data")  # for non-telemetry data
                         
    if sys.version_info.major < 3:
      self.csv_file = open(name, 'wb')
    else:
      self.csv_file = open(name, 'w', newline='')
    self.csv_writer = csv.DictWriter(self.csv_file, fieldnames=csv_header)

    self.csv_writer.writerow(internal_name_dict)
    self.csv_writer.writerow(display_name_dict)
    self.csv_writer.writerow(units_dict)
    
    self.pending_data = ""
    
  def add_char(self, data):
    if data == '\n' or data == '\r':
      if self.pending_data:
        self.csv_writer.writerow({'data': self.pending_data})
        self.pending_data = ''
    else:
      self.pending_data += data
    
  def write_data(self, data_packet):
    if self.pending_data:
      self.csv_writer.writerow({'data': self.pending_data})
      self.pending_data = ""
    self.csv_writer.writerow(data_packet.get_data_dict())
        
  def finish(self):
    if self.pending_data:
      self.csv_writer.writerow({'data': self.pending_data})
    self.csv_file.close()
  
if __name__ == "__main__":
  import argparse
  parser = argparse.ArgumentParser(description='Telemetry data plotter.')
  parser.add_argument('port', metavar='p', help='serial port to receive on')
  parser.add_argument('--baud', metavar='b', type=int, default=38400,
                      help='serial baud rate')
  parser.add_argument('--indep_name', metavar='i', default='time',
                      help='internal name of independent axis')
  parser.add_argument('--span', metavar='s', type=int, default=10000,
                      help='independent variable axis span')
  parser.add_argument('--log_prefix', metavar='f', default='telemetry',
                      help='filename prefix for logging output, set to empty to disable logging')
  args = parser.parse_args()
 
  telemetry = TelemetrySerial(serial.Serial(args.port, args.baud))
  
  fig = plt.figure()
  
  # note: mutable elements are in lists to allow access from nested functions
  indep_def = [None]  # note: data ID 0 is invalid
  latest_indep = [0]
  plots_dict = [[]]
  
  csv_logger = [None]
  
  def update(data):
    telemetry.process_rx()
    plot_updated = False
    
    while True:
      packet = telemetry.next_rx_packet()
      if not packet:
        break
      
      if isinstance(packet, HeaderPacket):
        fig.clf()
        
        # get independent variable data ID
        indep_def[0] = None
        for _, data_def in packet.get_data_defs().items():
          if data_def.internal_name == args.indep_name:
            indep_def[0] = data_def
        # TODO warn on missing indep id or duplicates
        
        # instantiate plots
        plots_dict[0] = subplots_from_header(packet, fig, indep_def[0], args.span)
        
        # prepare CSV file and headers
        timestring = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        filename = '%s-%s.csv' %  (args.log_prefix, timestring)
        if csv_logger[0] is not None:
          csv_logger[0].finish()
        if args.log_prefix:
          csv_logger[0] = CsvLogger(filename, packet)
        
      elif isinstance(packet, DataPacket):
        if indep_def[0] is not None:
          indep_value = packet.get_data_by_id(indep_def[0].data_id)
          if indep_value is not None:
            latest_indep[0] = indep_value
            for plot_list in plots_dict[0].values():
              for plot in plot_list:
                plot.update_from_packet(packet)
            plot_updated = True
        
        if csv_logger[0]:
          csv_logger[0].write_data(packet)
        
      else:
        raise Exception("Unknown received packet %s" % repr(packet))
      
    while True:
      next_byte = telemetry.next_rx_byte()
      if next_byte is None:
        break
      try:
        print(chr(next_byte), end='')
        if csv_logger[0]:
          csv_logger[0].add_char(chr(next_byte))
      except UnicodeEncodeError:
        pass

    if plot_updated:
      for plot_list in plots_dict[0].values():
        for plot in plot_list:
          plot.update_show()  
      for subplot in plots_dict[0].keys():
        subplot.set_xlim([latest_indep[0] - args.span, latest_indep[0]])
        
  def set_plot_dialog(plot):
    def set_plot_dialog_inner():
      got = plot.get_dep_def().get_latest_value()
      error = ""
      while True:
        got = simpledialog.askstring("Set remote value",
                                     "Set %s\n%s" % (plot.get_name(), error),
                                     initialvalue=got)
        if got is None:
          break
        try:
          parsed = ast.literal_eval(got)
          telemetry.transmit_set_packet(plot.get_dep_def(), parsed)
          break
        except ValueError as e:
          error = str(e)
        except SyntaxError as e:
          error = str(e)
                  
    return set_plot_dialog_inner
        
  def on_click(event):
    ax = event.inaxes
    if event.dblclick:
      if ax is None or ax not in plots_dict[0]:
        return
      plots_list = plots_dict[0][ax]
      if len(plots_list) > 1:
        menu = Menu(fig.canvas.get_tk_widget(), tearoff=0)
        for plot in plots_list: 
          menu.add_command(label="Set %s" % plot.get_name(),
                           command=set_plot_dialog(plot))
        menu.post(event.guiEvent.x_root, event.guiEvent.y_root)
      elif len(plots_list) == 1:
        set_plot_dialog(plots_list[0])()
      else:
        return
        
  fig.canvas.mpl_connect('button_press_event', on_click)
  ani = animation.FuncAnimation(fig, update, interval=30)
  plt.show()
