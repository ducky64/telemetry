from __future__ import print_function
from collections import deque

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
    self.indep_name = indep_def.internal_name
    self.dep_name = dep_def.internal_name
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
    data_names = packet.get_data_names()
    
    if self.indep_name in data_names and self.dep_name in data_names:
      latest_indep = packet.get_data(self.indep_name)
      self.indep_data.append(latest_indep)
      self.dep_data.append(packet.get_data(self.dep_name))
      
      indep_cutoff = latest_indep - self.indep_span
      
      while self.indep_data[0] < indep_cutoff:
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
    data_names = packet.get_data_names()

    if self.indep_name in data_names and self.dep_name in data_names:
      latest_indep = packet.get_data(self.indep_name)
      self.x_mesh = np.vstack([self.x_mesh, np.array([[latest_indep] * (self.count + 1)])])
      self.y_mesh = np.vstack([self.y_mesh, np.array(self.y_array)])
      if self.data_array is None:
        self.data_array = np.array([packet.get_data(self.dep_name)])
      else:
        self.data_array = np.vstack([self.data_array, packet.get_data(self.dep_name)])
        
      indep_cutoff = latest_indep - self.indep_span
      
      while self.x_mesh[0][0] < indep_cutoff:
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


def create_subplots_from_header(packet, figure, indep_name='time', indep_span=10000):
  """Instantiate subplots and plots from a received telemetry HeaderPacket.
  The default implementation creates a new plot for each dependent variable,
  but you can customize it to do better things.
  
  Arguments:
  packet -- HeaderPacket object
  figure -- matplotlib figure to draw on 
  indep_name -- internal_name of the independent variable.
  indep_span -- span of the independent variable to display.
  
  Returns: two element tuple of a list of matplotlib subplots and list of
    BasePlot objects.
  """
  assert isinstance(packet, HeaderPacket)
  
  indep_def = None
  data_defs = []
  for _, data_def in reversed(sorted(packet.get_data_defs().items())):
    if data_def.internal_name == indep_name:
      indep_def = data_def 
    else:
      data_defs.append(data_def)

  if indep_def is None:
    print("Unable to find independent variable '%s'" % indep_name)
    return [], []

  subplots = []
  plots = []

  for plot_idx, data_def in enumerate(data_defs):
    if data_def.internal_name == indep_name:
      continue

    ax = figure.add_subplot(len(data_defs), 1, len(data_defs)-plot_idx)
    ax.set_title("%s: %s (%s)"           
        % (data_def.internal_name, data_def.display_name, data_def.units))
    if plot_idx != 0:
      plt.setp(ax.get_xticklabels(), visible=False)
    
    print(indep_def)
    print(data_def)
    
    assert data_def.__class__ in plot_registry, "Unable to handle TelemetryData type %s" % data_def.__class__.__name__ 
    plot = plot_registry[data_def.__class__](ax, indep_def, data_def, 
                                             indep_span)
    subplots.append(ax)
    plots.append(plot)
    
    print("Found dependent data %s" % data_def.internal_name)

  return subplots, plots

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
  args = parser.parse_args()
 
  telemetry = TelemetrySerial(serial.Serial(args.port, args.baud))
  
  fig = plt.figure()
  
  # note: mutable elements are in lists to allow access from inner function
  # update
  latest_indep = [0]
  subplots = [[]]
  plots = [[]]
  
  def update(data):
    telemetry.process_rx()
    plot_updated = False
    
    while True:
      packet = telemetry.next_rx_packet()
      if not packet:
        break
      
      if isinstance(packet, HeaderPacket):
        fig.clf()
        
        subplots[0], plots[0] = create_subplots_from_header(packet, fig,
                                                            args.indep_name,
                                                            args.span)

      elif isinstance(packet, DataPacket):
        indep_value = packet.get_data(args.indep_name)
        if indep_value is not None:
          latest_indep[0] = indep_value
          
        for plot in plots[0]:
          plot.update_from_packet(packet)
          
        plot_updated = True
      else:
        raise Exception("Unknown received packet %s" % repr(packet))
      
    while True:
      next_byte = telemetry.next_rx_byte()
      if next_byte is None:
        break
      try:
        print(chr(next_byte), end='')
      except UnicodeEncodeError:
        pass

    if plot_updated:
      for plot in plots[0]:
        plot.update_show()  
      for subplot in subplots[0]:
        subplot.set_xlim([latest_indep[0] - args.span, latest_indep[0]])
        
  ani = animation.FuncAnimation(fig, update, interval=30)
  plt.show()
